// Copyright Onyx2D. All Rights Reserved.

#include "Camera/OnyxCameraComponent.h"
#include "Camera/OnyxCameraBounds.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/World.h"
#include "Feedback/OnyxFeedbackSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	constexpr float CameraYaw = -90.f; // camera at +Y looking toward -Y
	constexpr float OrthographicDistance = 2000.f;

	float GridSnap(float Value, float Grid)
	{
		return Grid > KINDA_SMALL_NUMBER ? FMath::GridSnap(Value, Grid) : Value;
	}
}

UOnyxCameraComponent::UOnyxCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	bConstrainAspectRatio = false;
	// All Onyx framing maths is horizontal (view width = height x aspect). The engine default (Maintain Y FOV)
	// would read the ortho width / field of view as the vertical size: the view would be wider than the confiner,
	// framing and 2D light buffer think. Pin it so every system agrees with what is rendered.
	bOverrideAspectRatioAxisConstraint = true;
	AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
	FieldOfView = Settings.FieldOfView;
}

void UOnyxCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		TraumaHandle = Feedback->OnTrauma.AddUObject(this, &UOnyxCameraComponent::AddTrauma);
	}
	SnapToTarget();
}

void UOnyxCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOnyxFeedbackSubsystem* Feedback = UOnyxFeedbackSubsystem::Get(this))
	{
		Feedback->OnTrauma.Remove(TraumaHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UOnyxCameraComponent::AddTrauma(float Amount)
{
	Shake.AddTrauma(Amount);
}

void UOnyxCameraComponent::PushOverride(UOnyxCameraProfile* InOverride, float BlendTime)
{
	Override = InOverride;
	OverrideBlend = BlendTime;
}

void UOnyxCameraComponent::ClearOverride(float BlendTime)
{
	Override = nullptr;
	OverrideBlend = BlendTime;
}

const FOnyxCameraSettings& UOnyxCameraComponent::GetBaseSettings() const
{
	return Profile ? Profile->Settings : Settings;
}

const AOnyxCameraBounds* UOnyxCameraComponent::FindZone(const FOnyxCameraTarget& Target) const
{
	const UWorld* World = GetWorld();
	const UOnyxWorldSubsystem* Registry = World ? World->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	return Registry
		? Registry->FindCameraBounds(FVector(Target.Position.X, 0.f, Target.Position.Y), [](const AOnyxCameraBounds& Zone) { return Zone.AffectsCamera(); })
		: nullptr;
}

FVector2D UOnyxCameraComponent::GetViewHalfExtents() const
{
	const float HalfHeight = GetEffectiveViewHeight() * 0.5f;
	return FVector2D(HalfHeight * GetAspectRatio(), HalfHeight);
}

FIntPoint UOnyxCameraComponent::GetViewportPixels() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		int32 Width = 0;
		int32 Height = 0;
		Controller->GetViewportSize(Width, Height);
		return FIntPoint(Width, Height);
	}
	return FIntPoint::ZeroValue;
}

float UOnyxCameraComponent::GetEffectiveViewHeight() const
{
	const int32 ScreenHeight = GetViewportPixels().Y;
	if (!Current.bPixelPerfect || ScreenHeight <= 0 || Current.ArtPixelSize <= KINDA_SMALL_NUMBER)
	{
		return Current.ViewHeight;
	}
	// Whole screen pixels per art pixel, as close as possible to the wanted framing.
	const float WantedArtPixels = FMath::Max(Current.ViewHeight / Current.ArtPixelSize, 1.f);
	const int32 Scale = FMath::Max(1, FMath::RoundToInt(ScreenHeight / WantedArtPixels));
	return float(ScreenHeight) / float(Scale) * Current.ArtPixelSize;
}

float UOnyxCameraComponent::GetWorldUnitsPerScreenPixel() const
{
	const int32 ScreenHeight = GetViewportPixels().Y;
	return ScreenHeight > 0 ? GetEffectiveViewHeight() / ScreenHeight : 0.f;
}

float UOnyxCameraComponent::GetAspectRatio() const
{
	const FIntPoint Pixels = GetViewportPixels();
	if (Pixels.X > 0 && Pixels.Y > 0)
	{
		return float(Pixels.X) / float(Pixels.Y);
	}
	return AspectRatio > 0.f ? AspectRatio : 16.f / 9.f;
}

FOnyxCameraTarget UOnyxCameraComponent::GetTarget() const
{
	FOnyxCameraTarget Target;
	if (const AActor* Owner = GetOwner())
	{
		const FVector Location = Owner->GetActorLocation();
		const FVector Velocity = Owner->GetVelocity();
		Target.Position = FVector2D(Location.X, Location.Z);
		Target.Velocity = FVector2D(Velocity.X, Velocity.Z);
		const ACharacter* Character = Cast<ACharacter>(Owner);
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		Target.bGrounded = !Movement || Movement->IsMovingOnGround();
	}
	return Target;
}

void UOnyxCameraComponent::UpdateSettings(float DeltaTime, const AOnyxCameraBounds* Zone)
{
	// Priority: scripted override > zone profile > own profile / settings.
	const UObject* Source = this;
	const FOnyxCameraSettings* Wanted = &GetBaseSettings();
	float BlendTime = DefaultBlendTime;
	if (Override)
	{
		Source = Override;
		Wanted = &Override->Settings;
		BlendTime = OverrideBlend;
	}
	else if (Zone && Zone->Profile)
	{
		Source = Zone;
		Wanted = &Zone->Profile->Settings;
		BlendTime = Zone->BlendTime;
	}

	if (Source != ActiveSource.Get())
	{
		ActiveSource = Source;
		BlendFrom = Current;
		BlendElapsed = 0.f;
		BlendDuration = BlendTime;
	}
	BlendElapsed += DeltaTime;
	const float Alpha = BlendDuration > KINDA_SMALL_NUMBER ? FMath::SmoothStep(0.f, 1.f, BlendElapsed / BlendDuration) : 1.f;
	Current = FOnyxCameraSettings::Blend(BlendFrom, *Wanted, Alpha);
}

void UOnyxCameraComponent::ApplyLens()
{
	// Pixel-perfect needs orthographic: in perspective, layers at different depths get slightly different sizes.
	if (Current.Projection == EOnyxCameraProjection::Orthographic || Current.bPixelPerfect)
	{
		SetProjectionMode(ECameraProjectionMode::Orthographic);
		SetOrthoWidth(GetViewHalfExtents().X * 2.f);
		// Fixed planes: everything from the camera back to the farthest parallax layer is drawn.
		SetAutoCalculateOrthoPlanes(false);
		SetOrthoNearClipPlane(1.f);
		SetOrthoFarClipPlane(OrthographicDistance + 20000.f);
	}
	else
	{
		SetProjectionMode(ECameraProjectionMode::Perspective);
		SetFieldOfView(Current.FieldOfView);
	}
}

namespace
{
	FBox2D ToViewPlane(const FBox& Area, float Padding)
	{
		return Area.IsValid
			? FBox2D(FVector2D(Area.Min.X - Padding, Area.Min.Z - Padding), FVector2D(Area.Max.X + Padding, Area.Max.Z + Padding))
			: FBox2D(ForceInit);
	}
}

FBox2D UOnyxCameraComponent::GetZoneArea(const AOnyxCameraBounds* Zone) const
{
	const bool bUseZone = Current.Confine == EOnyxCameraConfine::Zones || Current.Confine == EOnyxCameraConfine::ZonesThenLevel;
	return bUseZone && Zone && Zone->bConfine ? ToViewPlane(Zone->GetBounds(), Current.ConfinePadding) : FBox2D(ForceInit);
}

FBox2D UOnyxCameraComponent::GetLevelArea() const
{
	const bool bUseLevel = Current.Confine == EOnyxCameraConfine::Level || Current.Confine == EOnyxCameraConfine::ZonesThenLevel;
	const UOnyxWorldSubsystem* Registry = bUseLevel ? GetWorld()->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	return Registry ? ToViewPlane(Registry->GetLevelBounds(), Current.ConfinePadding) : FBox2D(ForceInit);
}

FVector2D UOnyxCameraComponent::Confine(const FVector2D& Framed, const FVector2D& ViewHalf, const AOnyxCameraBounds* Zone, float DeltaTime)
{
	// A zone narrower than the view centres on it, but never shows past the edge of the level.
	const FVector2D Confined = FOnyxCameraConfiner::Clamp(FOnyxCameraConfiner::Clamp(Framed, ViewHalf, GetZoneArea(Zone)), ViewHalf, GetLevelArea());
	const FVector2D Wanted = Confined - Framed;

	if (Zone != ConfineZone.Get())
	{
		ConfineZone = Zone;
		ConfineBlendFrom = ConfineCorrection;
		ConfineBlendElapsed = 0.f;
		ConfineBlendDuration = Zone ? Zone->BlendTime : DefaultBlendTime;
	}
	ConfineBlendElapsed += DeltaTime;
	const float Alpha = ConfineBlendDuration > KINDA_SMALL_NUMBER ? FMath::SmoothStep(0.f, 1.f, ConfineBlendElapsed / ConfineBlendDuration) : 1.f;
	ConfineCorrection = FMath::Lerp(ConfineBlendFrom, Wanted, Alpha);
	return Framed + ConfineCorrection;
}

void UOnyxCameraComponent::SnapToTarget()
{
	const FOnyxCameraTarget Target = GetTarget();
	const AOnyxCameraBounds* Zone = FindZone(Target);

	// Start fully in the settings that apply here, no blend.
	Current = Override ? Override->Settings : (Zone && Zone->Profile ? Zone->Profile->Settings : GetBaseSettings());
	BlendFrom = Current;
	ActiveSource = nullptr;
	UpdateSettings(0.f, Zone);
	BlendDuration = 0.f;
	Current = BlendFrom;

	ApplyLens();
	const FVector2D ViewHalf = GetViewHalfExtents();
	Framing.Reset(Current, Target, ViewHalf);
	ConfineZone = Zone;
	ConfineBlendDuration = 0.f;
	ApplyTransform(Confine(Framing.GetCenter(), ViewHalf, Zone, 0.f), FVector::ZeroVector);
}

void UOnyxCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const FOnyxCameraTarget Target = GetTarget();
	const AOnyxCameraBounds* Zone = FindZone(Target);
	UpdateSettings(DeltaTime, Zone);
	ApplyLens();

	const FVector2D ViewHalf = GetViewHalfExtents();
	const FVector2D Center = Confine(Framing.Update(Current, Target, ViewHalf, DeltaTime), ViewHalf, Zone, DeltaTime);
	ApplyTransform(Center, Shake.Update(Current, DeltaTime));
}

void UOnyxCameraComponent::ApplyTransform(const FVector2D& Center, const FVector& ShakeOffset)
{
	const AActor* Owner = GetOwner();
	const float PlaneY = Owner ? Owner->GetActorLocation().Y : 0.f;
	const bool bOrthographic = Current.Projection == EOnyxCameraProjection::Orthographic || Current.bPixelPerfect;
	const float Distance = bOrthographic
		? OrthographicDistance
		: GetViewHalfExtents().X / FMath::Tan(FMath::DegreesToRadians(FMath::Max(Current.FieldOfView, 1.f) * 0.5f));

	// Pixel-perfect: move one whole screen pixel at a time, and never roll (rotation breaks the pixel grid).
	const float Snap = Current.bPixelPerfect ? GetWorldUnitsPerScreenPixel() : Current.PixelSnap;
	const FVector Location(
		GridSnap(Center.X + ShakeOffset.X, Snap),
		PlaneY + Distance,
		GridSnap(Center.Y + ShakeOffset.Y, Snap));
	SetWorldLocationAndRotation(Location, FRotator(0.f, CameraYaw, Current.bPixelPerfect ? 0.f : ShakeOffset.Z));
}

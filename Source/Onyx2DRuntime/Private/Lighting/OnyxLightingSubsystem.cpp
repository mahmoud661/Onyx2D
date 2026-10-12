// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLightingSubsystem.h"
#include "Camera/OnyxCameraBounds.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/OnyxWorldSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Core/Onyx2DSettings.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Lighting/OnyxLightingEnvironment.h"

void UOnyxLightingSubsystem::RegisterEnvironment(AOnyxLightingEnvironment* InEnvironment)
{
	Environment = InEnvironment;
	ActiveSource = nullptr;
	bHasMood = false;
	if (InEnvironment && LightTarget)
	{
		InEnvironment->SetLightBuffers(LightTarget, PositionTarget);
	}
}

void UOnyxLightingSubsystem::UnregisterEnvironment(AOnyxLightingEnvironment* InEnvironment)
{
	if (Environment.Get() == InEnvironment)
	{
		Environment = nullptr;
	}
}

void UOnyxLightingSubsystem::RegisterLight(UOnyxLight2DComponent* Light)
{
	Lights.AddUnique(Light);
}

void UOnyxLightingSubsystem::UnregisterLight(UOnyxLight2DComponent* Light)
{
	Lights.Remove(Light);
}

void UOnyxLightingSubsystem::SetOccluders(const UObject* Owner, TArray<FOnyxOccluderSegment>&& Segments)
{
	OccluderSets.Add(Owner, MoveTemp(Segments));
	bOccludersDirty = true;
}

void UOnyxLightingSubsystem::ClearOccluders(const UObject* Owner)
{
	bOccludersDirty |= OccluderSets.Remove(Owner) > 0;
}

const TArray<FOnyxOccluderSegment>& UOnyxLightingSubsystem::GetAllOccluders()
{
	for (auto It = OccluderSets.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			bOccludersDirty = true;
		}
	}
	if (bOccludersDirty)
	{
		AllOccluders.Reset();
		for (const TPair<TWeakObjectPtr<const UObject>, TArray<FOnyxOccluderSegment>>& Set : OccluderSets)
		{
			AllOccluders.Append(Set.Value);
		}
		bOccludersDirty = false;
	}
	return AllOccluders;
}

void UOnyxLightingSubsystem::PushOverride(UOnyxLightingProfile* Profile, float BlendTime)
{
	Override = Profile;
	OverrideBlend = BlendTime;
}

UOnyxLightingSubsystem* UOnyxLightingSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UOnyxLightingSubsystem>() : nullptr;
}

void UOnyxLightingSubsystem::Flash(FLinearColor Color, float Intensity, float Duration)
{
	FlashColor = Color;
	FlashIntensity = FMath::Max(Intensity, 0.f);
	FlashDuration = FMath::Max(Duration, 0.01f);
	FlashElapsed = 0.f;
}

void UOnyxLightingSubsystem::ClearOverride(float BlendTime)
{
	Override = nullptr;
	ClearBlend = BlendTime;
}

bool UOnyxLightingSubsystem::GetView(FOnyxLightView& OutView, FVector& OutViewTarget) const
{
	const UWorld* World = GetWorld();
	if (!World->IsGameWorld())
	{
		OutView = EditorView;
		OutViewTarget = EditorView.POV.Location;
		return OutView.IsValid();
	}
	const APlayerController* Controller = World->GetFirstPlayerController();
	if (!Controller || !Controller->PlayerCameraManager || !World->GetGameViewport())
	{
		return false;
	}
	FVector2D Size;
	World->GetGameViewport()->GetViewportSize(Size);
	OutView.POV = Controller->PlayerCameraManager->GetCameraCacheView();
	OutView.ViewportSize = FIntPoint(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y));
	const AActor* Target = Controller->GetViewTarget();
	OutViewTarget = Target ? Target->GetActorLocation() : OutView.POV.Location;
	return OutView.IsValid();
}

const UObject* UOnyxLightingSubsystem::ChooseMood(const FVector& ViewTarget, FOnyxLightingMood& OutMood, float& OutBlendTime) const
{
	if (Override)
	{
		OutMood = Override->Mood;
		OutBlendTime = OverrideBlend;
		return Override;
	}
	const UOnyxWorldSubsystem* Registry = GetWorld()->IsGameWorld() ? GetWorld()->GetSubsystem<UOnyxWorldSubsystem>() : nullptr;
	const AOnyxCameraBounds* Zone = Registry
		? Registry->FindCameraBounds(ViewTarget, [](const AOnyxCameraBounds& Candidate) { return Candidate.Lighting != nullptr; })
		: nullptr;
	if (Zone)
	{
		OutMood = Zone->Lighting->Mood;
		OutBlendTime = Zone->LightingBlendTime;
		return Zone;
	}
	OutMood = Environment->GetBaseMood();
	// Leaving a zone uses that zone's timing; leaving an override uses ClearOverride's.
	const AOnyxCameraBounds* PreviousZone = Cast<AOnyxCameraBounds>(ActiveSource.Get());
	OutBlendTime = PreviousZone ? PreviousZone->LightingBlendTime : ClearBlend;
	return Environment.Get();
}

void UOnyxLightingSubsystem::UpdateMood(float DeltaTime, const FVector& ViewTarget)
{
	FOnyxLightingMood Wanted;
	float BlendTime = 0.f;
	const UObject* Source = ChooseMood(ViewTarget, Wanted, BlendTime);
	if (!bHasMood || !GetWorld()->IsGameWorld())
	{
		// First frame (and the editor): show the right mood at once.
		Current = Wanted;
		BlendFrom = Wanted;
		ActiveSource = Source;
		BlendDuration = 0.f;
		bHasMood = true;
	}
	else if (Source != ActiveSource.Get())
	{
		ActiveSource = Source;
		BlendFrom = Current;
		BlendElapsed = 0.f;
		BlendDuration = BlendTime;
	}
	BlendElapsed += DeltaTime;
	const float Alpha = BlendDuration > KINDA_SMALL_NUMBER ? FMath::SmoothStep(0.f, 1.f, BlendElapsed / BlendDuration) : 1.f;
	Current = FOnyxLightingMood::Blend(BlendFrom, Wanted, Alpha);
}

void UOnyxLightingSubsystem::EnsureTargets(const FIntPoint& ViewportSize)
{
	const int32 Width = FMath::Max(64, FMath::RoundToInt(ViewportSize.X * BufferScale));
	const int32 Height = FMath::Max(64, FMath::RoundToInt(ViewportSize.Y * BufferScale));
	if (!Falloff)
	{
		Falloff = FOnyxLightBufferRenderer::CreateFalloffTexture(this);
	}
	if (LightTarget && LightTarget->SizeX == Width && LightTarget->SizeY == Height)
	{
		return;
	}
	if (LightTarget)
	{
		LightTarget->ResizeTarget(Width, Height);
		PositionTarget->ResizeTarget(Width, Height);
	}
	else
	{
		LightTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Width, Height, RTF_RGBA16f, FLinearColor::Black);
		PositionTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Width, Height, RTF_RGBA16f, FLinearColor::Black);
	}
	if (AOnyxLightingEnvironment* Target = Environment.Get())
	{
		Target->SetLightBuffers(LightTarget, PositionTarget);
	}
}

void UOnyxLightingSubsystem::Tick(float DeltaTime)
{
	AOnyxLightingEnvironment* Target = Environment.Get();
	FOnyxLightView View;
	FVector ViewTarget;
	if (!Target || !GetView(View, ViewTarget))
	{
		return;
	}
	UpdateMood(DeltaTime, ViewTarget);
	FOnyxLightingMood Shown = Current;
	if (FlashElapsed < FlashDuration && FlashIntensity > 0.f)
	{
		// Bright at once, then eases back.
		FlashElapsed += DeltaTime;
		const float Alpha = FMath::Square(1.f - FMath::Clamp(FlashElapsed / FlashDuration, 0.f, 1.f));
		Shown.AmbientColor = FMath::Lerp(Shown.AmbientColor, FlashColor, Alpha * 0.8f);
		Shown.AmbientIntensity += FlashIntensity * Alpha;
		Shown.BackgroundTint = FMath::Lerp(Shown.BackgroundTint, FlashColor * (1.f + FlashIntensity), Alpha * 0.6f);
	}
	Target->ApplyMood(Shown);
	if (UMaterialParameterCollection* Parameters = UOnyx2DSettings::Get()->LightingParameters.LoadSynchronous())
	{
		UKismetMaterialLibrary::SetVectorParameterValue(this, Parameters, TEXT("BackgroundTint"), Shown.BackgroundTint);
	}

	EnsureTargets(View.ViewportSize);
	TArray<const UOnyxLight2DComponent*, TInlineAllocator<64>> Visible;
	for (auto It = Lights.CreateIterator(); It; ++It)
	{
		if (const UOnyxLight2DComponent* Light = It->Get())
		{
			Visible.Add(Light);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
	FOnyxLightBufferRenderer::Render(GetWorld(), View, LightTarget, PositionTarget, Falloff, Visible, GetAllOccluders(), Current.LightStrength);
}

TStatId UOnyxLightingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxLightingSubsystem, STATGROUP_Tickables);
}

bool UOnyxLightingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UOnyxLightingSubsystem::Deinitialize()
{
	Lights.Reset();
	OccluderSets.Reset();
	AllOccluders.Reset();
	Super::Deinitialize();
}

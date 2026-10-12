// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxWater.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Core/Onyx2DSettings.h"
#include "Core/Onyx2DTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Movement/OnyxMovementComponent.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"
#include "World/OnyxFlipbookEffect.h"

namespace
{
	constexpr float WaveStep = 1.f / 60.f;
	constexpr float MaxWaveOffset = 160.f;
	/** The water is drawn just in front of characters, so what is in it looks wet. */
	constexpr float WaterMeshDepth = 12.f;
	/** The area reaches this far above the surface, so things about to splash in are already tracked. */
	constexpr float AreaAboveSurface = 80.f;
	/** The mesh starts this far above the surface; the shader draws the (anti-aliased) edge inside that margin. */
	constexpr float EdgeMargin = 6.f;
	/** Lift / drag sample points across a floating object, per side. */
	constexpr int32 FloatGrid = 3;
	constexpr int32 MaxDroplets = 400;
}

AOnyxWater::AOnyxWater()
{
	PrimaryActorTick.bCanEverTick = true;

	SurfaceRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Surface"));
	RootComponent = SurfaceRoot;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetupAttachment(SurfaceRoot);
	Area->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Area->SetGenerateOverlapEvents(true);
	Area->InitBoxExtent(FVector(400.f, 60.f, 190.f));

	WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetupAttachment(SurfaceRoot);
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterMesh->SetCastShadow(false);
}

// Queries ----------------------------------------------------------------------------------------------------------------

float AOnyxWater::GetSurfaceZ() const
{
	return GetActorLocation().Z;
}

float AOnyxWater::GetLocalX(float WorldX) const
{
	return WorldX - (GetActorLocation().X - Size.X * 0.5f);
}

float AOnyxWater::GetRippleAt(float LocalX) const
{
	// Two slow sine trains in opposite directions: never repeats exactly, always smooth.
	return IdleWaveHeight * (0.7f * FMath::Sin(WaveTime * IdleWaveSpeed * 2.f + LocalX * 0.02f)
		+ 0.3f * FMath::Sin(WaveTime * IdleWaveSpeed * 3.1f - LocalX * 0.035f));
}

float AOnyxWater::GetSurfaceHeightAt(float WorldX) const
{
	const float LocalX = GetLocalX(WorldX);
	float Offset = GetRippleAt(LocalX);
	if (bWaves && Heights.Num() >= 2)
	{
		const float Position = FMath::Clamp(LocalX / Size.X, 0.f, 1.f) * (Heights.Num() - 1);
		const int32 Index = FMath::Min(FMath::FloorToInt(Position), Heights.Num() - 2);
		Offset += FMath::Lerp(Heights[Index], Heights[Index + 1], Position - Index);
	}
	return GetSurfaceZ() + Offset;
}

bool AOnyxWater::IsUnderwater(const FVector& Location) const
{
	const FVector Origin = GetActorLocation();
	return FMath::Abs(Location.X - Origin.X) <= Size.X * 0.5f && Location.Z <= GetSurfaceHeightAt(Location.X) && Location.Z >= Origin.Z - Size.Y;
}

FVector2D AOnyxWater::GetCurrentAt(const FVector& Location) const
{
	if (CurrentVariation <= 0.f)
	{
		return Current;
	}
	const float Phase = WaveTime * 0.6f + Location.X * 0.002f;
	return Current * (1.f + CurrentVariation * FMath::Sin(Phase) * FMath::Cos(Phase * 0.37f));
}

// Waves and droplets -----------------------------------------------------------------------------------------------------

void AOnyxWater::ResetSurface()
{
	const int32 Count = FMath::Clamp(FMath::CeilToInt(Size.X / FMath::Max(WaveResolution, 2.f)) + 1, 2, 4096);
	Heights.Init(0.f, Count);
	Speeds.Init(0.f, Count);
}

void AOnyxWater::Splash(float WorldX, float Strength, float Width)
{
	if (Heights.Num() < 2)
	{
		return;
	}
	const float Spacing = Size.X / (Heights.Num() - 1);
	const float Center = GetLocalX(WorldX) / Spacing;
	const float Radius = FMath::Max(Width * 0.5f / Spacing, 1.f);
	const int32 First = FMath::Max(FMath::FloorToInt(Center - Radius), 0);
	const int32 Last = FMath::Min(FMath::CeilToInt(Center + Radius), Heights.Num() - 1);
	for (int32 Index = First; Index <= Last; ++Index)
	{
		// A smooth dip: strongest in the middle, nothing at the edges.
		const float Falloff = 0.5f + 0.5f * FMath::Cos(FMath::Clamp((Index - Center) / Radius, -1.f, 1.f) * PI);
		Speeds[Index] -= Strength * Falloff;
	}
}

void AOnyxWater::ThrowDroplets(float WorldX, float Speed, int32 Count)
{
	const float Surface = GetSurfaceHeightAt(WorldX);
	for (int32 Index = 0; Index < Count && Droplets.Num() < MaxDroplets; ++Index)
	{
		FDroplet& Drop = Droplets.AddDefaulted_GetRef();
		const float Angle = FMath::DegreesToRadians(FMath::FRandRange(-55.f, 55.f));
		const float Launch = FMath::Min(Speed * FMath::FRandRange(0.25f, 0.65f), 750.f);
		Drop.Position = FVector2D(WorldX + FMath::FRandRange(-20.f, 20.f), Surface);
		Drop.Velocity = FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Launch;
		Drop.Life = FMath::FRandRange(0.45f, 1.1f);
		Drop.Radius = FMath::FRandRange(2.5f, 6.f);
	}
}

void AOnyxWater::SimulateWaves(float DeltaSeconds)
{
	// Fixed steps keep the springs stable whatever the frame rate: each point springs back to rest, pulls on its
	// neighbours (that is what makes waves travel) and loses a little energy.
	StepRemainder = FMath::Min(StepRemainder + DeltaSeconds, WaveStep * 8.f);
	const int32 Count = Heights.Num();
	while (StepRemainder >= WaveStep)
	{
		StepRemainder -= WaveStep;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Left = Heights[FMath::Max(Index - 1, 0)];
			const float Right = Heights[FMath::Min(Index + 1, Count - 1)];
			const float Pull = WaveSpread * WaveStiffness * (Left + Right - 2.f * Heights[Index]);
			Speeds[Index] += (Pull - WaveStiffness * Heights[Index] - WaveDamping * Speeds[Index]) * WaveStep;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Heights[Index] = FMath::Clamp(Heights[Index] + Speeds[Index] * WaveStep, -MaxWaveOffset, MaxWaveOffset);
		}
	}
}

void AOnyxWater::UpdateDroplets(float DeltaSeconds)
{
	const float Gravity = GetWorld() ? GetWorld()->GetGravityZ() : -980.f;
	for (int32 Index = Droplets.Num() - 1; Index >= 0; --Index)
	{
		FDroplet& Drop = Droplets[Index];
		Drop.Velocity.Y += Gravity * DeltaSeconds;
		Drop.Position += Drop.Velocity * DeltaSeconds;
		Drop.Age += DeltaSeconds;
		const bool bBackIn = Drop.Velocity.Y < 0.f && Drop.Position.Y < GetSurfaceHeightAt(Drop.Position.X);
		if (bBackIn)
		{
			Splash(Drop.Position.X, Drop.Radius * 2.f, 12.f); // a tiny ring where it lands
		}
		if (bBackIn || Drop.Age >= Drop.Life)
		{
			Droplets.RemoveAtSwap(Index);
		}
	}
}

// Look -------------------------------------------------------------------------------------------------------------------

void AOnyxWater::EnsureMaterials()
{
	const UOnyx2DSettings* Settings = UOnyx2DSettings::Get();
	if (!WaterMaterial)
	{
		if (UMaterialInterface* Base = Settings->WaterMaterial.LoadSynchronous())
		{
			WaterMaterial = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
	if (!DropMaterial)
	{
		if (UMaterialInterface* Base = Settings->WaterDropMaterial.LoadSynchronous())
		{
			DropMaterial = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
}

void AOnyxWater::UpdateMesh()
{
	const int32 Count = Heights.Num();
	if (Count < 2)
	{
		return;
	}
	// One strip from just above the surface to the bottom. UV.x = world X in metres, UV.y = cm under the surface:
	// the shader draws the gradient, caustics, bubbles and the smooth surface edge from them.
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	Vertices.Reserve(Count * 2);
	UVs.Reserve(Count * 2);
	const float Left = -Size.X * 0.5f;
	const float Step = Size.X / (Count - 1);
	const float WorldLeft = GetActorLocation().X + Left;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float X = Left + Index * Step;
		const float Top = (bWaves ? Heights[Index] : 0.f) + GetRippleAt(Index * Step);
		const float U = (WorldLeft + Index * Step) / 100.f;
		Vertices.Add(FVector(X, WaterMeshDepth, Top + EdgeMargin));
		Vertices.Add(FVector(X, WaterMeshDepth, -Size.Y));
		UVs.Add(FVector2D(U, -EdgeMargin));
		UVs.Add(FVector2D(U, Top + Size.Y));
	}
	if (BuiltColumns == Count && WaterMesh->GetNumSections() > 0)
	{
		WaterMesh->UpdateMeshSection_LinearColor(0, Vertices, {}, UVs, {}, {});
		return;
	}
	TArray<int32> Triangles;
	Triangles.Reserve((Count - 1) * 6);
	for (int32 Index = 0; Index + 1 < Count; ++Index)
	{
		const int32 Top = Index * 2;
		Triangles.Append({ Top, Top + 2, Top + 1, Top + 2, Top + 3, Top + 1 });
	}
	WaterMesh->ClearAllMeshSections();
	WaterMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, UVs, {}, {}, false);	EnsureMaterials();
	WaterMesh->SetMaterial(0, WaterMaterial);
	BuiltColumns = Count;
}

void AOnyxWater::UpdateDropletMesh()
{
	if (Droplets.IsEmpty())
	{
		if (WaterMesh->GetNumSections() > 1)
		{
			WaterMesh->ClearMeshSection(1);
		}
		return;
	}
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;
	Vertices.Reserve(Droplets.Num() * 4);
	UVs.Reserve(Droplets.Num() * 4);
	Colors.Reserve(Droplets.Num() * 4);
	Triangles.Reserve(Droplets.Num() * 6);
	const FVector Origin = GetActorLocation();
	for (const FDroplet& Drop : Droplets)
	{
		const int32 First = Vertices.Num();
		const FVector Center(Drop.Position.X - Origin.X, WaterMeshDepth + 1.f, Drop.Position.Y - Origin.Z);
		// Stretched a little along its motion, like a real drop.
		const float Stretch = 1.f + FMath::Min(Drop.Velocity.Size() / 900.f, 0.8f);
		const FVector2D Direction = Drop.Velocity.GetSafeNormal();
		const FVector2D Along = (Direction.IsNearlyZero() ? FVector2D(0.f, 1.f) : Direction) * Drop.Radius * Stretch;
		const FVector2D Across = FVector2D(-Along.Y, Along.X).GetSafeNormal() * Drop.Radius;
		const FVector A(Along.X, 0.f, Along.Y);
		const FVector B(Across.X, 0.f, Across.Y);
		Vertices.Append({ Center - A - B, Center - A + B, Center + A - B, Center + A + B });
		UVs.Append({ FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f) });
		const FLinearColor Color = SurfaceColor.CopyWithNewOpacity(0.9f * (1.f - Drop.Age / Drop.Life));
		Colors.Append({ Color, Color, Color, Color });
		Triangles.Append({ First, First + 2, First + 1, First + 2, First + 3, First + 1 });
	}
	WaterMesh->CreateMeshSection_LinearColor(1, Vertices, Triangles, {}, UVs, Colors, {}, false);
	EnsureMaterials();
	WaterMesh->SetMaterial(1, DropMaterial);
}

void AOnyxWater::UpdateLook()
{
	EnsureMaterials();
	FLinearColor Tint = FLinearColor::White;
	if (bFollowLighting)
	{
		if (const UOnyxLightingSubsystem* Lighting = UOnyxLightingSubsystem::Get(this))
		{
			// The light a sprite would get from the global light and the sun (2D lights don't reach the water).
			const FOnyxLightingMood Mood = Lighting->GetCurrentMood();
			const FLinearColor Light = Mood.AmbientColor * Mood.AmbientIntensity + Mood.SunColor * Mood.SunIntensity;
			Tint = FLinearColor(FMath::Clamp(Light.R, 0.15f, 1.f), FMath::Clamp(Light.G, 0.15f, 1.f), FMath::Clamp(Light.B, 0.15f, 1.f));
		}
	}
	if (WaterMaterial)
	{
		WaterMaterial->SetVectorParameterValue(TEXT("ShallowColor"), ShallowColor);
		WaterMaterial->SetVectorParameterValue(TEXT("DeepColor"), DeepColor);
		WaterMaterial->SetVectorParameterValue(TEXT("SurfaceColor"), SurfaceColor);
		WaterMaterial->SetScalarParameterValue(TEXT("Depth"), Size.Y);
		WaterMaterial->SetScalarParameterValue(TEXT("Thickness"), SurfaceThickness);
		WaterMaterial->SetScalarParameterValue(TEXT("Caustics"), Caustics);
		WaterMaterial->SetScalarParameterValue(TEXT("Bubbles"), bBubbles ? 1.f : 0.f);
		WaterMaterial->SetVectorParameterValue(TEXT("Tint"), Tint);
	}
	if (DropMaterial)
	{
		DropMaterial->SetVectorParameterValue(TEXT("Tint"), Tint);
	}
}

// Lifecycle --------------------------------------------------------------------------------------------------------------

void AOnyxWater::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// The box hangs from the surface (the actor's location) down to the bottom, plus a margin above.
	Area->SetBoxExtent(FVector(Size.X * 0.5f, 60.f, (Size.Y + AreaAboveSurface) * 0.5f));
	Area->SetRelativeLocation(FVector(0.f, 0.f, (AreaAboveSurface - Size.Y) * 0.5f));
	ResetSurface();
	BuiltColumns = 0;
	UpdateMesh();
	UpdateLook();
}

void AOnyxWater::BeginPlay()
{
	Super::BeginPlay();
	ResetSurface();
	BuiltColumns = 0;
	UpdateMesh();
	UpdateLook();
}

void AOnyxWater::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<UOnyxMovementComponent>& Swimmer : Swimmers)
	{
		if (UOnyxMovementComponent* Movement = Swimmer.Get())
		{
			Movement->ClearWater(this);
		}
	}
	Swimmers.Reset();
	Super::EndPlay(EndPlayReason);
}

void AOnyxWater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WaveTime += DeltaSeconds;
	if (Heights.Num() != FMath::Clamp(FMath::CeilToInt(Size.X / FMath::Max(WaveResolution, 2.f)) + 1, 2, 4096))
	{
		ResetSurface();
	}
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		AffectOverlaps(DeltaSeconds);
	}
	if (bWaves)
	{
		SimulateWaves(DeltaSeconds);
	}
	UpdateDroplets(DeltaSeconds);
	UpdateMesh();
	UpdateDropletMesh();
	UpdateLook();
}

// Floating, swimming, splashes --------------------------------------------------------------------------------------------

void AOnyxWater::CrossedSurface(float WorldX, float Speed, float Width, bool bEntering)
{
	if (Speed < SplashMinSpeed)
	{
		return;
	}
	const float Power = FMath::Clamp(Speed / 800.f, 0.2f, 1.f);
	Splash(WorldX, Speed * SplashStrength * (bEntering ? 1.f : -0.5f), FMath::Max(Width, 20.f));
	ThrowDroplets(WorldX, Speed, FMath::RoundToInt(SplashDroplets * Power));
	const FVector Location(WorldX, GetActorLocation().Y + WaterMeshDepth + 1.f, GetSurfaceHeightAt(WorldX));
	if (SplashEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, SplashEffect, Location, 1.f);
	}
	if (SplashSound)
	{
		UOnyxAudioSubsystem::PlaySoundAt(this, SplashSound, Location, EOnyxSoundCategory::Effects, SplashVolume * FMath::Max(Power, 0.3f), FMath::FRandRange(0.92f, 1.08f));
	}
}

float AOnyxWater::Float(UPrimitiveComponent* Primitive, float DeltaSeconds)
{
	const FTransform Transform = Primitive->GetComponentTransform();
	const FBox Local = Primitive->CalcBounds(FTransform::Identity).GetBox();
	if (!Local.IsValid)
	{
		return 0.f;
	}
	const FVector WorldSize = Local.GetSize() * Transform.GetScale3D().GetAbs();
	const float Liters = WorldSize.X * FMath::Max(WorldSize.Y, 1.f) * WorldSize.Z / 1000.f;
	const float Mass = Primitive->GetMass();
	const float Gravity = FMath::Abs(GetWorld()->GetGravityZ());
	const float Bottom = GetSurfaceZ() - Size.Y;
	const float CellHeight = FMath::Max(WorldSize.Z / FloatGrid, 1.f);
	const float Share = 1.f / (FloatGrid * FloatGrid);

	// Each sample point carries a share of the volume: the wet ones push up (Density in g/cm³) and are slowed where they
	// are, so a tilted object rights itself and a falling one bobs.
	float Wetness = 0.f;
	for (int32 Column = 0; Column < FloatGrid; ++Column)
	{
		for (int32 Row = 0; Row < FloatGrid; ++Row)
		{
			const FVector LocalPoint(FMath::Lerp(Local.Min.X, Local.Max.X, (Column + 0.5f) / FloatGrid), Local.GetCenter().Y,
				FMath::Lerp(Local.Min.Z, Local.Max.Z, (Row + 0.5f) / FloatGrid));
			const FVector Point = Transform.TransformPosition(LocalPoint);
			if (Point.Z < Bottom)
			{
				continue;
			}
			const float Wet = FMath::Clamp((GetSurfaceHeightAt(Point.X) - Point.Z) / CellHeight + 0.5f, 0.f, 1.f);
			if (Wet <= 0.f)
			{
				continue;
			}
			Wetness += Wet * Share;
			Primitive->AddForceAtLocation(FVector(0.f, 0.f, Density * Liters * Share * Wet * Gravity), Point);
			Primitive->AddForceAtLocation(-Primitive->GetPhysicsLinearVelocityAtPoint(Point) * Drag * Mass * Share * Wet, Point);
		}
	}
	if (Wetness > 0.f)
	{
		const FVector2D Flow = GetCurrentAt(Transform.GetLocation());
		Primitive->AddForce(FVector(Flow.X, 0.f, Flow.Y) * Wetness, NAME_None, true);
		Primitive->SetPhysicsAngularVelocityInRadians(Primitive->GetPhysicsAngularVelocityInRadians() * FMath::Exp(-SpinDrag * Wetness * DeltaSeconds));
		if (Steadiness > 0.f)
		{
			// Turn towards the nearest flat side (a real square block floats on a corner, game crates float flat).
			// Spinning about +Y lowers this angle (right-hand rule), so the push follows the error's sign.
			const FVector Forward = Transform.GetUnitAxis(EAxis::X);
			const float Angle = FMath::Atan2(Forward.Z, Forward.X);
			const float Error = Angle - FMath::RoundToFloat(Angle / UE_HALF_PI) * UE_HALF_PI;
			Primitive->AddTorqueInRadians(FVector(0.f, Error * Steadiness * 25.f * Wetness, 0.f), NAME_None, true);
		}
	}
	return Wetness;
}

void AOnyxWater::AffectOverlaps(float DeltaSeconds)
{
	// Everything movable inside the area, whether or not it generates overlap events (physics cubes don't by default).
	TArray<FOverlapResult> Results;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxWater), false, this);
	GetWorld()->OverlapMultiByObjectType(Results, Area->GetComponentLocation(), Area->GetComponentQuat(), FCollisionObjectQueryParams::AllDynamicObjects,
		FCollisionShape::MakeBox(Area->GetScaledBoxExtent()), Params);

	TSet<TWeakObjectPtr<UOnyxMovementComponent>> StillIn;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> Seen;
	for (const FOverlapResult& Result : Results)
	{
		UPrimitiveComponent* Primitive = Result.GetComponent();
		if (!Primitive || Primitive->GetOwner() == this || Seen.Contains(Primitive))
		{
			continue;
		}
		Seen.Add(Primitive);
		const FBox Box = Primitive->Bounds.GetBox();
		const FVector Center = Box.GetCenter();
		const float Width = Box.GetSize().X;
		const float Surface = GetSurfaceHeightAt(Center.X);

		// Characters swim (their movement component does the rest); their capsule makes the splashes and the wake.
		const ACharacter* Character = Cast<ACharacter>(Primitive->GetOwner());
		UOnyxMovementComponent* Movement = Character && Primitive == Character->GetRootComponent() ? Cast<UOnyxMovementComponent>(Character->GetCharacterMovement()) : nullptr;
		FVector Velocity = FVector::ZeroVector;
		if (Movement)
		{
			Movement->SetWater(this);
			StillIn.Add(Movement);
			Velocity = Movement->Velocity;
			if (FMath::Abs(Center.Z - Surface) < Box.GetExtent().Z)
			{
				Splash(Center.X, FMath::Abs(Velocity.X) * DeltaSeconds * 0.5f, Width * 1.5f); // a wake while swimming along the top
			}
		}
		else if (Primitive->IsSimulatingPhysics())
		{
			const float Wetness = Float(Primitive, DeltaSeconds);
			Velocity = Primitive->GetPhysicsLinearVelocity();
			if (Wetness > 0.02f && Wetness < 0.98f)
			{
				Splash(Center.X, -Velocity.Z * DeltaSeconds * 2.f, Width); // bobbing objects keep the surface moving
			}
		}
		else
		{
			continue;
		}

		// Splash when the centre crosses the surface.
		if (const float* Last = LastCenters.Find(Primitive))
		{
			const bool bWasAbove = *Last > Surface;
			if (bWasAbove != (Center.Z > Surface))
			{
				CrossedSurface(Center.X, FMath::Abs(Velocity.Z), Width, bWasAbove);
			}
		}
		LastCenters.Add(Primitive, Center.Z);
	}

	// Characters that left the water.
	for (const TWeakObjectPtr<UOnyxMovementComponent>& Swimmer : Swimmers)
	{
		if (UOnyxMovementComponent* Movement = Swimmer.Get(); Movement && !StillIn.Contains(Swimmer))
		{
			Movement->ClearWater(this);
		}
	}
	Swimmers = MoveTemp(StillIn);
	for (auto It = LastCenters.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() || !Seen.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
}

// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxWater.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Core/Onyx2DLibrary.h"
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
	constexpr int32 MaxBubbles = 300;
	constexpr int32 MaxEffects = 400;
	/** From above, the water lies on the ground: in front of the tiles, behind everything standing on it (the draw order). */
	constexpr float AboveMeshDepth = -11.f;
	/** From above, the mesh is a grid of this many cm, so the distance to a rounded shore follows the corners. */
	constexpr float AboveCell = 16.f;
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

bool AOnyxWater::IsSeenFromAbove() const
{
	return UOnyx2DLibrary::IsTopDown(this);
}

float AOnyxWater::GetDepthFromAbove(const FVector& Location) const
{
	// A rounded rectangle on the ground: how far in from its edge (its signed distance, turned round).
	const FVector Origin = GetActorLocation();
	const FVector2D Half(Size.X * 0.5f, Size.Y * 0.5f);
	const float Radius = FMath::Clamp(CornerRadius, 0.f, FMath::Min(Half.X, Half.Y));
	const FVector2D Local(FMath::Abs(Location.X - Origin.X), FMath::Abs(Location.Z - (Origin.Z - Half.Y)));
	const FVector2D Q = Local - Half + FVector2D(Radius, Radius);
	const float Outside = FVector2D(FMath::Max(Q.X, 0.f), FMath::Max(Q.Y, 0.f)).Size();
	const float Inside = FMath::Min(FMath::Max(Q.X, Q.Y), 0.f);
	return Radius - Outside - Inside;
}

void AOnyxWater::SetLevel(float SurfaceZ, float Speed)
{
	TargetLevel = FMath::Max(SurfaceZ, GetBottomZ() + 1.f);
	LevelSpeed = FMath::Max(Speed, 0.f);
	if (LevelSpeed <= 0.f)
	{
		ChangeLevel(TargetLevel - GetSurfaceZ());
	}
}

void AOnyxWater::ChangeLevel(float Delta)
{
	// The bottom stays: the surface (the actor) moves and the depth with it.
	Delta = FMath::Max(Delta, 1.f - Size.Y);
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}
	Size.Y += Delta;
	AddActorWorldOffset(FVector(0.f, 0.f, Delta));
	UpdateArea();
}

void AOnyxWater::Widen(float Amount)
{
	if (FMath::IsNearlyZero(Amount))
	{
		return;
	}
	// Grows (or shrinks) on one side: the middle moves half as far that way.
	const float Grow = FMath::Max(Size.X + FMath::Abs(Amount), 10.f) - Size.X;
	Size.X += Grow;
	AddActorWorldOffset(FVector(FMath::Sign(Amount) * Grow * 0.5f, 0.f, 0.f));
	ResetSurface();
	BuiltColumns = 0;
	UpdateArea();
}

void AOnyxWater::UpdateArea()
{
	if (bBuiltFromAbove)
	{
		Area->SetBoxExtent(FVector(Size.X * 0.5f, 60.f, Size.Y * 0.5f));
		Area->SetRelativeLocation(FVector(0.f, 0.f, -Size.Y * 0.5f));
		return;
	}
	Area->SetBoxExtent(FVector(Size.X * 0.5f, 60.f, (Size.Y + AreaAboveSurface) * 0.5f));
	Area->SetRelativeLocation(FVector(0.f, 0.f, (AreaAboveSurface - Size.Y) * 0.5f));
}

void AOnyxWater::UpdateLevel(float DeltaSeconds)
{
	if (LevelSpeed <= 0.f)
	{
		return;
	}
	const float Gap = TargetLevel - GetSurfaceZ();
	const float Step = FMath::Sign(Gap) * FMath::Min(FMath::Abs(Gap), LevelSpeed * DeltaSeconds);
	ChangeLevel(Step);
	if (FMath::Abs(TargetLevel - GetSurfaceZ()) < 0.5f)
	{
		LevelSpeed = 0.f;
	}
}

void AOnyxWater::UpdateFlowLines(float DeltaSeconds)
{
	const float Strength = Current.Size();
	if (FlowLines <= 0.f || Strength < 20.f || Size.Y < 8.f)
	{
		return;
	}
	// How many a second: more for a wider, faster river.
	const FVector2D Direction = Current.GetSafeNormal();
	const float Drift = FMath::Clamp(Strength * 0.55f, 60.f, 520.f);
	FlowLineDebt += DeltaSeconds * FlowLines * Size.X / 100.f * FMath::Clamp(Strength / 300.f, 0.4f, 2.f) * 1.6f;
	const FVector Origin = GetActorLocation();
	while (FlowLineDebt >= 1.f)
	{
		FlowLineDebt -= 1.f;
		const float X = Origin.X + FMath::FRandRange(-0.5f, 0.5f) * Size.X;
		if (FMath::FRand() < 0.35f)
		{
			// A fleck of foam carried along the top.
			const float Fleck = FMath::FRandRange(4.f, 8.f);
			if (FEffect* Foam = AddEffect(EEffectKind::Foam, FVector2D(X, GetSurfaceHeightAt(X)), FMath::FRandRange(1.2f, 2.2f), Fleck, Fleck * 1.4f, FoamColor.A * 0.7f))
			{
				Foam->Velocity = Direction * Drift;
			}
			continue;
		}
		// A streak under the surface, thicker near the top where the water runs fastest.
		const float Depth = FMath::Square(FMath::FRand()) * FMath::Min(Size.Y - 6.f, 220.f) + 6.f;
		if (FEffect* Streak = AddEffect(EEffectKind::Streak, FVector2D(X, GetSurfaceZ() - Depth), FMath::FRandRange(0.6f, 1.1f), 1.6f, 1.6f, 0.35f))
		{
			Streak->Direction = Direction;
			Streak->Length = FMath::FRandRange(18.f, 42.f);
			Streak->Velocity = Direction * Drift * FMath::Lerp(1.f, 0.6f, Depth / FMath::Max(Size.Y, 1.f));
		}
	}
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

void AOnyxWater::EmitBubbles(const FVector& Location, int32 Count, float Spread)
{
	if (bBuiltFromAbove)
	{
		return; // from above nothing is under the surface to see
	}
	const FVector Origin = GetActorLocation();
	for (int32 Index = 0; Index < Count && Bubbles.Num() < MaxBubbles; ++Index)
	{
		const FVector2D Offset = FMath::RandPointInCircle(FMath::Max(Spread, 0.f));
		const FVector2D Position(Location.X + Offset.X, Location.Z + Offset.Y);
		if (FMath::Abs(Position.X - Origin.X) > Size.X * 0.5f || Position.Y > GetSurfaceHeightAt(Position.X) || Position.Y < Origin.Z - Size.Y)
		{
			continue; // only under the surface, inside the water
		}
		FBubble& Bubble = Bubbles.AddDefaulted_GetRef();
		Bubble.Position = Position;
		Bubble.Radius = FMath::FRandRange(1.5f, 4.5f);
		Bubble.RiseSpeed = FMath::FRandRange(70.f, 120.f) + Bubble.Radius * 8.f; // bigger bubbles rise faster
		Bubble.Phase = FMath::FRandRange(0.f, UE_TWO_PI);
		Bubble.Life = FMath::FRandRange(3.f, 5.f);
	}
}

void AOnyxWater::UpdateBubbles(float DeltaSeconds)
{
	const float HalfWidth = Size.X * 0.5f;
	const float CenterX = GetActorLocation().X;
	for (int32 Index = Bubbles.Num() - 1; Index >= 0; --Index)
	{
		FBubble& Bubble = Bubbles[Index];
		Bubble.Age += DeltaSeconds;
		// Speeding up from rest, wobbling side to side, carried by the current.
		const FVector2D Flow = GetCurrentAt(FVector(Bubble.Position.X, 0.f, Bubble.Position.Y));
		Bubble.Position.Y += Bubble.RiseSpeed * FMath::Min(Bubble.Age / 0.25f, 1.f) * DeltaSeconds;
		Bubble.Position.X += (FMath::Sin(Bubble.Age * 7.f + Bubble.Phase) * 20.f + Flow.X * 0.2f) * DeltaSeconds;
		Bubble.Position.X = FMath::Clamp(Bubble.Position.X, CenterX - HalfWidth, CenterX + HalfWidth);
		const bool bPopped = Bubble.Position.Y >= GetSurfaceHeightAt(Bubble.Position.X) - Bubble.Radius;
		if (bPopped)
		{
			Splash(Bubble.Position.X, -Bubble.Radius * 0.5f, 8.f); // a tiny ring where it pops
		}
		if (bPopped || Bubble.Age >= Bubble.Life)
		{
			Bubbles.RemoveAtSwap(Index);
		}
	}
}

// Effects ----------------------------------------------------------------------------------------------------------------

AOnyxWater::FEffect* AOnyxWater::AddEffect(EEffectKind Kind, const FVector2D& Position, float Life, float StartSize, float EndSize, float Alpha)
{
	if (Effects.Num() >= MaxEffects)
	{
		return nullptr;
	}
	FEffect& Effect = Effects.AddDefaulted_GetRef();
	Effect.Kind = Kind;
	Effect.Position = Position;
	Effect.Life = FMath::Max(Life, 0.05f);
	Effect.StartSize = StartSize;
	Effect.EndSize = EndSize;
	Effect.Alpha = Alpha;
	Effect.Seed = FMath::FRand();
	return &Effect;
}

void AOnyxWater::EmitRing(const FVector& Location, float Radius, float Strength)
{
	if (Strength > 0.f)
	{
		AddEffect(EEffectKind::Ring, FVector2D(Location.X, Location.Z), 0.45f + 0.2f * FMath::Min(Strength, 1.f), Radius * 0.15f, Radius, 0.55f * FMath::Min(Strength, 1.f));
	}
}

void AOnyxWater::EmitFoam(float WorldX, float Width, float Strength)
{
	// Patches over the width, pushed outwards from the middle, bigger with a stronger splash.
	const int32 Count = FMath::Clamp(FMath::RoundToInt(3.f + 6.f * Strength), 1, 12);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Offset = FMath::FRandRange(-0.5f, 0.5f) * FMath::Max(Width, 10.f);
		const float PatchSize = FMath::FRandRange(8.f, 16.f) * FMath::Clamp(0.6f + Strength * 0.6f, 0.6f, 1.5f);
		if (FEffect* Foam = AddEffect(EEffectKind::Foam, FVector2D(WorldX + Offset, GetSurfaceHeightAt(WorldX + Offset)), FMath::FRandRange(0.9f, 1.8f), PatchSize, PatchSize * 2.2f, FoamColor.A))
		{
			Foam->Velocity = FVector2D(FMath::Sign(Offset + KINDA_SMALL_NUMBER) * FMath::FRandRange(30.f, 110.f) * FMath::Min(Strength, 1.5f), 0.f);
		}
	}
}

void AOnyxWater::EmitSilt(const FVector& Location, float Strength)
{
	const int32 Count = FMath::Clamp(FMath::RoundToInt(2.f + 4.f * Strength), 1, 8);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float CloudSize = FMath::FRandRange(10.f, 18.f);
		if (FEffect* Silt = AddEffect(EEffectKind::Silt, FVector2D(Location.X + FMath::FRandRange(-12.f, 12.f), Location.Z + FMath::FRandRange(0.f, 6.f)),
			FMath::FRandRange(1.2f, 2.2f), CloudSize, CloudSize * 2.5f, SiltColor.A))
		{
			// Rolling out to the sides and up a little.
			Silt->Velocity = FVector2D(FMath::FRandRange(-1.f, 1.f) * 90.f * Strength, FMath::FRandRange(15.f, 45.f) * Strength);
		}
	}
}

void AOnyxWater::UpdateEffects(float DeltaSeconds)
{
	const float HalfWidth = Size.X * 0.5f;
	const float CenterX = GetActorLocation().X;
	const float Slowing = FMath::Exp(-2.5f * DeltaSeconds); // the water soon stops everything it carries...
	const float Carried = FMath::Exp(-0.15f * DeltaSeconds); // ...except what its current carries along
	for (int32 Index = Effects.Num() - 1; Index >= 0; --Index)
	{
		FEffect& Effect = Effects[Index];
		Effect.Age += DeltaSeconds;
		if (Effect.Age >= Effect.Life)
		{
			Effects.RemoveAtSwap(Index);
			continue;
		}
		const bool bCarried = !Current.IsNearlyZero() && FVector2D::DotProduct(Effect.Velocity, Current) > 0.f && FlowLines > 0.f
			&& (Effect.Kind == EEffectKind::Streak || Effect.Kind == EEffectKind::Foam);
		Effect.Velocity *= bCarried ? Carried : Slowing;
		Effect.Position += Effect.Velocity * DeltaSeconds;
		Effect.Position.X = FMath::Clamp(Effect.Position.X, CenterX - HalfWidth, CenterX + HalfWidth);
		if (Effect.Kind == EEffectKind::Foam && !bBuiltFromAbove)
		{
			Effect.Position.Y = GetSurfaceHeightAt(Effect.Position.X); // rides the waves
		}
	}
}

void AOnyxWater::LeaveWake(const FBox& Bounds, const FVector& Velocity, float DeltaSeconds)
{
	if (!bWakes || WakeAmount <= 0.f)
	{
		return;
	}
	const FVector2D Motion(Velocity.X, Velocity.Z);
	const float Speed = Motion.Size();
	if (Speed < 60.f)
	{
		return;
	}
	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	const float Surface = GetSurfaceHeightAt(Center.X);
	const FVector2D Ahead = Motion / Speed;
	const FVector2D Side(-Ahead.Y, Ahead.X);
	const FVector2D Back(Center.X - Ahead.X * Extent.X, Center.Z - Ahead.Y * Extent.Z);
	auto Chance = [DeltaSeconds, this](float PerSecond) { return FMath::FRand() < PerSecond * WakeAmount * DeltaSeconds; };

	if (Center.Z < Surface - Extent.Z * 0.5f)
	{
		// Under water: bubbles shed from the back, more the faster it goes...
		if (Speed > 120.f && Chance(Speed / 60.f))
		{
			EmitBubbles(FVector(Back.X, 0.f, Back.Y), 1, Extent.X * 0.5f);
		}
		// ...and when fast (dives, strokes, dashes), soft streaks of churned water trailing behind, curling to the sides.
		if (Speed > 260.f && Chance(Speed / 40.f))
		{
			const FVector2D Start = Back + Side * FMath::FRandRange(-0.8f, 0.8f) * FMath::Max(Extent.X, Extent.Z);
			if (FEffect* Streak = AddEffect(EEffectKind::Streak, Start, FMath::FRandRange(0.35f, 0.6f), FMath::FRandRange(5.f, 9.f), 0.f, 0.32f))
			{
				Streak->Direction = (Ahead + Side * FMath::FRandRange(-0.35f, 0.35f)).GetSafeNormal();
				Streak->Length = FMath::Clamp(Speed * 0.12f, 25.f, 130.f);
				Streak->Velocity = Motion * 0.3f + Side * FMath::FRandRange(-40.f, 40.f);
			}
		}
	}
	else if (FMath::Abs(Center.Z - Surface) <= Extent.Z && FMath::Abs(Velocity.X) > 90.f)
	{
		// Along the surface: foam left behind, and a splash from the paddling now and then.
		if (Chance(FMath::Abs(Velocity.X) / 45.f))
		{
			EmitFoam(Center.X - FMath::Sign(Velocity.X) * Extent.X, Extent.X, 0.3f);
		}
		if (Chance(FMath::Abs(Velocity.X) / 160.f))
		{
			ThrowDroplets(Center.X - FMath::Sign(Velocity.X) * Extent.X * 0.5f, FMath::Abs(Velocity.X) * 0.9f, FMath::RandRange(1, 3));
		}
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
		if (bBuiltFromAbove)
		{
			Drop.Velocity *= FMath::Exp(-4.f * DeltaSeconds); // thrown out across the water, slowing, fading
		}
		else
		{
			Drop.Velocity.Y += Gravity * DeltaSeconds;
		}
		Drop.Position += Drop.Velocity * DeltaSeconds;
		Drop.Age += DeltaSeconds;
		const bool bBackIn = !bBuiltFromAbove && Drop.Velocity.Y < 0.f && Drop.Position.Y < GetSurfaceHeightAt(Drop.Position.X);
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
	if (Count < 2 || bBuiltFromAbove)
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
		WaterMesh->SetMeshSectionVisible(0, bDrawWater);
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
	WaterMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, UVs, {}, {}, false);
	EnsureMaterials();
	WaterMesh->SetMaterial(0, WaterMaterial);
	WaterMesh->SetMeshSectionVisible(0, bDrawWater);
	BuiltColumns = Count;
}

void AOnyxWater::UpdateParticleMesh()
{
	if (Droplets.IsEmpty() && Bubbles.IsEmpty() && Effects.IsEmpty())
	{
		if (WaterMesh->GetNumSections() > 1)
		{
			WaterMesh->ClearMeshSection(1);
		}
		return;
	}
	const int32 Count = Droplets.Num() + Bubbles.Num() + Effects.Num();
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FVector2D> Kinds; // UV1: x = the shape (0 droplet, 1 bubble, then EEffectKind), y = a seed that varies it
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;
	Vertices.Reserve(Count * 4);
	UVs.Reserve(Count * 4);
	Kinds.Reserve(Count * 4);
	Colors.Reserve(Count * 4);
	Triangles.Reserve(Count * 6);
	const FVector Origin = GetActorLocation();
	const float Depth = (bBuiltFromAbove ? AboveMeshDepth : WaterMeshDepth) + 1.f;
	auto AddQuad = [&](const FVector2D& Position, const FVector2D& Along, const FVector2D& Across, const FLinearColor& Color, float Kind, float Seed = 0.f)
	{
		const int32 First = Vertices.Num();
		const FVector Center(Position.X - Origin.X, Depth, Position.Y - Origin.Z);
		const FVector A(Along.X, 0.f, Along.Y);
		const FVector B(Across.X, 0.f, Across.Y);
		Vertices.Append({ Center - A - B, Center - A + B, Center + A - B, Center + A + B });
		UVs.Append({ FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(0.f, 0.f), FVector2D(1.f, 0.f) });
		const FVector2D Shape(Kind, Seed);
		Kinds.Append({ Shape, Shape, Shape, Shape });
		Colors.Append({ Color, Color, Color, Color });
		Triangles.Append({ First, First + 2, First + 1, First + 2, First + 3, First + 1 });
	};
	for (const FDroplet& Drop : Droplets)
	{
		// Stretched a little along its motion, like a real drop.
		const float Stretch = 1.f + FMath::Min(Drop.Velocity.Size() / 900.f, 0.8f);
		const FVector2D Direction = Drop.Velocity.GetSafeNormal();
		const FVector2D Along = (Direction.IsNearlyZero() ? FVector2D(0.f, 1.f) : Direction) * Drop.Radius * Stretch;
		AddQuad(Drop.Position, Along, FVector2D(-Along.Y, Along.X).GetSafeNormal() * Drop.Radius, SurfaceColor.CopyWithNewOpacity(0.9f * (1.f - Drop.Age / Drop.Life)), 0.f);
	}
	for (const FBubble& Bubble : Bubbles)
	{
		// Upright (the highlight stays at the top left), fading in and out.
		const float Fade = FMath::Min(Bubble.Age / 0.15f, 1.f) * FMath::Clamp((Bubble.Life - Bubble.Age) / 0.5f, 0.f, 1.f);
		AddQuad(Bubble.Position, FVector2D(0.f, Bubble.Radius), FVector2D(Bubble.Radius, 0.f), SurfaceColor.CopyWithNewOpacity(0.85f * Fade), 1.f);
	}
	for (const FEffect& Effect : Effects)
	{
		// Growing quickly at first then easing off, fading over its life.
		const float T = Effect.Age / Effect.Life;
		const float Radius = FMath::Lerp(Effect.StartSize, Effect.EndSize, 1.f - FMath::Square(1.f - T));
		const float FadeIn = FMath::Min(Effect.Age / 0.08f, 1.f);
		FLinearColor Color = SurfaceColor;
		FVector2D Along(0.f, Radius);
		FVector2D Across(Radius, 0.f);
		float Fade = FadeIn * (1.f - T);
		switch (Effect.Kind)
		{
		case EEffectKind::Ring:
			Fade = FMath::Pow(1.f - T, 1.5f);
			break;
		case EEffectKind::Foam:
			Color = FoamColor;
			Along.Y = Radius * 0.45f; // a flat patch lying on the surface
			Fade = FadeIn * FMath::Clamp((1.f - T) / 0.5f, 0.f, 1.f);
			break;
		case EEffectKind::Streak:
			// Thin and long, pointing the way the water was pushed; it shortens as it fades.
			Along = Effect.Direction * Effect.Length * 0.5f * FMath::Lerp(1.f, 0.6f, T);
			Across = FVector2D(-Effect.Direction.Y, Effect.Direction.X) * Effect.StartSize * 0.5f;
			break;
		case EEffectKind::Silt:
			Color = SiltColor;
			Along.Y = Radius * 0.7f;
			Fade = FadeIn * FMath::Clamp((1.f - T) / 0.6f, 0.f, 1.f);
			break;
		}
		// Rings stay in the water: their seed is where the surface cuts across them (0 = top of the quad, 1 = bottom); from
		// above the whole ring lies on the water.
		const float Seed = Effect.Kind == EEffectKind::Ring
			? (bBuiltFromAbove ? 0.f : FMath::Clamp(0.5f - (GetSurfaceHeightAt(Effect.Position.X) - Effect.Position.Y) / (2.f * FMath::Max(Radius, 1.f)), 0.f, 1.f))
			: Effect.Seed;
		AddQuad(Effect.Position, Along, Across, Color.CopyWithNewOpacity(Effect.Alpha * Fade), float(uint8(Effect.Kind)), Seed);
	}
	WaterMesh->CreateMeshSection_LinearColor(1, Vertices, Triangles, {}, UVs, Kinds, {}, {}, Colors, {}, false);
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
		WaterMaterial->SetScalarParameterValue(TEXT("Depth"), bBuiltFromAbove ? DeepFromShore : Size.Y);
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

void AOnyxWater::BuildShape()
{
	bBuiltFromAbove = IsSeenFromAbove();
	if (bBuiltFromAbove)
	{
		// From above the box is the pond on the ground, from its top edge (the actor's location) down.
		Area->SetBoxExtent(FVector(Size.X * 0.5f, 60.f, Size.Y * 0.5f));
		Area->SetRelativeLocation(FVector(0.f, 0.f, -Size.Y * 0.5f));
		Droplets.Reset();
		Bubbles.Reset();
		UpdateMeshFromAbove();
	}
	else
	{
		// The box hangs from the surface (the actor's location) down to the bottom, plus a margin above.
		Area->SetBoxExtent(FVector(Size.X * 0.5f, 60.f, (Size.Y + AreaAboveSurface) * 0.5f));
		Area->SetRelativeLocation(FVector(0.f, 0.f, (AreaAboveSurface - Size.Y) * 0.5f));
		ResetSurface();
		BuiltColumns = 0;
		UpdateMesh();
	}
	UpdateLook();
}

void AOnyxWater::UpdateMeshFromAbove()
{
	// A grid over the box; UV0 = world X and Z in metres (caustics), UV1 = cm in from the shore and 1 (seen from above).
	const int32 Columns = FMath::Clamp(FMath::CeilToInt(Size.X / AboveCell), 2, 160);
	const int32 Rows = FMath::Clamp(FMath::CeilToInt(Size.Y / AboveCell), 2, 160);
	const FVector Origin = GetActorLocation();
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FVector2D> Shore;
	TArray<int32> Triangles;
	Vertices.Reserve((Columns + 1) * (Rows + 1));
	UVs.Reserve((Columns + 1) * (Rows + 1));
	Shore.Reserve((Columns + 1) * (Rows + 1));
	Triangles.Reserve(Columns * Rows * 6);
	for (int32 Row = 0; Row <= Rows; ++Row)
	{
		for (int32 Column = 0; Column <= Columns; ++Column)
		{
			const float X = -Size.X * 0.5f + Size.X * Column / Columns;
			const float Z = -Size.Y * Row / Rows;
			Vertices.Add(FVector(X, AboveMeshDepth, Z));
			UVs.Add(FVector2D((Origin.X + X) / 100.f, (Origin.Z + Z) / 100.f));
			Shore.Add(FVector2D(GetDepthFromAbove(Origin + FVector(X, 0.f, Z)), 1.f));
		}
	}
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const int32 TopLeft = Row * (Columns + 1) + Column;
			const int32 BottomLeft = TopLeft + Columns + 1;
			Triangles.Append({ TopLeft, TopLeft + 1, BottomLeft, TopLeft + 1, BottomLeft + 1, BottomLeft });
		}
	}
	WaterMesh->ClearAllMeshSections();
	WaterMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, UVs, Shore, {}, {}, {}, {}, false);
	EnsureMaterials();
	WaterMesh->SetMaterial(0, WaterMaterial);
	BuiltColumns = 0;
}

void AOnyxWater::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildShape();
}

void AOnyxWater::BeginPlay()
{
	Super::BeginPlay();
	BuildShape();
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
	if (bBuiltFromAbove != IsSeenFromAbove())
	{
		BuildShape(); // the level's game style changed
	}
	if (bBuiltFromAbove)
	{
		const UWorld* World = GetWorld();
		if (World && World->IsGameWorld())
		{
			AffectFromAbove(DeltaSeconds);
		}
		UpdateDroplets(DeltaSeconds);
		UpdateEffects(DeltaSeconds);
		UpdateParticleMesh();
		UpdateLook();
		return;
	}
	if (Heights.Num() != FMath::Clamp(FMath::CeilToInt(Size.X / FMath::Max(WaveResolution, 2.f)) + 1, 2, 4096))
	{
		ResetSurface();
	}
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		UpdateLevel(DeltaSeconds);
		AffectOverlaps(DeltaSeconds);
		UpdateFlowLines(DeltaSeconds);
	}
	if (bWaves)
	{
		SimulateWaves(DeltaSeconds);
	}
	UpdateDroplets(DeltaSeconds);
	UpdateBubbles(DeltaSeconds);
	UpdateEffects(DeltaSeconds);
	UpdateMesh();
	UpdateParticleMesh();
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
	if (bWakes)
	{
		// Foam spreading on the surface; going in, a ring pushed through the water under the splash.
		EmitFoam(WorldX, FMath::Max(Width, 20.f) * 1.5f, Power * (bEntering ? 1.f : 0.6f));
		if (bEntering)
		{
			EmitRing(FVector(WorldX, 0.f, GetSurfaceHeightAt(WorldX) - 25.f), 50.f + 70.f * Power, Power);
		}
	}
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

void AOnyxWater::SplashFromAbove(const FVector& Location, float Strength)
{
	const float Power = FMath::Clamp(Strength, 0.2f, 1.f);
	if (bWakes)
	{
		EmitRing(Location, 50.f + 70.f * Power, Power);
		EmitRing(Location, 25.f + 30.f * Power, Power * 0.8f);
	}
	// Droplets thrown out across the water round it.
	const int32 Count = FMath::RoundToInt(SplashDroplets * Power * 0.6f);
	for (int32 Index = 0; Index < Count && Droplets.Num() < MaxDroplets; ++Index)
	{
		FDroplet& Drop = Droplets.AddDefaulted_GetRef();
		const FVector2D Out = FVector2D(1.f, 0.f).GetRotated(FMath::FRandRange(0.f, 360.f));
		Drop.Position = FVector2D(Location.X, Location.Z) + Out * FMath::FRandRange(4.f, 14.f);
		Drop.Velocity = Out * FMath::FRandRange(90.f, 260.f) * Power;
		Drop.Life = FMath::FRandRange(0.3f, 0.6f);
		Drop.Radius = FMath::FRandRange(2.f, 4.f);
	}
	const FVector Where(Location.X, GetActorLocation().Y + AboveMeshDepth + 2.f, Location.Z);
	if (SplashEffect)
	{
		AOnyxFlipbookEffect::Spawn(this, SplashEffect, Where, 1.f);
	}
	if (SplashSound)
	{
		UOnyxAudioSubsystem::PlaySoundAt(this, SplashSound, Where, EOnyxSoundCategory::Effects, SplashVolume * FMath::Max(Power, 0.3f), FMath::FRandRange(0.92f, 1.08f));
	}
}

void AOnyxWater::AffectFromAbove(float DeltaSeconds)
{
	TArray<FOverlapResult> Results;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OnyxWater), false, this);
	GetWorld()->OverlapMultiByObjectType(Results, Area->GetComponentLocation(), Area->GetComponentQuat(), FCollisionObjectQueryParams::AllDynamicObjects,
		FCollisionShape::MakeBox(Area->GetScaledBoxExtent()), Params);

	TSet<TWeakObjectPtr<UOnyxMovementComponent>> StillIn;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> Seen;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> InWater;
	for (const FOverlapResult& Result : Results)
	{
		UPrimitiveComponent* Primitive = Result.GetComponent();
		if (!Primitive || Primitive->GetOwner() == this || Seen.Contains(Primitive))
		{
			continue;
		}
		Seen.Add(Primitive);
		// A character is in the water where its feet are (from above its capsule stands on them); it wades, then swims
		// (its movement component decides which from how far in it is).
		const ACharacter* Character = Cast<ACharacter>(Primitive->GetOwner());
		UOnyxMovementComponent* Movement = Character && Primitive == Character->GetRootComponent() ? Cast<UOnyxMovementComponent>(Character->GetCharacterMovement()) : nullptr;
		FVector Where;
		FVector Velocity;
		float Width;
		if (Movement)
		{
			Where = Character->GetActorLocation();
			if (GetDepthFromAbove(Where) <= 0.f)
			{
				continue;
			}
			Movement->SetWater(this);
			StillIn.Add(Movement);
			Velocity = Movement->Velocity;
			Width = Primitive->Bounds.BoxExtent.X * 2.f;
		}
		else if (Primitive->IsSimulatingPhysics())
		{
			Where = Primitive->Bounds.Origin;
			if (GetDepthFromAbove(Where) <= 0.f)
			{
				continue;
			}
			// Floating: slowed by the water, carried by its current, turning less.
			Velocity = Primitive->GetPhysicsLinearVelocity();
			Primitive->AddForce(-FVector(Velocity.X, 0.f, Velocity.Z) * Drag * Primitive->GetMass());
			const FVector2D Flow = GetCurrentAt(Where);
			Primitive->AddForce(FVector(Flow.X, 0.f, Flow.Y), NAME_None, true);
			Primitive->SetPhysicsAngularVelocityInRadians(Primitive->GetPhysicsAngularVelocityInRadians() * FMath::Exp(-SpinDrag * DeltaSeconds));
			Width = Primitive->Bounds.BoxExtent.X * 2.f;
		}
		else
		{
			continue;
		}
		InWater.Add(Primitive);
		const float Speed = FVector2D(Velocity.X, Velocity.Z).Size();
		// Going in: a splash.
		if (!LastCenters.Contains(Primitive))
		{
			SplashFromAbove(Where, FMath::Max(Speed / 400.f, 0.35f));
		}
		LastCenters.Add(Primitive, 1.f);
		// Ripples spreading round it: often while it moves, now and then while it keeps still.
		if (bWakes && WakeAmount > 0.f)
		{
			float& Timer = RippleTimers.FindOrAdd(Primitive, FMath::FRandRange(0.f, 0.3f));
			Timer -= DeltaSeconds;
			if (Timer <= 0.f)
			{
				const bool bMoving = Speed > 40.f;
				EmitRing(Where, Width * (bMoving ? 1.3f : 1.f) + 18.f, bMoving ? 0.7f : 0.35f);
				Timer = (bMoving ? FMath::Clamp(0.36f - Speed / 1500.f, 0.12f, 0.36f) : FMath::FRandRange(0.9f, 1.4f)) / WakeAmount;
			}
		}
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
		if (!It->Key.IsValid() || !InWater.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = RippleTimers.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() || !InWater.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
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
		LeaveWake(Box, Velocity, DeltaSeconds);

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

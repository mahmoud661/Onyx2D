// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxRain.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/Onyx2DLibrary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Lighting/OnyxLightBufferRenderer.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	/**
	 * Vertex colours are 8 bits (0-1), but drops in lamplight are brighter than white so they bloom: colours are stored
	 * divided by this and the material multiplies them back (its Brightness parameter).
	 */
	constexpr float RainColorRange = 3.f;
	/** A drop falling this fast drifts sideways at exactly Wind; faster or slower drops drift in proportion (parallel streaks). */
	constexpr float RainWindReferenceSpeed = 1800.f;
	constexpr float RainDropletGravity = 1200.f;
	constexpr int32 RainMaxBurstsPerLayer = 800;

	float RainLuminance(const FLinearColor& Color)
	{
		return Color.R * 0.2126f + Color.G * 0.7152f + Color.B * 0.0722f;
	}
}

AOnyxRain::AOnyxRain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the camera has moved this frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Onyx2D/Materials/M_OnyxRain.M_OnyxRain")));

	// Behind the scenery: fine, faint, slow.
	FOnyxRainLayer Far;
	Far.Name = TEXT("Far");
	Far.Depth = -600.f;
	Far.Drops = 260;
	Far.Speed = 1300.f;
	Far.Length = 34.f;
	Far.Width = 3.f;
	Far.Opacity = 0.22f;
	Far.LightResponse = 0.35f;
	// Just behind the gameplay: lands on the ground and splashes.
	FOnyxRainLayer Middle;
	Middle.Name = TEXT("Middle");
	Middle.Depth = -2.f; // in front of tile maps' back layers, behind their front layer, props and characters
	Middle.Drops = 320;
	Middle.Speed = 1900.f;
	Middle.Length = 56.f;
	Middle.Width = 4.f;
	Middle.Opacity = 0.42f;
	Middle.bSplash = true;
	// In front of everything: long, sparse streaks rushing past the camera.
	FOnyxRainLayer Near;
	Near.Name = TEXT("Near");
	Near.Depth = 400.f;
	Near.Drops = 60;
	Near.Speed = 2600.f;
	Near.Length = 120.f;
	Near.Width = 5.f;
	Near.Opacity = 0.28f;
	Near.LightResponse = 0.7f;
	RainLayers = { Far, Middle, Near };
}

void AOnyxRain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// One mesh per layer, made by the construction script (rebuilt whenever a property changes).
	Meshes.Reset();
	States.SetNum(RainLayers.Num());
	for (int32 Index = 0; Index < RainLayers.Num(); ++Index)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transactional | RF_Transient);
		Mesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Mesh->SetupAttachment(RootComponent);
		// Vertices are in world space: the drops follow the camera, not the actor.
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetWorldTransform(FTransform::Identity);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->bSelectable = false; // clicking the rain in the viewport selects what is behind it
		Mesh->RegisterComponent();
		Meshes.Add(Mesh);
	}
}

void AOnyxRain::BeginPlay()
{
	Super::BeginPlay();
	IntensityTarget = Intensity;
	if (RainSound)
	{
		SoundLoop = UGameplayStatics::SpawnSound2D(this, RainSound, SoundVolume * Intensity, 1.f, 0.f, nullptr, false, false);
	}
}

void AOnyxRain::SetIntensity(float NewIntensity, float FadeTime)
{
	IntensityTarget = FMath::Clamp(NewIntensity, 0.f, 1.f);
	IntensityRate = FadeTime > 0.f ? FMath::Abs(IntensityTarget - Intensity) / FadeTime : 0.f;
	if (FadeTime <= 0.f)
	{
		Intensity = IntensityTarget;
	}
}

bool AOnyxRain::GetRainArea(float LayerY, FBox2D& OutArea) const
{
	const UOnyxLightingSubsystem* Lighting = UOnyxLightingSubsystem::Get(this);
	FOnyxLightView View;
	FVector Target;
	if (!Lighting || !Lighting->GetView(View, Target))
	{
		return false;
	}
	const FMinimalViewInfo& POV = View.POV;
	const float HalfWidth = POV.ProjectionMode == ECameraProjectionMode::Orthographic
		? POV.OrthoWidth * 0.5f
		: FMath::Abs(POV.Location.Y - LayerY) * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(POV.FOV, 1.f, 170.f) * 0.5f));
	const float Width = FMath::Min(HalfWidth, 20000.f);
	const FVector2D Half(Width + Margin, Width * View.ViewportSize.Y / FMath::Max(View.ViewportSize.X, 1) + Margin);
	const FVector2D Center(POV.Location.X, POV.Location.Z);
	OutArea = FBox2D(Center - Half, Center + Half);
	return true;
}

void AOnyxRain::FindStop(FDrop& Drop, const FVector2D& Direction, float BottomZ) const
{
	Drop.StopZ = -UE_BIG_NUMBER;
	Drop.bSplashAtStop = false;
	const float Fall = Drop.Position.Y - BottomZ;
	if (Direction.Y > -KINDA_SMALL_NUMBER || Fall <= 0.f)
	{
		return;
	}
	if (bFromAbove)
	{
		// From above a drop falls towards the ground under it: it lands on a spot of its own somewhere down its path in the
		// view, and splashes there (on roofs and hedge tops too).
		const float Drop01 = FMath::FRandRange(0.15f, 1.f);
		Drop.StopZ = Drop.Position.Y - Fall * Drop01;
		Drop.Stop = Drop.Position + Direction * (Fall * Drop01 / -Direction.Y);
		Drop.bSplashAtStop = true;
		return;
	}
	// Along the drop's slanted path, on the gameplay plane; only the level's static and moving geometry stops it (not characters).
	const FVector Start(Drop.Position.X, 0.f, Drop.Position.Y);
	const FVector End = Start + FVector(Direction.X, 0.f, Direction.Y) * (Fall / -Direction.Y);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, Start, End, Objects, FCollisionQueryParams(SCENE_QUERY_STAT(OnyxRain), false, this));
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
	for (const FHitResult& Hit : Hits)
	{
		// Only what a character would stand on or bump into: triggers, hitboxes and pickups let the rain through.
		const UPrimitiveComponent* Component = Hit.GetComponent();
		if (!Component || Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			continue;
		}
		Drop.Stop = FVector2D(Hit.ImpactPoint.X, Hit.ImpactPoint.Z);
		Drop.StopZ = Hit.ImpactPoint.Z;
		Drop.bSplashAtStop = Hit.ImpactNormal.Z > 0.5f; // against a wall a drop just ends
		break;
	}
}

void AOnyxRain::Respawn(FDrop& Drop, const FOnyxRainLayer& Layer, const FBox2D& Area, const FVector2D& Direction, bool bAtTop) const
{
	Drop.Length = Layer.Length * FMath::FRandRange(0.7f, 1.3f) * (bFromAbove ? 0.45f : 1.f); // from above, short streaks
	Drop.Alpha = FMath::FRandRange(0.5f, 1.f);
	Drop.SpeedScale = FMath::FRandRange(0.85f, 1.15f);
	Drop.Position.X = FMath::FRandRange(Area.Min.X, Area.Max.X);
	Drop.Position.Y = bAtTop ? Area.Max.Y + FMath::FRand() * Drop.Length * 2.f : FMath::FRandRange(Area.Min.Y, Area.Max.Y);
	Drop.StopZ = -UE_BIG_NUMBER;
	Drop.bSplashAtStop = false;
	if (Layer.bSplash || bFromAbove)
	{
		FindStop(Drop, Direction, Area.Min.Y);
		Drop.bSplashAtStop &= Layer.bSplash; // from above every drop lands; only a splashing layer's splash
	}
}

void AOnyxRain::Splash(FLayerState& State, const FVector2D& Where)
{
	if (State.Bursts.Num() >= RainMaxBurstsPerLayer)
	{
		return;
	}
	FBurst& Ring = State.Bursts.AddDefaulted_GetRef();
	Ring.Kind = EBurstKind::Ring;
	Ring.Position = Where;
	Ring.Life = FMath::FRandRange(0.16f, 0.26f);
	Ring.Size = SplashSize * FMath::FRandRange(0.75f, 1.2f);
	for (int32 Index = 0; Index < SplashDroplets; ++Index)
	{
		FBurst& Droplet = State.Bursts.AddDefaulted_GetRef();
		Droplet.Kind = EBurstKind::Droplet;
		Droplet.Position = Where + FVector2D(0.f, 2.f);
		// From the side they leap up and fall back; from above they spray out across the ground.
		Droplet.Velocity = bFromAbove ? FVector2D(1.f, 0.f).GetRotated(FMath::FRandRange(0.f, 360.f)) * FMath::FRandRange(50.f, 130.f)
			: FVector2D(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(110.f, 250.f));
		Droplet.Life = FMath::FRandRange(0.28f, 0.42f);
		Droplet.Size = FMath::FRandRange(2.5f, 4.f);
	}
}

void AOnyxRain::GatherLights(const FBox2D& Area)
{
	LightSamples.Reset();
	const UOnyxLightingSubsystem* Lighting = bLitByLights ? UOnyxLightingSubsystem::Get(this) : nullptr;
	if (!Lighting)
	{
		return;
	}
	const float Strength = Lighting->GetCurrentMood().LightStrength;
	for (const UOnyxLight2DComponent* Light : Lighting->GetLights())
	{
		const float Brightness = Light->GetEffectiveIntensity() * Strength;
		if (Brightness <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector Location = Light->GetComponentLocation();
		const FVector2D Position(Location.X, Location.Z);
		const FBox2D Reach(Position - FVector2D(Light->Radius), Position + FVector2D(Light->Radius));
		if (Reach.Intersect(Area))
		{
			LightSamples.Add({ Position, Light->Radius, Light->Color * Brightness });
		}
	}
}

bool AOnyxRain::IsSheltered(const FVector2D& Where) const
{
	for (const FBox2D& Area : ShelterAreas)
	{
		if (Area.IsInside(Where))
		{
			return true;
		}
	}
	return false;
}

FLinearColor AOnyxRain::LightAt(const FVector2D& Where) const
{
	FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
	for (const FLightSample& Light : LightSamples)
	{
		const float Distance = FVector2D::Distance(Where, Light.Position);
		if (Distance < Light.Radius)
		{
			Sum += Light.Color * FMath::Square(1.f - Distance / Light.Radius);
		}
	}
	return Sum;
}

void AOnyxRain::Simulate(int32 LayerIndex, float DeltaSeconds, const FVector2D& Velocity)
{
	const FOnyxRainLayer& Layer = RainLayers[LayerIndex];
	FLayerState& State = States[LayerIndex];
	FBox2D Area;
	if (!GetRainArea(GetActorLocation().Y + Layer.Depth, Area))
	{
		return;
	}
	const FVector2D Direction = Velocity.GetSafeNormal();
	const int32 Wanted = FMath::RoundToInt(Layer.Drops * FMath::Clamp(Intensity, 0.f, 1.f));
	if (State.Drops.Num() > Wanted)
	{
		State.Drops.SetNum(Wanted);
	}
	while (State.Drops.Num() < Wanted)
	{
		Respawn(State.Drops.AddDefaulted_GetRef(), Layer, Area, Direction, false);
	}

	const float AreaWidth = Area.Max.X - Area.Min.X;
	for (FDrop& Drop : State.Drops)
	{
		Drop.Position += Velocity * Drop.SpeedScale * DeltaSeconds;
		// Keep every drop around the camera as it moves: wrap around sideways, start again at the top below the view.
		if (Drop.Position.X < Area.Min.X || Drop.Position.X > Area.Max.X)
		{
			Drop.Position.X += Drop.Position.X < Area.Min.X ? AreaWidth : -AreaWidth;
			if (Layer.bSplash || bFromAbove)
			{
				FindStop(Drop, Direction, Area.Min.Y);
				Drop.bSplashAtStop &= Layer.bSplash;
			}
		}
		if ((Layer.bSplash || bFromAbove) && Drop.Position.Y <= Drop.StopZ)
		{
			if (Drop.bSplashAtStop && !IsSheltered(Drop.Stop))
			{
				Splash(State, Drop.Stop);
			}
			Respawn(Drop, Layer, Area, Direction, true);
		}
		else if (Drop.Position.Y < Area.Min.Y)
		{
			Respawn(Drop, Layer, Area, Direction, true);
		}
		else if (Drop.Position.Y > Area.Max.Y + Layer.Length * 3.f)
		{
			Respawn(Drop, Layer, Area, Direction, false); // the camera fell faster than the rain
		}
	}

	for (int32 Index = State.Bursts.Num() - 1; Index >= 0; --Index)
	{
		FBurst& Burst = State.Bursts[Index];
		Burst.Age += DeltaSeconds;
		if (Burst.Age >= Burst.Life)
		{
			State.Bursts.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}
		if (Burst.Kind == EBurstKind::Droplet)
		{
			if (bFromAbove)
			{
				Burst.Velocity *= FMath::Exp(-6.f * DeltaSeconds);
			}
			else
			{
				Burst.Velocity.Y -= RainDropletGravity * DeltaSeconds;
			}
			Burst.Position += Burst.Velocity * DeltaSeconds;
		}
	}
}

void AOnyxRain::BuildMesh(int32 LayerIndex, const FVector2D& Velocity)
{
	UProceduralMeshComponent* Mesh = Meshes.IsValidIndex(LayerIndex) ? Meshes[LayerIndex].Get() : nullptr;
	if (!Mesh)
	{
		return;
	}
	const FOnyxRainLayer& Layer = RainLayers[LayerIndex];
	const FLayerState& State = States[LayerIndex];
	if (State.Drops.IsEmpty() && State.Bursts.IsEmpty())
	{
		if (Mesh->GetNumSections() > 0)
		{
			Mesh->ClearAllMeshSections();
		}
		return;
	}
	if (!DropMaterial)
	{
		if (UMaterialInterface* Base = Material.LoadSynchronous())
		{
			DropMaterial = UMaterialInstanceDynamic::Create(Base, this);
			DropMaterial->SetScalarParameterValue(TEXT("Brightness"), RainColorRange);
		}
	}

	const int32 Count = State.Drops.Num() + State.Bursts.Num();
	TArray<FVector> Vertices;
	TArray<FVector2D> UVs;
	TArray<FVector2D> Kinds; // UV1.x: 0 streak, 1 ring, 2 droplet
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;
	Vertices.Reserve(Count * 4);
	UVs.Reserve(Count * 4);
	Kinds.Reserve(Count * 4);
	Colors.Reserve(Count * 4);
	Triangles.Reserve(Count * 6);
	const float LayerY = GetActorLocation().Y + Layer.Depth;
	// Splashes sit just in front of the ground tiles, so the street doesn't hide them.
	const float BurstY = FMath::Max(LayerY, GetActorLocation().Y) + 3.f;
	auto AddQuad = [&](const FVector2D& Start, const FVector2D& Along, const FVector2D& Across, float Y, const FLinearColor& QuadColor, float Kind)
	{
		const int32 First = Vertices.Num();
		const FVector A(Start.X - Across.X, Y, Start.Y - Across.Y);
		const FVector B(Start.X + Across.X, Y, Start.Y + Across.Y);
		const FVector Offset(Along.X, 0.f, Along.Y);
		Vertices.Append({ A, B, A + Offset, B + Offset });
		UVs.Append({ FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f) });
		const FVector2D Shape(Kind, 0.f);
		Kinds.Append({ Shape, Shape, Shape, Shape });
		const FLinearColor Stored(FMath::Clamp(QuadColor.R / RainColorRange, 0.f, 1.f), FMath::Clamp(QuadColor.G / RainColorRange, 0.f, 1.f), FMath::Clamp(QuadColor.B / RainColorRange, 0.f, 1.f),
			FMath::Clamp(QuadColor.A, 0.f, 1.f));
		Colors.Append({ Stored, Stored, Stored, Stored });
		Triangles.Append({ First, First + 2, First + 1, First + 2, First + 3, First + 1 });
	};

	// Streaks run from the head (UV.y = 0) back up along the way they came.
	const FVector2D Back = (-Velocity).GetSafeNormal();
	const FVector2D Side(-Back.Y, Back.X);
	for (const FDrop& Drop : State.Drops)
	{
		if (!ShelterAreas.IsEmpty() && IsSheltered(Drop.Position))
		{
			continue;
		}
		const FLinearColor Light = LightSamples.IsEmpty() ? FLinearColor::Transparent : LightAt(Drop.Position) * Layer.LightResponse;
		FLinearColor Tint = Color + Light;
		Tint.A = FMath::Min(Layer.Opacity * Drop.Alpha * (1.f + 0.8f * FMath::Min(RainLuminance(Light), 1.f)), 1.f);
		AddQuad(Drop.Position, Back * Drop.Length, Side * Layer.Width * 0.5f, LayerY, Tint, 0.f);
	}
	for (const FBurst& Burst : State.Bursts)
	{
		const float T = Burst.Age / Burst.Life;
		const FLinearColor Light = LightSamples.IsEmpty() ? FLinearColor::Transparent : LightAt(Burst.Position) * Layer.LightResponse;
		FLinearColor Tint = SplashColor + Light;
		if (Burst.Kind == EBurstKind::Ring)
		{
			// Opens out quickly then fades: a flat ellipse on the ground.
			const float Grow = FMath::Lerp(0.3f, 1.f, 1.f - FMath::Square(1.f - T));
			const FVector2D Half(Burst.Size * 0.5f * Grow, Burst.Size * 0.16f * Grow);
			Tint.A = 0.85f * FMath::Pow(1.f - T, 1.3f);
			AddQuad(Burst.Position - FVector2D(0.f, Half.Y), FVector2D(0.f, Half.Y * 2.f), FVector2D(Half.X, 0.f), BurstY, Tint, 1.f);
		}
		else
		{
			const float Half = Burst.Size * 0.5f;
			Tint.A = 0.9f * (1.f - T);
			AddQuad(Burst.Position - FVector2D(0.f, Half), FVector2D(0.f, Half * 2.f), FVector2D(Half, 0.f), BurstY, Tint, 2.f);
		}
	}
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, UVs, Kinds, {}, {}, Colors, {}, false);
	Mesh->SetMaterial(0, DropMaterial);
}

void AOnyxRain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f); // a hitch doesn't teleport the rain
	Time += DeltaSeconds;
	if (const bool bAbove = UOnyx2DLibrary::IsTopDown(this); bAbove != bFromAbove)
	{
		bFromAbove = bAbove;
		States.Reset(); // the drops start again the new way
	}
	if (IntensityRate > 0.f)
	{
		Intensity = FMath::FInterpConstantTo(Intensity, IntensityTarget, DeltaSeconds, IntensityRate);
	}
	if (States.Num() != RainLayers.Num())
	{
		States.SetNum(RainLayers.Num());
	}

	ShelterAreas.Reset();
	for (const AActor* Shelter : Shelters)
	{
		if (Shelter)
		{
			const FBox Box = Shelter->GetComponentsBoundingBox(true);
			if (Box.IsValid)
			{
				ShelterAreas.Add(FBox2D(FVector2D(Box.Min.X, Box.Min.Z), FVector2D(Box.Max.X, Box.Max.Z)));
			}
		}
	}

	// Gusts: two slow waves, so the sway never repeats exactly.
	const float Gust = 1.f + Gustiness * FMath::Sin(Time * 0.6f) * FMath::Sin(Time * 0.17f + 1.3f);
	FBox2D Area;
	if (GetRainArea(GetActorLocation().Y, Area))
	{
		GatherLights(Area);
	}
	for (int32 Index = 0; Index < RainLayers.Num(); ++Index)
	{
		const FOnyxRainLayer& Layer = RainLayers[Index];
		const FVector2D Velocity(Wind * Gust * Layer.Speed / RainWindReferenceSpeed, -Layer.Speed);
		Simulate(Index, DeltaSeconds, Velocity);
		BuildMesh(Index, Velocity);
	}
	if (SoundLoop)
	{
		SoundLoop->SetVolumeMultiplier(SoundVolume * Intensity);
	}
}

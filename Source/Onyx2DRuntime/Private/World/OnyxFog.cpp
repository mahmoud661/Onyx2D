// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxFog.h"
#include "Components/BoxComponent.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "Lighting/OnyxLightingSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace OnyxFog
{
	/** How far in front of (or behind) the characters' plane the fog is drawn, cm (the camera is at +Y). */
	constexpr float FrontDepth = 40.f;
	constexpr float BehindDepth = -40.f;
	/** How fast it turns to its night thickness and back (share per second). */
	constexpr float NightEase = 0.35f;

	/** Fades in over Edge from the start of 0..1 and over EndEdge from its end (0: a hard side). */
	float EdgeFade(float T, float Edge, float EndEdge)
	{
		const float In = Edge <= KINDA_SMALL_NUMBER ? (T >= 0.f ? 1.f : 0.f) : FMath::SmoothStep(0.f, Edge, T);
		const float Out = EndEdge <= KINDA_SMALL_NUMBER ? (T <= 1.f ? 1.f : 0.f) : FMath::SmoothStep(0.f, EndEdge, 1.f - T);
		return In * Out;
	}
}

// --- Onyx Fog ------------------------------------------------------------------------------------------------------------

AOnyxFog::AOnyxFog()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the lights have moved this frame

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetHiddenInGame(true);
	Area->ShapeColor = FColor(150, 180, 200);
	RootComponent = Area;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Area);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bUseAsyncCooking = true;

	Look = GetPresetLook(Preset);
}

FOnyxFogLook AOnyxFog::GetPresetLook(EOnyxFogPreset InPreset)
{
	FOnyxFogLook Result;
	switch (InPreset)
	{
	case EOnyxFogPreset::GroundMist:
		Result.Color = FLinearColor(0.66f, 0.72f, 0.8f, 0.5f);
		Result.Density = 0.45f;
		Result.CloudSize = 360.f;
		Result.Drift = FVector2D(16.f, 0.f);
		Result.Ground = 0.85f;
		Result.Detail = 0.45f;
		Result.EdgeSoftness = 340.f;
		Result.LightScatter = 0.8f;
		Result.OwnLight = 0.5f;
		break;
	case EOnyxFogPreset::MarshFog:
		Result.Color = FLinearColor(0.55f, 0.66f, 0.5f, 0.7f);
		Result.Density = 0.65f;
		Result.CloudSize = 520.f;
		Result.Drift = FVector2D(9.f, 2.f);
		Result.Ground = 0.45f;
		Result.Detail = 0.35f;
		Result.EdgeSoftness = 460.f;
		Result.LightScatter = 0.9f;
		Result.OwnLight = 0.35f;
		break;
	case EOnyxFogPreset::CaveHaze:
		Result.Color = FLinearColor(0.6f, 0.62f, 0.7f, 0.26f);
		Result.Density = 0.6f;
		Result.CloudSize = 700.f;
		Result.Drift = FVector2D(5.f, 1.f);
		Result.Ground = 0.15f;
		Result.Detail = 0.2f;
		Result.EdgeSoftness = 420.f;
		Result.LightScatter = 0.9f;
		Result.OwnLight = 0.2f;
		break;
	case EOnyxFogPreset::FogWall:
		Result.Color = FLinearColor(0.62f, 0.66f, 0.72f, 0.95f);
		Result.Density = 0.95f;
		Result.CloudSize = 420.f;
		Result.Drift = FVector2D(6.f, 0.f);
		Result.Ground = 0.1f;
		Result.Detail = 0.4f;
		Result.EdgeSoftness = 300.f;
		Result.LightScatter = 0.7f;
		Result.OwnLight = 0.5f;
		break;
	case EOnyxFogPreset::NightMist:
		Result.Color = FLinearColor(0.5f, 0.62f, 0.85f, 0.45f);
		Result.Density = 0.4f;
		Result.CloudSize = 300.f;
		Result.Drift = FVector2D(20.f, 3.f);
		Result.Ground = 0.6f;
		Result.Detail = 0.7f;
		Result.EdgeSoftness = 320.f;
		Result.LightScatter = 1.0f;
		Result.OwnLight = 0.25f;
		break;
	default:
		break;
	}
	return Result;
}

void AOnyxFog::ApplyPreset()
{
	if (Preset != EOnyxFogPreset::Custom)
	{
		Look = GetPresetLook(Preset);
		if (Preset == EOnyxFogPreset::NightMist)
		{
			NightThickness = 2.2f;
		}
	}
	BuildMesh();
}

void AOnyxFog::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildMesh();
}

#if WITH_EDITOR
void AOnyxFog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AOnyxFog, Preset))
	{
		ApplyPreset();
	}
	else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AOnyxFog, Look))
	{
		Preset = EOnyxFogPreset::Custom; // tuned by hand now
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AOnyxFog::BeginPlay()
{
	Super::BeginPlay();
	if (UOnyxFogSubsystem* Subsystem = UOnyxFogSubsystem::Get(this))
	{
		Subsystem->Register(this);
	}
	TActorIterator<AOnyxLightingEnvironment> Environment(GetWorld());
	NightWeight = Environment && Environment->IsNight() ? 1.f : 0.f; // a level that starts at night starts thick
	BuildMesh(); // its material is made at run time (a level keeps the mesh, not the material instance)
}

void AOnyxFog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOnyxFogSubsystem* Subsystem = UOnyxFogSubsystem::Get(this))
	{
		Subsystem->Unregister(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AOnyxFog::BuildMesh()
{
	Area->SetBoxExtent(FVector(Size.X * 0.5f, 20.f, Size.Y * 0.5f));
	// One quad over the box: UV runs 0 to 1 across, 0 at the top to 1 at the bottom; the shader takes its clouds from
	// world positions, so the fog stays put in the world wherever the box is.
	const float Depth = bInFront ? OnyxFog::FrontDepth : OnyxFog::BehindDepth;
	const float HalfX = Size.X * 0.5f;
	const float HalfZ = Size.Y * 0.5f;
	const TArray<FVector> Vertices = { FVector(-HalfX, Depth, HalfZ), FVector(HalfX, Depth, HalfZ), FVector(-HalfX, Depth, -HalfZ), FVector(HalfX, Depth, -HalfZ) };
	const TArray<FVector2D> UVs = { FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f) };
	const TArray<int32> Triangles = { 0, 2, 1, 1, 2, 3 };
	Mesh->ClearAllMeshSections();
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, {}, UVs, {}, {}, false);
	Mesh->TranslucencySortPriority = bInFront ? 20 : -20;
	if (!Material)
	{
		if (UMaterialInterface* Base = UOnyx2DSettings::Get()->FogMaterial.LoadSynchronous())
		{
			Material = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
	Mesh->SetMaterial(0, Material);
	UpdateMaterial(0.f);
}

void AOnyxFog::GatherClearings(TArray<FVector, TInlineAllocator<4>>& Out) const
{
	Out.Reset();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<FVector, TInlineAllocator<8>> All;
	const APawn* Player = World->IsGameWorld() ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	const FVector Focus = Player ? Player->GetActorLocation() : GetActorLocation();
	if (Player && PlayerClearRadius > 0.f)
	{
		All.Add(FVector(Focus.X, Focus.Z, PlayerClearRadius));
	}
	if (const UOnyxFogSubsystem* Subsystem = UOnyxFogSubsystem::Get(this))
	{
		for (const TWeakObjectPtr<UOnyxFogClearerComponent>& Weak : Subsystem->GetClearers())
		{
			const UOnyxFogClearerComponent* Clearer = Weak.Get();
			if (Clearer && Clearer->bClearing && Clearer->Radius > 0.f)
			{
				const FVector At = Clearer->GetComponentLocation();
				All.Add(FVector(At.X, At.Z, Clearer->Radius));
			}
		}
	}
	// Only those that reach the box, the ones nearest the player first.
	const FVector Middle = GetActorLocation();
	All.RemoveAll([&](const FVector& C)
	{
		return FMath::Abs(C.X - Middle.X) > Size.X * 0.5f + C.Z || FMath::Abs(C.Y - Middle.Z) > Size.Y * 0.5f + C.Z;
	});
	All.Sort([&](const FVector& A, const FVector& B)
	{
		return FVector2D(A.X - Focus.X, A.Y - Focus.Z).SizeSquared() < FVector2D(B.X - Focus.X, B.Y - Focus.Z).SizeSquared();
	});
	for (int32 Index = 0; Index < All.Num() && Out.Num() < 4; ++Index)
	{
		Out.Add(All[Index]);
	}
}

void AOnyxFog::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMaterial(DeltaSeconds);
}

void AOnyxFog::UpdateMaterial(float DeltaSeconds)
{
	if (!Material)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && DeltaSeconds > 0.f)
	{
		TActorIterator<AOnyxLightingEnvironment> It(World);
		const float NightTarget = It && It->IsNight() ? 1.f : 0.f;
		NightWeight = FMath::FInterpConstantTo(NightWeight, NightTarget, DeltaSeconds, OnyxFog::NightEase);
	}
	const UOnyxFogSubsystem* Subsystem = UOnyxFogSubsystem::Get(this);
	const float Amount = (Subsystem ? Subsystem->GetFogAmount() : 1.f) * FMath::Lerp(1.f, NightThickness, NightWeight);

	Material->SetVectorParameterValue(TEXT("Tint"), Look.Color);
	Material->SetScalarParameterValue(TEXT("Density"), Look.Density);
	Material->SetScalarParameterValue(TEXT("CloudSize"), Look.CloudSize);
	Material->SetVectorParameterValue(TEXT("Drift"), FLinearColor(Look.Drift.X, Look.Drift.Y, 0.f, 0.f));
	Material->SetScalarParameterValue(TEXT("Ground"), Look.Ground);
	Material->SetScalarParameterValue(TEXT("Detail"), Look.Detail);
	Material->SetVectorParameterValue(TEXT("Edge"), FLinearColor(FMath::Min(Look.EdgeSoftness / FMath::Max(Size.X, 1.f), 0.5f),
		FMath::Min(Look.EdgeSoftness / FMath::Max(Size.Y, 1.f), 0.5f), 0.f, 0.f));
	Material->SetScalarParameterValue(TEXT("Scatter"), Look.LightScatter);
	Material->SetScalarParameterValue(TEXT("OwnLight"), Look.OwnLight);
	Material->SetScalarParameterValue(TEXT("Amount"), Amount);

	TArray<FVector, TInlineAllocator<4>> Clearings;
	GatherClearings(Clearings);
	static const FName ClearNames[] = { TEXT("Clear0"), TEXT("Clear1"), TEXT("Clear2"), TEXT("Clear3") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FLinearColor Value = Clearings.IsValidIndex(Index)
			? FLinearColor(Clearings[Index].X, Clearings[Index].Y, Clearings[Index].Z, ClearSoftness) : FLinearColor(0.f, 0.f, 0.f, 0.f);
		Material->SetVectorParameterValue(ClearNames[Index], Value);
	}

	// The 2D lights' buffer, to glow where they shine (it is remade when the view changes size).
	const UOnyxLightingSubsystem* Lighting = UOnyxLightingSubsystem::Get(this);
	UTexture* Buffer = Lighting ? Lighting->GetLightBuffer() : nullptr;
	if (Buffer != ShownLightBuffer)
	{
		ShownLightBuffer = Buffer;
		if (Buffer)
		{
			Material->SetTextureParameterValue(TEXT("LightBuffer"), Buffer);
		}
	}
}

float AOnyxFog::GetThicknessAt(const FVector& Location) const
{
	const FVector Local = Location - GetActorLocation();
	const float U = Local.X / FMath::Max(Size.X, 1.f) + 0.5f;
	const float V = 0.5f - Local.Z / FMath::Max(Size.Y, 1.f);
	if (U < 0.f || U > 1.f || V < 0.f || V > 1.f)
	{
		return 0.f;
	}
	const float EdgeX = FMath::Min(Look.EdgeSoftness / FMath::Max(Size.X, 1.f), 0.5f);
	const float EdgeY = FMath::Min(Look.EdgeSoftness / FMath::Max(Size.Y, 1.f), 0.5f);
	// As drawn: a layer lying on the ground fades less at its bottom (the floor), the more Ground says.
	const float Edge = OnyxFog::EdgeFade(U, EdgeX, EdgeX) * OnyxFog::EdgeFade(V, EdgeY, EdgeY * (1.f - 0.8f * Look.Ground));
	const float GroundLayer = FMath::Lerp(1.f, FMath::Clamp(V * 1.3f, 0.f, 1.f), Look.Ground);
	const UOnyxFogSubsystem* Subsystem = UOnyxFogSubsystem::Get(this);
	const float Amount = (Subsystem ? Subsystem->GetFogAmount() : 1.f) * FMath::Lerp(1.f, NightThickness, NightWeight);
	float Thickness = FMath::Clamp(Look.Density * Amount * 1.2f, 0.f, 1.f) * Look.Color.A * Edge * GroundLayer;
	TArray<FVector, TInlineAllocator<4>> Clearings;
	GatherClearings(Clearings);
	for (const FVector& Clearing : Clearings)
	{
		const float Distance = FVector2D(Location.X - Clearing.X, Location.Z - Clearing.Y).Size();
		const float Radius = float(Clearing.Z);
		Thickness *= FMath::SmoothStep(Radius * (1.f - ClearSoftness), Radius, Distance);
	}
	return FMath::Clamp(Thickness, 0.f, 1.f);
}

// --- Onyx Fog Clearer ----------------------------------------------------------------------------------------------------------

void UOnyxFogClearerComponent::OnRegister()
{
	Super::OnRegister();
	if (UOnyxFogSubsystem* Subsystem = UOnyxFogSubsystem::Get(this))
	{
		Subsystem->Register(this);
	}
}

void UOnyxFogClearerComponent::OnUnregister()
{
	if (UOnyxFogSubsystem* Subsystem = UOnyxFogSubsystem::Get(this))
	{
		Subsystem->Unregister(this);
	}
	Super::OnUnregister();
}

// --- Onyx Fog subsystem ------------------------------------------------------------------------------------------------------------

UOnyxFogSubsystem* UOnyxFogSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UOnyxFogSubsystem>() : nullptr;
}

void UOnyxFogSubsystem::SetFogAmount(float InAmount, float Seconds)
{
	TargetAmount = FMath::Max(InAmount, 0.f);
	if (Seconds <= 0.f)
	{
		Amount = TargetAmount;
		FadeSpeed = 0.f;
		return;
	}
	FadeSpeed = FMath::Abs(TargetAmount - Amount) / Seconds;
}

float UOnyxFogSubsystem::GetFogAt(const FVector& Location) const
{
	float Thickest = 0.f;
	for (const TWeakObjectPtr<AOnyxFog>& Weak : Fogs)
	{
		if (const AOnyxFog* Fog = Weak.Get())
		{
			Thickest = FMath::Max(Thickest, Fog->GetThicknessAt(Location));
		}
	}
	return Thickest;
}

void UOnyxFogSubsystem::Tick(float DeltaTime)
{
	if (FadeSpeed > 0.f)
	{
		Amount = FMath::FInterpConstantTo(Amount, TargetAmount, DeltaTime, FadeSpeed);
		if (FMath::IsNearlyEqual(Amount, TargetAmount))
		{
			FadeSpeed = 0.f;
		}
	}
	Fogs.RemoveAll([](const TWeakObjectPtr<AOnyxFog>& Weak) { return !Weak.IsValid(); });
	Clearers.RemoveAll([](const TWeakObjectPtr<UOnyxFogClearerComponent>& Weak) { return !Weak.IsValid(); });
}

TStatId UOnyxFogSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxFogSubsystem, STATGROUP_Tickables);
}

bool UOnyxFogSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxParallaxBackground.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/OnyxParallaxComponent.h"

namespace
{
	const TCHAR* PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
}

AOnyxParallaxBackground::AOnyxParallaxBackground()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Onyx2D/Materials/M_OnyxBackground.M_OnyxBackground")));

	FOnyxParallaxLayer Sky;
	Sky.Name = TEXT("Sky");
	Sky.Tint = FLinearColor(0.035f, 0.045f, 0.08f);
	Sky.Factor = FVector2D(1.f, 1.f);
	Sky.Depth = 2000.f;
	Sky.Height = 6000.f;
	BackgroundLayers.Add(Sky);
}

void AOnyxParallaxBackground::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UMaterialInterface* BaseMaterial = Material.LoadSynchronous();
	for (int32 Index = 0; Index < BackgroundLayers.Num(); ++Index)
	{
		if (BackgroundLayers[Index].bVisible)
		{
			BuildLayer(BackgroundLayers[Index], Index, BaseMaterial);
		}
	}
}

void AOnyxParallaxBackground::BuildLayer(const FOnyxParallaxLayer& Layer, int32 Index, UMaterialInterface* BaseMaterial)
{
	static UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, PlaneMeshPath);
	if (!Plane)
	{
		return;
	}

	// Components made here are owned by the construction script, so the engine
	// destroys and rebuilds them every time a property changes.
	UOnyxParallaxComponent* Pivot = NewObject<UOnyxParallaxComponent>(this, NAME_None, RF_Transactional);
	Pivot->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Pivot->Factor = Layer.Factor;
	Pivot->SetupAttachment(RootComponent);
	Pivot->SetRelativeLocation(FVector(Layer.OffsetX, -Layer.Depth, Layer.OffsetZ));
	Pivot->RegisterComponent();

	UStaticMeshComponent* Quad = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transactional);
	Quad->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Quad->SetStaticMesh(Plane);
	Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quad->SetCastShadow(false);
	Quad->SetMobility(EComponentMobility::Movable);
	Quad->SetupAttachment(Pivot);
	// The engine plane is 100x100 on XY facing +Z; stand it up so it faces the camera (+Y). Extending below makes it taller
	// at the bottom only: the picture keeps its place and size, its last row fills the extra part. A picture placed once
	// is as wide as its shape says; a repeating one (or a colour, a mist) spans Total Width.
	// The picture's shape from its imported size: in the editor the texture may still be compiling when the level loads,
	// and its GPU size is a placeholder until then (layers built from it came out squashed, and Play in Editor copied them).
	const FIntPoint Pixels = Layer.Texture ? Layer.Texture->GetImportedSize() : FIntPoint::ZeroValue;
	const float Aspect = Pixels.X > 0 && Pixels.Y > 0 ? float(Pixels.X) / Pixels.Y
		: (Layer.Texture && Layer.Texture->GetSizeY() > 0 ? float(Layer.Texture->GetSizeX()) / Layer.Texture->GetSizeY() : 1.f);
	const bool bOnce = !Layer.bRepeat && Layer.Texture && Layer.Material.IsNull();
	const float Width = bOnce ? Layer.Height * Aspect : TotalWidth;
	const float FullHeight = Layer.Height + Layer.ExtendBelow;
	Quad->SetRelativeLocation(FVector(0.f, 0.f, -Layer.ExtendBelow * 0.5f));
	Quad->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	Quad->SetRelativeScale3D(FVector(Width / 100.f, FullHeight / 100.f, 1.f));
	Quad->TranslucencySortPriority = Index;

	UMaterialInterface* LayerMaterial = Layer.Material.IsNull() ? BaseMaterial : Layer.Material.LoadSynchronous();
	if (LayerMaterial)
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(LayerMaterial, Quad);
		Instance->SetVectorParameterValue(TEXT("Tint"), Layer.Tint);
		// One repeat per layer height (times the texture's aspect): textures keep their shape, procedural layers their scale.
		Instance->SetScalarParameterValue(TEXT("TilingX"), Width / FMath::Max(Layer.Height * Aspect, 1.f));
		Instance->SetScalarParameterValue(TEXT("TilingY"), FullHeight / FMath::Max(Layer.Height, 1.f));
		if (Layer.Texture)
		{
			Instance->SetTextureParameterValue(TEXT("Texture"), Layer.Texture);
		}
		Quad->SetMaterial(0, Instance);
	}
	Quad->RegisterComponent();
}

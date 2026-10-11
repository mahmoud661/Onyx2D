// Copyright Onyx2D. All Rights Reserved.

#include "Core/Onyx2DLibrary.h"
#include "Components/MeshComponent.h"
#include "Core/Onyx2DSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

void UOnyx2DLibrary::ConfigureSpriteLighting(UMeshComponent* SpriteComponent, bool bCastShadows)
{
	if (!SpriteComponent)
	{
		return;
	}
	if (UMaterialInterface* LitMaterial = UOnyx2DSettings::Get()->LitSpriteMaterial.LoadSynchronous())
	{
		SpriteComponent->SetMaterial(0, LitMaterial);
	}
	SpriteComponent->SetCastShadow(bCastShadows);
}

void UOnyx2DLibrary::ApplySolidColor(UMeshComponent* Mesh, FLinearColor Color)
{
	if (!Mesh)
	{
		return;
	}
	if (UMaterialInterface* Base = UOnyx2DSettings::Get()->SolidColorMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Mesh);
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		for (int32 Index = 0; Index < FMath::Max(Mesh->GetNumMaterials(), 1); ++Index)
		{
			Mesh->SetMaterial(Index, Instance);
		}
	}
}

void UOnyx2DLibrary::ApplyGlowColor(UMeshComponent* Mesh, FLinearColor Color, float Intensity)
{
	UMaterialInterface* Base = Mesh ? UOnyx2DSettings::Get()->GlowMaterial.LoadSynchronous() : nullptr;
	if (!Base)
	{
		ApplySolidColor(Mesh, Color);
		return;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Mesh);
	Instance->SetVectorParameterValue(TEXT("Color"), Color);
	Instance->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	for (int32 Index = 0; Index < FMath::Max(Mesh->GetNumMaterials(), 1); ++Index)
	{
		Mesh->SetMaterial(Index, Instance);
	}
}

FVector UOnyx2DLibrary::SnapToPixelGrid(const FVector& Location)
{
	const float UnitsPerPixel = 1.f / FMath::Max(UOnyx2DSettings::Get()->PixelsPerUnit, KINDA_SMALL_NUMBER);
	return FVector(
		FMath::GridSnap(Location.X, UnitsPerPixel),
		Location.Y,
		FMath::GridSnap(Location.Z, UnitsPerPixel));
}

EOnyxFacing UOnyx2DLibrary::FacingFromDirection(float DirectionX, EOnyxFacing Current)
{
	if (FMath::Abs(DirectionX) < KINDA_SMALL_NUMBER)
	{
		return Current;
	}
	return DirectionX > 0.f ? EOnyxFacing::Right : EOnyxFacing::Left;
}

void UOnyx2DLibrary::CenterFlipbook(UPaperFlipbookComponent* Flipbook)
{
	const UPaperFlipbook* Asset = Flipbook ? Flipbook->GetFlipbook() : nullptr;
	if (!Asset || Asset->GetNumFrames() == 0)
	{
		return;
	}
	const FVector Center = Asset->GetRenderBounds().Origin * Flipbook->GetRelativeScale3D();
	Flipbook->SetRelativeLocation(FVector(-Center.X, Flipbook->GetRelativeLocation().Y, -Center.Z));
}

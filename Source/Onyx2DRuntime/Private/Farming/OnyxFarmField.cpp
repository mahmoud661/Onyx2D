// Copyright Onyx2D. All Rights Reserved.

#include "Farming/OnyxFarmField.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "Core/Onyx2DSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Farming/OnyxAutotileSet.h"
#include "Farming/OnyxCrop.h"
#include "Items/OnyxInventoryComponent.h"
#include "Materials/MaterialInterface.h"
#include "PaperGroupedSpriteComponent.h"
#include "PaperSprite.h"
#include "Time/OnyxCalendarSubsystem.h"
#include "World/OnyxFlipbookEffect.h"

namespace OnyxFarmFieldPrivate
{
	const FName FieldLayerTag(TEXT("Onyx.Layer.Background"));
	const FName CropRowNoFadeTag(TEXT("Onyx.NoSeeThrough"));

	UMaterialInterface* LitMaterial()
	{
		return UOnyx2DSettings::Get()->LitSpriteMaterial.LoadSynchronous();
	}

	uint32 CellHash(int32 X, int32 Y)
	{
		uint32 H = uint32(X) * 73856093u ^ uint32(Y) * 19349663u;
		return H ^ (H >> 13);
	}
}

AOnyxFarmField::AOnyxFarmField()
{
	PrimaryActorTick.bCanEverTick = false;
	Soil = CreateDefaultSubobject<UPaperGroupedSpriteComponent>(TEXT("Soil"));
	Soil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Soil->SetCastShadow(false);
	// Laid on the ground (behind everything standing), over the ground's pictures.
	Soil->TranslucencySortPriority = 2;
	RootComponent = Soil;
	Tags.Add(OnyxFarmFieldPrivate::FieldLayerTag);
}

void AOnyxFarmField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureCells();
	RefreshSoil();
}

void AOnyxFarmField::BeginPlay()
{
	Super::BeginPlay();
	EnsureCells();
	if (UOnyxCalendarSubsystem* Calendar = UOnyxCalendarSubsystem::Get(this))
	{
		Calendar->OnNewDay.AddUniqueDynamic(this, &AOnyxFarmField::HandleNewDay);
	}
	if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
	{
		Save->Register(this);
	}
	RefreshSoil();
	RefreshCrops();
}

void AOnyxFarmField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOnyxSaveSubsystem* Save = UOnyxSaveSubsystem::Get(this))
	{
		Save->Unregister(this);
	}
	for (AActor* Row : CropRows)
	{
		if (IsValid(Row))
		{
			Row->Destroy();
		}
	}
	CropRows.Reset();
	Super::EndPlay(EndPlayReason);
}

void AOnyxFarmField::HandleNewDay(int32 Day)
{
	AdvanceDay();
}

void AOnyxFarmField::EnsureCells()
{
	Width = FMath::Clamp(Width, 1, 256);
	Height = FMath::Clamp(Height, 1, 256);
	if (Cells.Num() != Width * Height)
	{
		Cells.SetNum(Width * Height);
	}
}

FOnyxSoilCell* AOnyxFarmField::At(FIntPoint Cell)
{
	return IsInField(Cell) && Cells.IsValidIndex(Cell.Y * Width + Cell.X) ? &Cells[Cell.Y * Width + Cell.X] : nullptr;
}

const FOnyxSoilCell* AOnyxFarmField::At(FIntPoint Cell) const
{
	return IsInField(Cell) && Cells.IsValidIndex(Cell.Y * Width + Cell.X) ? &Cells[Cell.Y * Width + Cell.X] : nullptr;
}

AOnyxFarmField* AOnyxFarmField::FindAt(const UWorld* World, const FVector& Location, FIntPoint& OutCell)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AOnyxFarmField> It(World); It; ++It)
	{
		if (It->WorldToCell(Location, OutCell))
		{
			return *It;
		}
	}
	return nullptr;
}

bool AOnyxFarmField::WorldToCell(const FVector& Location, FIntPoint& OutCell) const
{
	const FVector Corner = GetActorLocation();
	OutCell = FIntPoint(FMath::FloorToInt((Location.X - Corner.X) / CellSize), FMath::FloorToInt((Corner.Z - Location.Z) / CellSize));
	return IsInField(OutCell);
}

FVector AOnyxFarmField::GetCellCenter(FIntPoint Cell) const
{
	const FVector Corner = GetActorLocation();
	return FVector(Corner.X + (Cell.X + 0.5f) * CellSize, Corner.Y, Corner.Z - (Cell.Y + 0.5f) * CellSize);
}

FOnyxSoilCell AOnyxFarmField::GetCell(FIntPoint Cell) const
{
	const FOnyxSoilCell* Found = At(Cell);
	return Found ? *Found : FOnyxSoilCell();
}

bool AOnyxFarmField::IsReady(FIntPoint Cell) const
{
	const FOnyxSoilCell* Found = At(Cell);
	return Found && Found->Crop && Found->Stage >= Found->Crop->GetReadyStage();
}

bool AOnyxFarmField::CanTill(FIntPoint Cell) const
{
	if (!IsInField(Cell))
	{
		return false;
	}
	const int32 Index = Cell.Y * Width + Cell.X;
	return Tillable.IsEmpty() || (Tillable.IsValidIndex(Index) && Tillable[Index] != 0);
}

bool AOnyxFarmField::Till(FIntPoint Cell)
{
	FOnyxSoilCell* Found = At(Cell);
	if (!Found || Found->bTilled || !CanTill(Cell))
	{
		return false;
	}
	Found->bTilled = true;
	RefreshSoil();
	Feedback(Cell, TillEffect, TillSound);
	return true;
}

bool AOnyxFarmField::Water(FIntPoint Cell)
{
	FOnyxSoilCell* Found = At(Cell);
	if (!Found || !Found->bTilled || Found->bWatered)
	{
		return false;
	}
	Found->bWatered = true;
	RefreshSoil();
	Feedback(Cell, WaterEffect, WaterSound);
	return true;
}

bool AOnyxFarmField::Plant(FIntPoint Cell, UOnyxCrop* Crop)
{
	FOnyxSoilCell* Found = At(Cell);
	if (!Found || !Crop || !Found->bTilled || Found->Crop)
	{
		return false;
	}
	Found->Crop = Crop;
	Found->Stage = 0;
	Found->Days = 0;
	RefreshCrops();
	Feedback(Cell, nullptr, TillSound);
	return true;
}

bool AOnyxFarmField::Harvest(FIntPoint Cell, UOnyxInventoryComponent* Into)
{
	FOnyxSoilCell* Found = At(Cell);
	if (!Found || !IsReady(Cell))
	{
		return false;
	}
	UOnyxCrop* Crop = Found->Crop;
	if (Into && Crop->Produce)
	{
		// A full backpack keeps the crop in the ground.
		if (Into->AddItem(Crop->Produce, Crop->ProduceCount) > 0)
		{
			return false;
		}
	}
	if (Crop->RegrowDays > 0 && Crop->Stages.Num() > 1)
	{
		Found->Stage = Crop->GetReadyStage() - 1;
		Found->Days = FMath::Max(Crop->GetDaysForStage(Found->Stage) - Crop->RegrowDays, 0);
	}
	else
	{
		Found->Crop = nullptr;
		Found->Stage = 0;
		Found->Days = 0;
	}
	RefreshCrops();
	Feedback(Cell, HarvestEffect, HarvestSound);
	return true;
}

void AOnyxFarmField::AdvanceDay()
{
	for (FOnyxSoilCell& Cell : Cells)
	{
		if (Cell.Crop && Cell.bWatered && Cell.Stage < Cell.Crop->GetReadyStage())
		{
			if (++Cell.Days >= Cell.Crop->GetDaysForStage(Cell.Stage))
			{
				++Cell.Stage;
				Cell.Days = 0;
			}
		}
		Cell.bWatered = false;
	}
	RefreshSoil();
	RefreshCrops();
}

void AOnyxFarmField::WaterAll()
{
	for (FOnyxSoilCell& Cell : Cells)
	{
		Cell.bWatered = Cell.bTilled;
	}
	RefreshSoil();
}

void AOnyxFarmField::RefreshSoil()
{
	if (!Soil)
	{
		return;
	}
	Soil->ClearInstances();
	UMaterialInterface* Material = OnyxFarmFieldPrivate::LitMaterial();
	auto IsTilled = [this](int32 X, int32 Y) { const FOnyxSoilCell* Cell = At(FIntPoint(X, Y)); return Cell && Cell->bTilled; };
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FOnyxSoilCell* Cell = At(FIntPoint(X, Y));
			const UOnyxAutotileSet* Set = Cell && Cell->bTilled ? (Cell->bWatered && WetSoil ? WetSoil.Get() : DrySoil.Get()) : nullptr;
			UPaperSprite* Tile = Set ? Set->GetTile(OnyxAutotile::MaskAt(X, Y, IsTilled), OnyxFarmFieldPrivate::CellHash(X, Y)) : nullptr;
			if (Tile)
			{
				// Tiles stand on their bottom edge: the cell's lower edge, in the middle.
				Soil->AddInstanceWithMaterial(FTransform(FVector((X + 0.5f) * CellSize, 0.f, -(Y + 1.f) * CellSize)), Tile, Material);
			}
		}
	}
}

void AOnyxFarmField::RefreshCrops()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	CropRows.SetNum(Height);
	UMaterialInterface* Material = OnyxFarmFieldPrivate::LitMaterial();
	for (int32 Y = 0; Y < Height; ++Y)
	{
		TObjectPtr<AActor>& Row = CropRows[Y];
		UPaperGroupedSpriteComponent* Sprites = Row ? Row->FindComponentByClass<UPaperGroupedSpriteComponent>() : nullptr;
		bool bAny = false;
		for (int32 X = 0; X < Width && !bAny; ++X)
		{
			const FOnyxSoilCell* Cell = At(FIntPoint(X, Y));
			bAny = Cell && Cell->Crop && Cell->Crop->Stages.IsValidIndex(Cell->Stage);
		}
		if (!bAny && !Row)
		{
			continue;
		}
		if (!Row)
		{
			// The row's actor stands on the row's lower edge, so it sorts against characters by its own feet.
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.ObjectFlags |= RF_Transient;
			Row = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(GetActorLocation() + FVector(0.f, 0.f, -(Y + 1.f) * CellSize)), Params);
			Sprites = NewObject<UPaperGroupedSpriteComponent>(Row, TEXT("Crops"));
			Sprites->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Sprites->SetCastShadow(false);
			Sprites->SetMobility(EComponentMobility::Movable);
			Row->SetRootComponent(Sprites);
			Sprites->RegisterComponent();
			Sprites->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, -(Y + 1.f) * CellSize));
			Row->Tags.Add(OnyxFarmFieldPrivate::CropRowNoFadeTag);
		}
		Sprites->ClearInstances();
		for (int32 X = 0; X < Width; ++X)
		{
			const FOnyxSoilCell* Cell = At(FIntPoint(X, Y));
			if (Cell && Cell->Crop && Cell->Crop->Stages.IsValidIndex(Cell->Stage) && Cell->Crop->Stages[Cell->Stage])
			{
				Sprites->AddInstanceWithMaterial(FTransform(FVector((X + 0.5f) * CellSize, 0.f, 0.f)), Cell->Crop->Stages[Cell->Stage], Material);
			}
		}
		Sprites->MarkRenderStateDirty();
	}
}

void AOnyxFarmField::Feedback(FIntPoint Cell, UPaperFlipbook* Effect, USoundBase* Sound) const
{
	const FVector Where = GetCellCenter(Cell) + FVector(0.f, 2.f, 0.f);
	if (Effect)
	{
		AOnyxFlipbookEffect::Spawn(const_cast<AOnyxFarmField*>(this), Effect, Where);
	}
	if (Sound)
	{
		UOnyxAudioSubsystem::PlaySoundAt(this, Sound, Where);
	}
}

FString AOnyxFarmField::GetSaveKey() const
{
	return TEXT("Onyx.Field.") + GetName();
}

void AOnyxFarmField::WriteSave(FArchive& Archive)
{
	int32 Count = Cells.Num();
	Archive << Count;
	for (FOnyxSoilCell& Cell : Cells)
	{
		FString CropPath = Cell.Crop ? Cell.Crop->GetPathName() : FString();
		Archive << Cell.bTilled << Cell.bWatered << CropPath << Cell.Stage << Cell.Days;
	}
}

void AOnyxFarmField::ReadSave(FArchive& Archive)
{
	int32 Count = 0;
	Archive << Count;
	TArray<FOnyxSoilCell> Read;
	Read.SetNum(FMath::Max(Count, 0));
	for (FOnyxSoilCell& Cell : Read)
	{
		FString CropPath;
		Archive << Cell.bTilled << Cell.bWatered << CropPath << Cell.Stage << Cell.Days;
		Cell.Crop = CropPath.IsEmpty() ? nullptr : LoadObject<UOnyxCrop>(nullptr, *CropPath);
	}
	// A save from a field of another size is ignored rather than scrambled.
	if (Read.Num() == Width * Height)
	{
		Cells = MoveTemp(Read);
	}
	RefreshSoil();
	RefreshCrops();
}

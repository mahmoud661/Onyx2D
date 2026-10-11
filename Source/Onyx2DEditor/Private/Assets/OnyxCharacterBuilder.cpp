// Copyright Onyx2D. All Rights Reserved.

#include "Assets/OnyxCharacterBuilder.h"
#include "Animation/OnyxAnimatorComponent.h"
#include "Assets/OnyxAssetFactory.h"
#include "Assets/OnyxImageAnalysis.h"
#include "Camera/OnyxCameraComponent.h"
#include "Character/OnyxEnemyCharacter.h"
#include "Character/OnyxPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"

TArray<FOnyxAnimationSpec> FOnyxCharacterSpec::DefaultAnimations()
{
	struct FPreset
	{
		const TCHAR* Name;
		float Fps;
		bool bLoop;
		bool bEnabled;
	};
	const FPreset Presets[] = {
		{ TEXT("Idle"), 8.f, true, true },
		{ TEXT("Run"), 12.f, true, true },
		{ TEXT("Jump"), 10.f, false, true },
		{ TEXT("Fall"), 10.f, true, true },
		{ TEXT("Attack1"), 16.f, false, true },
		{ TEXT("Attack2"), 16.f, false, false },
		{ TEXT("Attack3"), 16.f, false, false },
		{ TEXT("Hurt"), 12.f, false, true },
		{ TEXT("Death"), 10.f, false, true },
		{ TEXT("Dash"), 14.f, false, false },
		{ TEXT("WallSlide"), 8.f, true, false },
	};

	TArray<FOnyxAnimationSpec> Result;
	int32 Row = 0;
	for (const FPreset& Preset : Presets)
	{
		FOnyxAnimationSpec Spec;
		Spec.Name = Preset.Name;
		Spec.FramesPerSecond = Preset.Fps;
		Spec.bLoop = Preset.bLoop;
		Spec.bEnabled = Preset.bEnabled;
		Spec.Row = Row++;
		Result.Add(Spec);
	}
	return Result;
}

UTexture2D* FOnyxCharacterBuilder::ResolveTexture(const FOnyxCharacterSpec& Spec, const FOnyxAnimationSpec& Animation)
{
	return Animation.TextureOverride.IsValid() ? Animation.TextureOverride.Get() : Spec.SpriteSheet.Get();
}

namespace
{
	/** Per-texture alpha mask (true = has visible pixels in that cell), cached by frame size. */
	struct FCellOccupancy
	{
		TWeakObjectPtr<const UTexture2D> Texture;
		FIntPoint FrameSize = FIntPoint::ZeroValue;
		FGuid SourceId;
		TArray<bool> Occupied;
		int32 Columns = 0;
	};

	const FCellOccupancy* GetOccupancy(const UTexture2D* Texture, FIntPoint FrameSize)
	{
		static TArray<FCellOccupancy> Cache;
		const FGuid SourceId = Texture->Source.GetId();
		for (const FCellOccupancy& Entry : Cache)
		{
			if (Entry.Texture.Get() == Texture && Entry.FrameSize == FrameSize && Entry.SourceId == SourceId)
			{
				return &Entry;
			}
		}

		FTextureSource& Source = const_cast<UTexture2D*>(Texture)->Source;
		if (!Source.IsValid() || Source.GetFormat() != TSF_BGRA8)
		{
			return nullptr; // unknown layout: fall back to "every column"
		}
		const int32 Width = Source.GetSizeX();
		const int32 Height = Source.GetSizeY();
		const uint8* Pixels = Source.LockMipReadOnly(0);
		if (!Pixels)
		{
			return nullptr;
		}

		FCellOccupancy Entry;
		Entry.Texture = Texture;
		Entry.FrameSize = FrameSize;
		Entry.SourceId = SourceId;
		Entry.Columns = Width / FrameSize.X;
		const int32 Rows = Height / FrameSize.Y;
		Entry.Occupied.Init(false, Entry.Columns * Rows);
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Entry.Columns; ++Column)
			{
				bool bVisible = false;
				for (int32 Y = Row * FrameSize.Y; Y < (Row + 1) * FrameSize.Y && !bVisible; ++Y)
				{
					for (int32 X = Column * FrameSize.X; X < (Column + 1) * FrameSize.X; ++X)
					{
						if (Pixels[(Y * Width + X) * 4 + 3] > 8)
						{
							bVisible = true;
							break;
						}
					}
				}
				Entry.Occupied[Row * Entry.Columns + Column] = bVisible;
			}
		}
		Source.UnlockMip(0);

		if (Cache.Num() > 32)
		{
			Cache.RemoveAt(0);
		}
		Cache.Add(MoveTemp(Entry));
		return &Cache.Last();
	}
}

int32 FOnyxCharacterBuilder::ResolveFrameCount(const FOnyxCharacterSpec& Spec, const FOnyxAnimationSpec& Animation)
{
	const UTexture2D* Texture = ResolveTexture(Spec, Animation);
	if (!Texture || Spec.FrameSize.X <= 0 || Spec.FrameSize.Y <= 0)
	{
		return 0;
	}
	const int32 Columns = FMath::Max(Texture->GetImportedSize().X / Spec.FrameSize.X, 0);
	const int32 Available = FMath::Max(Columns - Animation.StartColumn, 0);
	if (Animation.FrameCount > 0)
	{
		return FMath::Min(Animation.FrameCount, Available);
	}

	// Auto: stop at the first empty cell of the row.
	if (const FCellOccupancy* Occupancy = GetOccupancy(Texture, Spec.FrameSize))
	{
		int32 Count = 0;
		while (Count < Available)
		{
			const int32 Cell = Animation.Row * Occupancy->Columns + Animation.StartColumn + Count;
			if (!Occupancy->Occupied.IsValidIndex(Cell) || !Occupancy->Occupied[Cell])
			{
				break;
			}
			++Count;
		}
		return Count;
	}
	return Available;
}

TArray<FIntRect> FOnyxCharacterBuilder::GetFrameRects(const FOnyxCharacterSpec& Spec, const FOnyxAnimationSpec& Animation)
{
	TArray<FIntRect> Rects;
	const UTexture2D* Texture = ResolveTexture(Spec, Animation);
	if (!Texture || Spec.FrameSize.X <= 0 || Spec.FrameSize.Y <= 0)
	{
		return Rects;
	}
	const FIntPoint TextureSize = Texture->GetImportedSize();
	const int32 Count = ResolveFrameCount(Spec, Animation);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FIntPoint Min((Animation.StartColumn + Index) * Spec.FrameSize.X, Animation.Row * Spec.FrameSize.Y);
		const FIntPoint Max = Min + Spec.FrameSize;
		if (Max.X > TextureSize.X || Max.Y > TextureSize.Y)
		{
			break;
		}
		Rects.Add(FIntRect(Min, Max));
	}
	return Rects;
}

void FOnyxCharacterBuilder::FitCapsule(const FOnyxCharacterSpec& Spec, AOnyxCharacter* Defaults, int32& OutPixelsBelowFeet, FOnyxCharacterBuildResult& Result)
{
	const FOnyxAnimationSpec* Reference = Spec.Animations.FindByPredicate([](const FOnyxAnimationSpec& A) { return A.bEnabled && A.Name == TEXT("Idle"); });
	Reference = Reference ? Reference : Spec.Animations.FindByPredicate([](const FOnyxAnimationSpec& A) { return A.bEnabled; });
	UTexture2D* Texture = Reference ? ResolveTexture(Spec, *Reference) : nullptr;
	const TOptional<FIntRect> Bounds = Texture ? FOnyxImageAnalysis::VisibleBounds(Texture, GetFrameRects(Spec, *Reference)) : TOptional<FIntRect>();
	if (!Bounds.IsSet())
	{
		Result.Warnings.Add(TEXT("Auto fit capsule: could not read the visible pixels, kept the default capsule."));
		return;
	}

	const float Scale = Spec.CentimetersPerPixel;
	const float HalfHeight = FMath::Max(Bounds->Height() * Scale * 0.5f, 10.f);
	const float Radius = FMath::Clamp(Bounds->Width() * Scale * 0.35f, 8.f, HalfHeight);
	UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent();
	Capsule->Modify();
	Capsule->SetCapsuleSize(Radius, HalfHeight);
	OutPixelsBelowFeet = Spec.FrameSize.Y - Bounds->Max.Y;
	Result.CapsuleSize = FVector2D(Radius, HalfHeight);
}

FOnyxCharacterBuildResult FOnyxCharacterBuilder::Build(const FOnyxCharacterSpec& Spec)
{
	FOnyxCharacterBuildResult Result;
	const FString Name = FOnyxAssetFactory::SanitizeName(Spec.Name);
	const FString Root = Spec.Folder / Name;

	if (!FOnyxAssetFactory::IsValidContentFolder(Spec.Folder))
	{
		Result.Error = INVTEXT("The folder must start with /Game.");
		return Result;
	}
	if (Spec.FrameSize.X <= 0 || Spec.FrameSize.Y <= 0)
	{
		Result.Error = INVTEXT("Frame width and height must be greater than zero.");
		return Result;
	}

	TArray<UPackage*> Packages;
	TMap<UTexture2D*, UTexture2D*> NormalMaps;
	TMap<FName, FOnyxAnimation> Animations;
	UPaperFlipbook* IdleFlipbook = nullptr;
	UPaperFlipbook* FirstFlipbook = nullptr;
	const float PixelsPerUnit = 1.f / FMath::Max(Spec.CentimetersPerPixel, 0.01f);

	for (const FOnyxAnimationSpec& Animation : Spec.Animations)
	{
		if (!Animation.bEnabled || Animation.Name.IsNone())
		{
			continue;
		}
		UTexture2D* Texture = ResolveTexture(Spec, Animation);
		if (!Texture)
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s: no texture, skipped."), *Animation.Name.ToString()));
			continue;
		}
		FOnyxAssetFactory::ConfigurePixelArtTexture(Texture);
		Packages.Add(Texture->GetPackage());

		const TArray<FIntRect> Rects = GetFrameRects(Spec, Animation);
		if (Rects.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s: row %d has no frames inside the texture, skipped."), *Animation.Name.ToString(), Animation.Row));
			continue;
		}

		// One normal map per sheet, shared by every frame cut from it.
		UTexture2D* NormalMap = nullptr;
		if (Spec.bGenerateNormalMap)
		{
			UTexture2D*& Cached = NormalMaps.FindOrAdd(Texture);
			if (!Cached)
			{
				Cached = FOnyxNormalMaps::CreateFor(Texture, Spec.NormalMap);
				if (Cached)
				{
					Packages.Add(Cached->GetPackage());
				}
			}
			NormalMap = Cached;
		}

		TArray<UPaperSprite*> Sprites;
		for (int32 Index = 0; Index < Rects.Num(); ++Index)
		{
			const FString SpriteName = FString::Printf(TEXT("S_%s_%s_%02d"), *Name, *Animation.Name.ToString(), Index);
			if (UPaperSprite* Sprite = FOnyxAssetFactory::CreateSprite(Root / TEXT("Sprites"), SpriteName, Texture,
				Rects[Index].Min, Rects[Index].Size(), PixelsPerUnit, ESpritePivotMode::Bottom_Center))
			{
				if (NormalMap)
				{
					FOnyxNormalMaps::Assign(Sprite, NormalMap);
				}
				Sprites.Add(Sprite);
				Packages.Add(Sprite->GetPackage());
			}
		}

		UPaperFlipbook* Flipbook = FOnyxAssetFactory::CreateFlipbook(Root / TEXT("Animations"),
			FString::Printf(TEXT("FB_%s_%s"), *Name, *Animation.Name.ToString()), Sprites, Animation.FramesPerSecond);
		if (!Flipbook)
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s: could not create the flipbook (name in use by another asset type)."), *Animation.Name.ToString()));
			continue;
		}
		Packages.Add(Flipbook->GetPackage());

		FOnyxAnimation Entry;
		Entry.Flipbook = Flipbook;
		Entry.bLoop = Animation.bLoop;
		Animations.Add(Animation.Name, Entry);

		Result.SpriteCount += Sprites.Num();
		++Result.FlipbookCount;
		FirstFlipbook = FirstFlipbook ? FirstFlipbook : Flipbook;
		if (Animation.Name == TEXT("Idle"))
		{
			IdleFlipbook = Flipbook;
		}
	}

	if (Animations.IsEmpty())
	{
		Result.Error = INVTEXT("No animation produced any frames. Check the sprite sheet, frame size and rows.");
		return Result;
	}

	UClass* ParentClass = Spec.Kind == EOnyxCharacterKind::Player ? AOnyxPlayerCharacter::StaticClass() : AOnyxEnemyCharacter::StaticClass();
	UBlueprint* Blueprint = FOnyxAssetFactory::CreateBlueprint(Root, TEXT("BP_") + Name, ParentClass);
	AOnyxCharacter* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<AOnyxCharacter>() : nullptr;
	if (!Defaults)
	{
		Result.Error = INVTEXT("Could not create the character Blueprint (the name may be used by another asset).");
		return Result;
	}

	Defaults->Modify();
	Defaults->Animator->Modify();
	for (const TPair<FName, FOnyxAnimation>& Pair : Animations)
	{
		Defaults->Animator->Animations.Add(Pair.Key, Pair.Value);
	}

	int32 PixelsBelowFeet = Spec.PixelsBelowFeet;
	if (Spec.bAutoFitCapsule)
	{
		FitCapsule(Spec, Defaults, PixelsBelowFeet, Result);
	}

	if (UPaperFlipbookComponent* SpriteComponent = Defaults->GetSprite())
	{
		SpriteComponent->Modify();
		SpriteComponent->SetFlipbook(IdleFlipbook ? IdleFlipbook : FirstFlipbook);
		const float HalfHeight = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		SpriteComponent->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight - PixelsBelowFeet * Spec.CentimetersPerPixel));
	}

	// The player's camera keeps every art pixel whole on screen at this character's pixel size (no shimmer, no seams).
	if (AOnyxPlayerCharacter* Player = Cast<AOnyxPlayerCharacter>(Defaults); Player && Player->Camera)
	{
		Player->Camera->Modify();
		Player->Camera->Settings.bPixelPerfect = true;
		Player->Camera->Settings.ArtPixelSize = Spec.CentimetersPerPixel;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Packages.Add(Blueprint->GetPackage());
	FOnyxAssetFactory::SavePackages(Packages);

	Result.Blueprint = Blueprint;
	return Result;
}

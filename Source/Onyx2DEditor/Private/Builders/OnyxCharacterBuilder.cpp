// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxCharacterBuilder.h"
#include "Misc/PackageName.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxImageAnalysis.h"
#include "Camera/OnyxCameraComponent.h"
#include "AI/OnyxPatrolComponent.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "Movement/OnyxMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"

TArray<FOnyxClipSpec> FOnyxCharacterSpec::DefaultClips()
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

	TArray<FOnyxClipSpec> Result;
	int32 Row = 0;
	for (const FPreset& Preset : Presets)
	{
		FOnyxClipSpec Spec;
		Spec.Name = Preset.Name;
		Spec.FramesPerSecond = Preset.Fps;
		Spec.bLoop = Preset.bLoop;
		Spec.bEnabled = Preset.bEnabled;
		Spec.Row = Row++;
		Result.Add(Spec);
	}
	return Result;
}

FOnyxCharacterSpec FOnyxCharacterSpec::FromStrips(const TMap<FName, UTexture2D*>& Strips, const FString& Name, const FString& Folder, FIntPoint FrameSize,
	int32 Behaviours, uint8 Team, float CentimetersPerPixel, const TMap<FName, float>& FramesPerSecond)
{
	FOnyxCharacterSpec Spec;
	Spec.Name = Name;
	Spec.Folder = Folder;
	Spec.FrameSize = FrameSize;
	Spec.CentimetersPerPixel = CentimetersPerPixel;
	Spec.Behaviours = Behaviours;
	Spec.Team = Team;
	Spec.bAutoFitCapsule = true;

	const TArray<FOnyxClipSpec> Presets = DefaultClips();
	for (const TPair<FName, UTexture2D*>& Strip : Strips)
	{
		const FOnyxClipSpec* Preset = Presets.FindByPredicate([&](const FOnyxClipSpec& P) { return P.Name == Strip.Key; });
		FOnyxClipSpec Clip = Preset ? *Preset : FOnyxClipSpec();
		Clip.Name = Strip.Key;
		Clip.bEnabled = true;
		Clip.TextureOverride = Strip.Value;
		Clip.Row = 0;
		Clip.StartColumn = 0;
		Clip.FrameCount = 0;
		if (const float* Speed = FramesPerSecond.Find(Strip.Key))
		{
			Clip.FramesPerSecond = *Speed;
		}
		Spec.Clips.Add(Clip);
	}
	return Spec;
}

UTexture2D* FOnyxCharacterBuilder::ResolveTexture(const FOnyxCharacterSpec& Spec, const FOnyxClipSpec& ClipSpec)
{
	return ClipSpec.TextureOverride.IsValid() ? ClipSpec.TextureOverride.Get() : Spec.SpriteSheet.Get();
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

int32 FOnyxCharacterBuilder::ResolveFrameCount(const FOnyxCharacterSpec& Spec, const FOnyxClipSpec& ClipSpec)
{
	const UTexture2D* Texture = ResolveTexture(Spec, ClipSpec);
	if (!Texture || Spec.FrameSize.X <= 0 || Spec.FrameSize.Y <= 0)
	{
		return 0;
	}
	const int32 Columns = FMath::Max(Texture->GetImportedSize().X / Spec.FrameSize.X, 0);
	const int32 Available = FMath::Max(Columns - ClipSpec.StartColumn, 0);
	if (ClipSpec.FrameCount > 0)
	{
		return FMath::Min(ClipSpec.FrameCount, Available);
	}

	// Auto: stop at the first empty cell of the row.
	if (const FCellOccupancy* Occupancy = GetOccupancy(Texture, Spec.FrameSize))
	{
		int32 Count = 0;
		while (Count < Available)
		{
			const int32 Cell = ClipSpec.Row * Occupancy->Columns + ClipSpec.StartColumn + Count;
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

TArray<FIntRect> FOnyxCharacterBuilder::GetFrameRects(const FOnyxCharacterSpec& Spec, const FOnyxClipSpec& ClipSpec)
{
	TArray<FIntRect> Rects;
	const UTexture2D* Texture = ResolveTexture(Spec, ClipSpec);
	if (!Texture || Spec.FrameSize.X <= 0 || Spec.FrameSize.Y <= 0)
	{
		return Rects;
	}
	const FIntPoint TextureSize = Texture->GetImportedSize();
	const int32 Count = ResolveFrameCount(Spec, ClipSpec);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FIntPoint Min((ClipSpec.StartColumn + Index) * Spec.FrameSize.X, ClipSpec.Row * Spec.FrameSize.Y);
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
	const FOnyxClipSpec* Reference = Spec.Clips.FindByPredicate([](const FOnyxClipSpec& A) { return A.bEnabled && A.Name == TEXT("Idle"); });
	Reference = Reference ? Reference : Spec.Clips.FindByPredicate([](const FOnyxClipSpec& A) { return A.bEnabled; });
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
	const FString Name = FOnyxAssetUtils::SanitizeName(Spec.Name);
	const FString Root = Spec.Folder / Name;

	if (!FOnyxAssetUtils::IsValidContentFolder(Spec.Folder))
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
	TMap<FName, UOnyxClip*> Clips;
	UPaperFlipbook* IdleFlipbook = nullptr;
	UPaperFlipbook* FirstFlipbook = nullptr;
	const float PixelsPerUnit = 1.f / FMath::Max(Spec.CentimetersPerPixel, 0.01f);

	for (const FOnyxClipSpec& ClipSpec : Spec.Clips)
	{
		if (!ClipSpec.bEnabled || ClipSpec.Name.IsNone())
		{
			continue;
		}
		UTexture2D* Texture = ResolveTexture(Spec, ClipSpec);
		if (!Texture)
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s: no texture, skipped."), *ClipSpec.Name.ToString()));
			continue;
		}
		FOnyxAssetUtils::ConfigurePixelArtTexture(Texture);
		Packages.Add(Texture->GetPackage());

		const TArray<FIntRect> Rects = GetFrameRects(Spec, ClipSpec);
		if (Rects.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s: row %d has no frames inside the texture, skipped."), *ClipSpec.Name.ToString(), ClipSpec.Row));
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
			const FString SpriteName = FString::Printf(TEXT("S_%s_%s_%02d"), *Name, *ClipSpec.Name.ToString(), Index);
			if (UPaperSprite* Sprite = FOnyxAssetUtils::CreateSprite(Root / TEXT("Sprites"), SpriteName, Texture,
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

		UPaperFlipbook* Flipbook = FOnyxAssetUtils::CreateFlipbook(Root / TEXT("Clips"),
			FString::Printf(TEXT("FB_%s_%s"), *Name, *ClipSpec.Name.ToString()), Sprites, ClipSpec.FramesPerSecond);
		if (!Flipbook)
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s: could not create the flipbook (name in use by another asset type)."), *ClipSpec.Name.ToString()));
			continue;
		}
		Packages.Add(Flipbook->GetPackage());

		// The clip: frames + how it plays; events are added in the Moveset Editor's timeline.
		UOnyxClip* Clip = FOnyxAssetUtils::CreateOrReuse<UOnyxClip>(Root / TEXT("Clips"),
			FString::Printf(TEXT("CL_%s_%s"), *Name, *ClipSpec.Name.ToString()));
		if (!Clip)
		{
			Result.Warnings.Add(FString::Printf(TEXT("%s: could not create the Clip (name in use by another asset type)."), *ClipSpec.Name.ToString()));
			continue;
		}
		Clip->Modify();
		Clip->Flipbook = Flipbook;
		Clip->bLoop = ClipSpec.bLoop;
		Clip->MarkPackageDirty();
		Packages.Add(Clip->GetPackage());
		Clips.Add(ClipSpec.Name, Clip);

		Result.SpriteCount += Sprites.Num();
		++Result.FlipbookCount;
		FirstFlipbook = FirstFlipbook ? FirstFlipbook : Flipbook;
		if (ClipSpec.Name == TEXT("Idle"))
		{
			IdleFlipbook = Flipbook;
		}
	}

	if (Clips.IsEmpty())
	{
		Result.Error = INVTEXT("No clip produced any frames. Check the sprite sheet, frame size and rows.");
		return Result;
	}

	UOnyxMoveset* Moveset = FOnyxAssetUtils::CreateOrReuse<UOnyxMoveset>(Root, TEXT("MS_") + Name);
	if (!Moveset)
	{
		Result.Error = INVTEXT("Could not create the Moveset (the name may be used by another asset).");
		return Result;
	}
	for (const TPair<FName, UOnyxClip*>& Pair : Clips)
	{
		Moveset->SetClip(Pair.Key, Pair.Value);
	}
	Moveset->MarkPackageDirty();
	Packages.Add(Moveset->GetPackage());
	Result.Moveset = Moveset;

	UBlueprint* Blueprint = FOnyxAssetUtils::CreateBlueprint(Root, TEXT("BP_") + Name, AOnyxCharacter::StaticClass());
	AOnyxCharacter* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<AOnyxCharacter>() : nullptr;
	if (!Defaults)
	{
		Result.Error = INVTEXT("Could not create the character Blueprint (the name may be used by another asset).");
		return Result;
	}

	Defaults->Modify();
	Defaults->MovesetComponent->Modify();
	Defaults->MovesetComponent->Moveset = Moveset;
	Defaults->TeamId = Spec.Team;
	if (Spec.Behaviours & int32(EOnyxBehaviour::PlayerControl))
	{
		Defaults->PlaceholderColor = FLinearColor(0.2f, 0.85f, 0.9f);
	}
	if (Spec.Behaviours & int32(EOnyxBehaviour::Patrol))
	{
		// A patrolling foe: slower, no dash or wall moves, single jump.
		if (UOnyxMovementComponent* Movement = Defaults->GetOnyxMovement())
		{
			Movement->RunSpeed = 170.f;
			Movement->DashSettings.bEnabled = false;
			Movement->WallSettings.bWallSlide = false;
			Movement->WallSettings.bWallJump = false;
			Movement->JumpSettings.MaxJumps = 1;
		}
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

	// Behaviours are components; a camera keeps every art pixel whole on screen at this character's pixel size.
	FOnyxAssetUtils::AddBehaviourComponents(Blueprint, Spec.Behaviours);
	if (UOnyxCameraComponent* Camera = FOnyxAssetUtils::FindComponentTemplate<UOnyxCameraComponent>(Blueprint))
	{
		Camera->Modify();
		Camera->Settings.bPixelPerfect = true;
		Camera->Settings.ArtPixelSize = Spec.CentimetersPerPixel;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Packages.Add(Blueprint->GetPackage());
	FOnyxAssetUtils::SavePackages(Packages);

	Result.Blueprint = Blueprint;
	return Result;
}

UBlueprint* FOnyxCharacterBuilder::CreateFromMoveset(const FString& InName, const FString& Folder, UOnyxMoveset* Moveset, int32 Behaviours, uint8 Team,
	float CentimetersPerPixel, FText& OutError)
{
	const FString Name = FOnyxAssetUtils::SanitizeName(InName);
	if (!FOnyxAssetUtils::IsValidContentFolder(Folder))
	{
		OutError = INVTEXT("The folder must start with /Game.");
		return nullptr;
	}
	if (!Moveset || Moveset->Clips.IsEmpty())
	{
		OutError = INVTEXT("The Moveset has no clips yet: make at least Idle first.");
		return nullptr;
	}
	UBlueprint* Blueprint = FOnyxAssetUtils::CreateBlueprint(Folder / Name, TEXT("BP_") + Name, AOnyxCharacter::StaticClass());
	AOnyxCharacter* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<AOnyxCharacter>() : nullptr;
	if (!Defaults)
	{
		OutError = INVTEXT("Could not create the character Blueprint (the name may be used by another asset).");
		return nullptr;
	}

	Defaults->Modify();
	Defaults->MovesetComponent->Modify();
	Defaults->MovesetComponent->Moveset = Moveset;
	Defaults->TeamId = Team;
	if (Behaviours & int32(EOnyxBehaviour::PlayerControl))
	{
		Defaults->PlaceholderColor = FLinearColor(0.2f, 0.85f, 0.9f);
	}
	if (Behaviours & int32(EOnyxBehaviour::Patrol))
	{
		if (UOnyxMovementComponent* Movement = Defaults->GetOnyxMovement())
		{
			Movement->RunSpeed = 170.f;
			Movement->DashSettings.bEnabled = false;
			Movement->WallSettings.bWallSlide = false;
			Movement->WallSettings.bWallJump = false;
			Movement->JumpSettings.MaxJumps = 1;
		}
	}

	// Idle (or the first clip) shows in the editor; the capsule fits its visible pixels, feet on the ground.
	UOnyxClip* Idle = Moveset->FindClip(TEXT("Idle"));
	if (!Idle)
	{
		for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Moveset->Clips)
		{
			Idle = Idle ? Idle : Pair.Value.Get();
		}
	}
	UPaperFlipbook* IdleFlipbook = Idle ? Idle->Flipbook.Get() : nullptr;
	int32 PixelsBelowFeet = 0;
	if (IdleFlipbook && IdleFlipbook->GetNumKeyFrames() > 0)
	{
		const UPaperSprite* Sprite = IdleFlipbook->GetKeyFrameChecked(0).Sprite;
		UTexture2D* Texture = Sprite ? Sprite->GetSourceTexture() : nullptr;
		if (Texture)
		{
			const FIntPoint Min(FMath::RoundToInt(Sprite->GetSourceUV().X), FMath::RoundToInt(Sprite->GetSourceUV().Y));
			const FIntPoint Size(FMath::RoundToInt(Sprite->GetSourceSize().X), FMath::RoundToInt(Sprite->GetSourceSize().Y));
			if (const TOptional<FIntRect> Bounds = FOnyxImageAnalysis::VisibleBounds(Texture, { FIntRect(Min, Min + Size) }))
			{
				const float HalfHeight = FMath::Max(Bounds->Height() * CentimetersPerPixel * 0.5f, 10.f);
				const float Radius = FMath::Clamp(Bounds->Width() * CentimetersPerPixel * 0.35f, 8.f, HalfHeight);
				Defaults->GetCapsuleComponent()->Modify();
				Defaults->GetCapsuleComponent()->SetCapsuleSize(Radius, HalfHeight);
				PixelsBelowFeet = Size.Y - Bounds->Max.Y;
			}
		}
	}
	if (UPaperFlipbookComponent* SpriteComponent = Defaults->GetSprite())
	{
		SpriteComponent->Modify();
		SpriteComponent->SetFlipbook(IdleFlipbook);
		const float HalfHeight = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		SpriteComponent->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight - PixelsBelowFeet * CentimetersPerPixel));
	}

	FOnyxAssetUtils::AddBehaviourComponents(Blueprint, Behaviours);
	if (UOnyxCameraComponent* Camera = FOnyxAssetUtils::FindComponentTemplate<UOnyxCameraComponent>(Blueprint))
	{
		Camera->Modify();
		Camera->Settings.bPixelPerfect = true;
		Camera->Settings.ArtPixelSize = CentimetersPerPixel;
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FOnyxAssetUtils::SavePackages({ Blueprint->GetPackage() });
	return Blueprint;
}

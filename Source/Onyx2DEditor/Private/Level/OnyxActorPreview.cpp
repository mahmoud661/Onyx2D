// Copyright Onyx2D. All Rights Reserved.

#include "Level/OnyxActorPreview.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxMovesetComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"

namespace
{
	const UPaperSprite* FlipbookSpriteAt(const UPaperFlipbook* Flipbook, double Time, float PlayRate = 1.f)
	{
		if (!Flipbook || Flipbook->GetNumKeyFrames() == 0)
		{
			return nullptr;
		}
		const float Duration = Flipbook->GetTotalDuration();
		const float At = Duration > 0.f ? float(FMath::Fmod(Time * FMath::Max(PlayRate, 0.01f), double(Duration))) : 0.f;
		return Flipbook->GetSpriteAtTime(At);
	}

	/** The sprite a render component shows at Time (and its tint). */
	const UPaperSprite* ComponentSpriteAt(const UActorComponent* Component, double Time, FLinearColor& OutColor)
	{
		if (const UPaperFlipbookComponent* Flipbook = Cast<UPaperFlipbookComponent>(Component))
		{
			OutColor = Flipbook->GetSpriteColor();
			return FlipbookSpriteAt(const_cast<UPaperFlipbookComponent*>(Flipbook)->GetFlipbook(), Time, Flipbook->GetPlayRate());
		}
		if (const UPaperSpriteComponent* Sprite = Cast<UPaperSpriteComponent>(Component))
		{
			OutColor = Sprite->GetSpriteColor();
			return const_cast<UPaperSpriteComponent*>(Sprite)->GetSprite(); // GetSprite() is not const
		}
		return nullptr;
	}
}

namespace OnyxActorPreview
{
	bool MakeDraw(const UPaperSprite* Sprite, const FTransform& Transform, FOnyxSpriteDraw& OutDraw)
	{
		UTexture2D* Texture = Sprite ? Sprite->GetSourceTexture() : nullptr;
		if (!Texture)
		{
			return false;
		}
		const FVector2D TextureSize(FMath::Max(Texture->GetSurfaceWidth(), 1.f), FMath::Max(Texture->GetSurfaceHeight(), 1.f));
		const FVector2D SourceMin = Sprite->GetSourceUV();
		const FVector2D SourceSize = Sprite->GetSourceSize();
		if (SourceSize.X <= 0.0 || SourceSize.Y <= 0.0)
		{
			return false;
		}
		// The source rectangle around the pivot, in the sprite's units (X right, Z up), then into the world.
		const FVector2D Pivot = Sprite->GetPivotPosition();
		const double PixelsPerUnit = FMath::Max(double(Sprite->GetPixelsPerUnrealUnit()), 0.0001);
		const double Left = (SourceMin.X - Pivot.X) / PixelsPerUnit;
		const double Right = (SourceMin.X + SourceSize.X - Pivot.X) / PixelsPerUnit;
		const double Top = (Pivot.Y - SourceMin.Y) / PixelsPerUnit;
		const double Bottom = (Pivot.Y - SourceMin.Y - SourceSize.Y) / PixelsPerUnit;
		const FVector A = Transform.TransformPosition(FVector(Left, 0.0, Top));
		const FVector B = Transform.TransformPosition(FVector(Right, 0.0, Bottom));
		OutDraw.Texture = Texture;
		OutDraw.Pixels = FIntPoint(FMath::RoundToInt(SourceSize.X), FMath::RoundToInt(SourceSize.Y));
		OutDraw.UV = FBox2f(FVector2f(SourceMin / TextureSize), FVector2f((SourceMin + SourceSize) / TextureSize));
		OutDraw.WorldMin = FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Z, B.Z));
		OutDraw.WorldMax = FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Z, B.Z));
		OutDraw.bFlipX = B.X < A.X;
		OutDraw.Depth = Transform.GetLocation().Y;
		return true;
	}

	void Gather(const AActor* Actor, double Time, TArray<FOnyxSpriteDraw>& OutDraws)
	{
		if (!Actor)
		{
			return;
		}
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (const UActorComponent* Component : Components)
		{
			const USceneComponent* Scene = Cast<USceneComponent>(Component);
			if (!Scene || !Scene->IsVisible() || Scene->bHiddenInGame)
			{
				continue;
			}
			FLinearColor Color = FLinearColor::White;
			FOnyxSpriteDraw Draw;
			if (MakeDraw(ComponentSpriteAt(Component, Time, Color), Scene->GetComponentTransform(), Draw))
			{
				Draw.Color = Color;
				OutDraws.Add(Draw);
			}
		}
		OutDraws.StableSort([](const FOnyxSpriteDraw& A, const FOnyxSpriteDraw& B) { return A.Depth < B.Depth; }); // far (more negative Y) first
	}

	bool GetClassSprite(const UClass* Class, double Time, FOnyxSpriteDraw& OutDraw)
	{
		const AActor* Defaults = Class ? Class->GetDefaultObject<AActor>() : nullptr;
		if (!Defaults)
		{
			return false;
		}
		// Characters: their Moveset's Idle clip (what they show standing in the level).
		if (const UOnyxMovesetComponent* Moveset = Defaults->FindComponentByClass<UOnyxMovesetComponent>(); Moveset && Moveset->Moveset)
		{
			const UOnyxClip* Idle = Moveset->Moveset->FindClip(TEXT("Idle"));
			for (const TPair<FName, TObjectPtr<UOnyxClip>>& Pair : Moveset->Moveset->Clips)
			{
				Idle = Idle ? Idle : Pair.Value.Get();
			}
			if (Idle && MakeDraw(FlipbookSpriteAt(Idle->Flipbook, Time, Idle->PlayRate), FTransform::Identity, OutDraw))
			{
				return true;
			}
		}
		// Native components on the defaults, then components added in the Blueprint (and its parents).
		TArray<const UActorComponent*> Candidates;
		for (const UActorComponent* Component : Defaults->GetComponents())
		{
			Candidates.Add(Component);
		}
		for (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class); Generated; Generated = Cast<UBlueprintGeneratedClass>(Generated->GetSuperClass()))
		{
			if (const USimpleConstructionScript* Script = Generated->SimpleConstructionScript)
			{
				for (const USCS_Node* Node : Script->GetAllNodes())
				{
					Candidates.Add(Node->ComponentTemplate);
				}
			}
		}
		for (const UActorComponent* Component : Candidates)
		{
			FLinearColor Color = FLinearColor::White;
			const USceneComponent* Scene = Cast<USceneComponent>(Component);
			if (Scene && MakeDraw(ComponentSpriteAt(Component, Time, Color), Scene->GetRelativeTransform(), OutDraw))
			{
				OutDraw.Color = Color;
				return true;
			}
		}
		return false;
	}
}

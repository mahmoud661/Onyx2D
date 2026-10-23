// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxContentKit.h"
#include "Builders/OnyxAutotile.h"
#include "Farming/OnyxAutotileSet.h"
#include "AI/OnyxEnemyAIComponent.h"
#include "AI/OnyxPatrolComponent.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipWindow.h"
#include "Animation/OnyxMoveset.h"
#include "Animation/OnyxMovesetComponent.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxColorGrade.h"
#include "Builders/OnyxFramePacker.h"
#include "Builders/OnyxSoundSynth.h"
#include "Builders/OnyxTileBuilder.h"
#include "Camera/OnyxCameraComponent.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Character/OnyxCharacter.h"
#include "Character/OnyxCharacterEffectsComponent.h"
#include "Combat/OnyxCombatComponent.h"
#include "Combat/OnyxCombatTypes.h"
#include "Combat/OnyxHealthComponent.h"
#include "Combat/OnyxHitboxComponent.h"
#include "Combat/OnyxStompableComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Onyx2DLibrary.h"
#include "Core/OnyxGameMode.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Movement/OnyxMovementComponent.h"
#include "ObjectTools.h"
#include "OnyxEditorLibrary.h"
#include "OnyxHUDWidget.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteActor.h"
#include "PaperSpriteComponent.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "World/OnyxPickup.h"
#include "World/OnyxTileCollision.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxContentKit, Log, All);

namespace
{
	/** One frame of a spinning gold coin (0-5): seen face on, turning to its edge and back. */
	FOnyxPixelCanvas DrawCoin(int32 Frame)
	{
		static const float HalfWidths[] = { 4.6f, 3.7f, 2.3f, 0.9f, 2.3f, 3.7f };
		const float HalfWidth = HalfWidths[Frame % 6];
		const float HalfHeight = 4.6f;
		const FColor Rim(0xa8, 0x6a, 0x1c);
		const FColor Face(0xf0, 0xb8, 0x42);
		const FColor Light(0xff, 0xe9, 0x9a);
		const FColor Shine(0xff, 0xff, 0xf0);
		FOnyxPixelCanvas Canvas(12, 12);
		for (int32 Y = 0; Y < 12; ++Y)
		{
			for (int32 X = 0; X < 12; ++X)
			{
				const float DX = (X + 0.5f - 6.f) / HalfWidth;
				const float DY = (Y + 0.5f - 6.f) / HalfHeight;
				const float D = DX * DX + DY * DY;
				if (D > 1.f)
				{
					continue;
				}
				FColor Color = D > 0.55f ? Rim : (DX < -0.1f && DY < -0.1f && D < 0.42f ? Light : Face);
				if (HalfWidth < 1.f)
				{
					Color = X + 0.5f < 6.f ? Light : Rim; // edge on: a lit side and a dark side
				}
				Canvas.Set(X, Y, Color);
			}
		}
		if (HalfWidth > 2.f)
		{
			Canvas.Set(FMath::FloorToInt(6.f - HalfWidth * 0.45f), 3, Shine);
		}
		Canvas.Outline(FColor(0x3a, 0x1e, 0x08));
		return Canvas;
	}

	/** One frame of the sparkle a coin leaves when picked up (0-3): eight points flying out and fading. */
	FOnyxPixelCanvas DrawSparkle(int32 Frame)
	{
		FOnyxPixelCanvas Canvas(16, 16);
		const uint8 Alphas[] = { 255, 230, 170, 90 };
		const FColor Color(0xff, 0xf3, 0xb0, Alphas[Frame]);
		const float Radius = 2.f + Frame * 1.8f;
		for (int32 Point = 0; Point < 8; ++Point)
		{
			const float Angle = Point * UE_PI / 4.f;
			const float Reach = Point % 2 == 0 ? Radius : Radius * 0.72f;
			Canvas.Set(FMath::RoundToInt(7.5f + FMath::Cos(Angle) * Reach), FMath::RoundToInt(7.5f - FMath::Sin(Angle) * Reach), Color);
		}
		if (Frame < 2)
		{
			for (const FIntPoint& Cell : { FIntPoint(8, 8), FIntPoint(7, 8), FIntPoint(9, 8), FIntPoint(8, 7), FIntPoint(8, 9) })
			{
				Canvas.Set(Cell.X, Cell.Y, FColor(0xff, 0xff, 0xff, Alphas[Frame]));
			}
		}
		return Canvas;
	}

	/** The HUD's health pip: a drop of pale blue light. */
	FOnyxPixelCanvas DrawDrop()
	{
		const FColor Fill(0xbf, 0xea, 0xff);
		const FColor Shade(0x6c, 0xb0, 0xe0);
		const FColor Deep(0x3d, 0x78, 0xb8);
		const FColor Light(0xff, 0xff, 0xff);
		FOnyxPixelCanvas Canvas(12, 16);
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 12; ++X)
			{
				const float PX = X + 0.5f - 6.f;
				const float PY = Y + 0.5f;
				const bool bRound = PX * PX + FMath::Square(PY - 10.f) <= 4.6f * 4.6f;
				const bool bTip = PY >= 1.5f && PY < 10.f && FMath::Abs(PX) <= (PY - 1.5f) * 0.56f;
				if (!bRound && !bTip)
				{
					continue;
				}
				FColor Color = Fill;
				if (PX + (PY - 10.f) > 4.f)
				{
					Color = Deep;
				}
				else if (PX > 1.5f && PY > 8.5f)
				{
					Color = Shade;
				}
				Canvas.Set(X, Y, Color);
			}
		}
		for (const FIntPoint& Cell : { FIntPoint(4, 8), FIntPoint(4, 9), FIntPoint(5, 7), FIntPoint(4, 10) })
		{
			Canvas.Set(Cell.X, Cell.Y, Light);
		}
		Canvas.Outline(FColor(0x08, 0x0e, 0x1e));
		return Canvas;
	}

	/** One frame of the dust kicked up at the feet (0-4): two soft clouds rolling apart, rising a little and thinning out. */
	FOnyxPixelCanvas DrawDust(int32 Frame)
	{
		FOnyxPixelCanvas Canvas(28, 14);
		const uint8 Alphas[] = { 235, 215, 175, 120, 60 };
		const FColor Light(0xc9, 0xc4, 0xd6, Alphas[Frame]);
		const FColor Shade(0x86, 0x80, 0x9a, Alphas[Frame]);
		const float Spread = 2.5f + Frame * 2.6f;
		const float Radius = 2.2f + Frame * 0.75f;
		const float CentreY = 8.5f - Frame * 0.8f;
		for (const float Side : { -1.f, 1.f })
		{
			const float CentreX = 14.f + Side * Spread;
			for (int32 Y = 0; Y < Canvas.Size.Y; ++Y)
			{
				for (int32 X = 0; X < Canvas.Size.X; ++X)
				{
					const float DX = (X + 0.5f - CentreX) / Radius;
					const float DY = (Y + 0.5f - CentreY) / (Radius * 0.8f);
					// Late frames break up into a dither, the dust settling.
					if (DX * DX + DY * DY > 1.f || (Frame >= 3 && (X + Y) % 2 == 1))
					{
						continue;
					}
					Canvas.Set(X, Y, DY < -0.2f ? Light : Shade);
				}
			}
		}
		return Canvas;
	}

	/** One frame of the spark where the spear strikes (0-3): a white flash, then rays and a ring flying out and cooling to blue. */
	FOnyxPixelCanvas DrawHitSpark(int32 Frame)
	{
		FOnyxPixelCanvas Canvas(32, 32);
		const FColor Core(0xff, 0xff, 0xff);
		const FColor Glow(0xc8, 0xf0, 0xff);
		const FColor Edge(0x5c, 0xb8, 0xf0);
		const float Centre = 15.5f;
		const float Reach[] = { 0.45f, 1.f, 1.1f, 1.15f };
		const float Eaten[] = { 0.f, 0.2f, 0.55f, 0.85f };
		struct FRay { float Degrees; float Length; };
		// Long along the thrust, shorter up and down, short on the diagonals.
		const FRay Rays[] = { { 0.f, 15.f }, { 180.f, 15.f }, { 90.f, 9.f }, { 270.f, 9.f }, { 35.f, 7.f }, { 145.f, 7.f }, { 215.f, 7.f }, { 325.f, 7.f } };
		for (const FRay& Ray : Rays)
		{
			const float Angle = FMath::DegreesToRadians(Ray.Degrees);
			for (float Along = Eaten[Frame] * Ray.Length; Along <= Reach[Frame] * Ray.Length; Along += 0.5f)
			{
				const float Part = Along / Ray.Length;
				const FColor Color = Frame == 3 ? Edge : Part < 0.4f ? Core : Part < 0.75f ? Glow : Edge;
				const int32 X = FMath::RoundToInt(Centre + FMath::Cos(Angle) * Along);
				const int32 Y = FMath::RoundToInt(Centre - FMath::Sin(Angle) * Along);
				Canvas.Set(X, Y, Color);
				if (Frame < 2 && Ray.Length > 10.f && Part < 0.5f)
				{
					Canvas.Set(X, Y + 1, Color); // the thrust's rays are thick near the middle
				}
			}
		}
		const float RingRadius[] = { 0.f, 5.5f, 8.f, 0.f };
		for (int32 Y = 0; Y < Canvas.Size.Y; ++Y)
		{
			for (int32 X = 0; X < Canvas.Size.X; ++X)
			{
				const float Distance = FVector2f(X - Centre, Y - Centre).Size();
				if (Frame == 0 && Distance <= 4.5f)
				{
					Canvas.Set(X, Y, Distance <= 3.2f ? Core : Glow);
				}
				else if (RingRadius[Frame] > 0.f && FMath::Abs(Distance - RingRadius[Frame]) < 0.6f && (Frame == 1 || (X + Y) % 2 == 0))
				{
					Canvas.Set(X, Y, Frame == 1 ? Glow : Edge);
				}
			}
		}
		return Canvas;
	}

	/** One frame of a burst (0-4): a ball of light swelling, hollowing out and breaking into embers. */
	FOnyxPixelCanvas DrawBurst(int32 Frame, const FColor& Core, const FColor& Middle, const FColor& Edge)
	{
		FOnyxPixelCanvas Canvas(24, 24);
		const float Centre = 12.f;
		const float Outer[] = { 4.f, 7.f, 9.f, 10.5f, 11.5f };
		const float Inner[] = { 0.f, 0.f, 3.5f, 6.5f, 9.f };
		for (int32 Y = 0; Y < Canvas.Size.Y; ++Y)
		{
			for (int32 X = 0; X < Canvas.Size.X; ++X)
			{
				const float DX = X + 0.5f - Centre;
				const float DY = Y + 0.5f - Centre;
				const float Distance = FMath::Sqrt(DX * DX + DY * DY);
				const float Angle = FMath::Atan2(DY, DX);
				const float Ragged = 1.f + 0.16f * FMath::Sin(Angle * 7.f + Frame * 1.3f) + 0.08f * FMath::Sin(Angle * 13.f);
				if (Distance > Outer[Frame] * Ragged || Distance < Inner[Frame] * Ragged || (Frame >= 3 && (X * 7 + Y * 3 + Frame) % 3 == 0))
				{
					continue;
				}
				const float Part = (Distance - Inner[Frame]) / FMath::Max(Outer[Frame] - Inner[Frame], 1.f);
				FColor Color = Part < 0.35f ? Core : Part < 0.7f ? Middle : Edge;
				if (Frame == 0)
				{
					Color = Core;
				}
				else if (Frame >= 3)
				{
					Color = Part < 0.5f ? Middle : Edge;
				}
				Canvas.Set(X, Y, Color);
			}
		}
		return Canvas;
	}

	/**
	 * One frame (0-7) of motes of light spiralling in to the middle, a spark lighting there as they arrive. Pale colours
	 * only: drawn over the character, in the dark.
	 */
	FOnyxPixelCanvas DrawGather(int32 Frame)
	{
		FOnyxPixelCanvas Canvas(40, 40);
		const FColor Core(0xff, 0xff, 0xff);
		const FColor Middle(0xd2, 0xf8, 0xff);
		const FColor Tail(0x9c, 0xe6, 0xff);
		const float Centre = 20.f;
		constexpr int32 Motes = 10;
		for (int32 Index = 0; Index < Motes; ++Index)
		{
			// Each mote comes in at its own time, curling as it nears the middle, and is gone when it gets there.
			const float Start = 16.f + (Index % 3) * 1.5f;
			const float Phase = FMath::Frac(Frame / 8.f + Index * 0.37f);
			const float Radius = Start * (1.f - Phase);
			if (Radius < 3.f)
			{
				continue;
			}
			const float Angle = Index * UE_TWO_PI / Motes + 1.1f * Phase;
			const int32 X = FMath::FloorToInt(Centre + FMath::Cos(Angle) * Radius);
			const int32 Y = FMath::FloorToInt(Centre + FMath::Sin(Angle) * Radius);
			const FColor Head = Radius < 9.f ? Core : Middle;
			Canvas.Set(X, Y, Head);
			Canvas.Set(X + 1, Y, Head);
			Canvas.Set(X, Y + 1, Head);
			Canvas.Set(X + 1, Y + 1, Middle);
			const int32 TX = FMath::FloorToInt(Centre + FMath::Cos(Angle - 0.3f) * (Radius + 3.f));
			const int32 TY = FMath::FloorToInt(Centre + FMath::Sin(Angle - 0.3f) * (Radius + 3.f));
			Canvas.Set(TX, TY, Tail);
		}
		// A small spark in the middle, brighter as the light gathers.
		const int32 Arm = Frame < 3 ? 0 : Frame < 6 ? 1 : 2;
		for (int32 Step = -Arm; Step <= Arm; ++Step)
		{
			Canvas.Set(20 + Step, 20, Step == 0 ? Core : Middle);
			Canvas.Set(20, 20 + Step, Step == 0 ? Core : Middle);
		}
		return Canvas;
	}

	/** One frame (0-5) of a thin ring of light going out, with four glints on it at first. Pale colours only. */
	FOnyxPixelCanvas DrawMended(int32 Frame)
	{
		FOnyxPixelCanvas Canvas(48, 48);
		const FColor Core(0xff, 0xff, 0xff);
		const FColor Middle(0xd2, 0xf8, 0xff);
		const FColor Edge(0x9c, 0xe6, 0xff);
		const float Centre = 24.f;
		const float Radius = 8.f + Frame * 3.f;
		const float Thickness = Frame < 2 ? 1.6f : Frame < 4 ? 1.1f : 0.8f;
		for (int32 Y = 0; Y < Canvas.Size.Y; ++Y)
		{
			for (int32 X = 0; X < Canvas.Size.X; ++X)
			{
				const float Distance = FVector2D(X + 0.5f - Centre, Y + 0.5f - Centre).Size();
				// Breaking up as it fades.
				if (FMath::Abs(Distance - Radius) > Thickness || (Frame >= 3 && (X * 5 + Y * 3 + Frame) % (Frame >= 5 ? 2 : 4) == 0))
				{
					continue;
				}
				Canvas.Set(X, Y, Frame == 0 ? Core : Distance < Radius ? Middle : Edge);
			}
		}
		if (Frame < 3)
		{
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const float Angle = UE_PI * 0.25f + Corner * UE_HALF_PI;
				const int32 GX = FMath::FloorToInt(Centre + FMath::Cos(Angle) * Radius);
				const int32 GY = FMath::FloorToInt(Centre + FMath::Sin(Angle) * Radius);
				const int32 Arm = Frame == 1 ? 2 : 1;
				for (int32 Step = -Arm; Step <= Arm; ++Step)
				{
					Canvas.Set(GX + Step, GY, Core);
					Canvas.Set(GX, GY + Step, Core);
				}
			}
		}
		return Canvas;
	}

	/** One frame (0-3) of blades meeting: a white star with a warm edge, shrinking away. */
	FOnyxPixelCanvas DrawClash(int32 Frame)
	{
		FOnyxPixelCanvas Canvas(36, 36);
		const FColor Core(0xff, 0xff, 0xff);
		const FColor Warm(0xff, 0xe4, 0x96);
		const int32 Centre = 18;
		const int32 Long[] = { 16, 12, 8, 4 };
		const int32 Short[] = { 8, 6, 4, 2 };
		for (int32 Step = -Long[Frame]; Step <= Long[Frame]; ++Step)
		{
			const FColor Color = FMath::Abs(Step) > Long[Frame] * 0.6f ? Warm : Core;
			Canvas.Set(Centre + Step, Centre, Color);
			Canvas.Set(Centre, Centre + Step, Color);
			if (Frame < 2 && FMath::Abs(Step) < Long[Frame] * 0.4f)
			{
				Canvas.Set(Centre + Step, Centre + 1, Color); // the long arms thicker at first
				Canvas.Set(Centre + 1, Centre + Step, Color);
			}
		}
		for (int32 Step = -Short[Frame]; Step <= Short[Frame]; ++Step)
		{
			Canvas.Set(Centre + Step, Centre + Step, Warm);
			Canvas.Set(Centre + Step, Centre - Step, Warm);
		}
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			for (int32 X = -1; X <= 1; ++X)
			{
				Canvas.Set(Centre + X, Centre + Y, Core);
			}
		}
		return Canvas;
	}

	/** One frame of a wisp of ghost-fire (0-3), flying right: a bright head and a tail that flickers behind it. */
	FOnyxPixelCanvas DrawWisp(int32 Frame)
	{
		FOnyxPixelCanvas Canvas(18, 12);
		const FColor Core(0xf0, 0xff, 0xff);
		const FColor Middle(0x8c, 0xe8, 0xff);
		const FColor Edge(0x3a, 0x9c, 0xe0);
		const float HeadX = 12.5f;
		const float HeadY = 6.f + (Frame % 2 == 0 ? 0.f : -0.5f);
		for (int32 Y = 0; Y < Canvas.Size.Y; ++Y)
		{
			for (int32 X = 0; X < Canvas.Size.X; ++X)
			{
				const float PX = X + 0.5f;
				const float PY = Y + 0.5f;
				const float Head = FVector2f(PX - HeadX, PY - HeadY).Size();
				if (Head <= 3.6f)
				{
					Canvas.Set(X, Y, Head <= 1.9f ? Core : Middle);
					continue;
				}
				if (PX < 1.5f || PX > HeadX)
				{
					continue;
				}
				const float Wave = HeadY + FMath::Sin(PX * 0.9f + Frame * 1.6f) * (HeadX - PX) * 0.12f;
				const float HalfWidth = 2.8f * (PX - 1.5f) / (HeadX - 1.5f);
				if (FMath::Abs(PY - Wave) <= HalfWidth)
				{
					Canvas.Set(X, Y, PX > HeadX - 4.f ? Middle : Edge);
				}
			}
		}
		return Canvas;
	}




	/** Adds a named widget under a panel, the way dragging it from the Palette does. */
	template <class TWidget>
	TWidget* AddWidget(UWidgetBlueprint& Blueprint, UPanelWidget* Parent, const TCHAR* Name)
	{
		TWidget* Widget = Blueprint.WidgetTree->ConstructWidget<TWidget>(TWidget::StaticClass(), FName(Name));
#if WITH_EDITORONLY_DATA
		Widget->bExpandedInDesigner = true;
#endif
		FText AddError;
		if (!FWidgetBlueprintOperationUtils::AddWidget(&Blueprint, Widget, Parent, INDEX_NONE, AddError))
		{
			UE_LOG(LogOnyxContentKit, Warning, TEXT("%s: could not add %s (%s)"), *Blueprint.GetName(), Name, *AddError.ToString());
		}
		return Widget;
	}
}

// --- Kit ------------------------------------------------------------------------------------------------------------

FOnyxContentKit::FOnyxContentKit(const FString& ContentFolder, float InPixelSize)
	: Root(ContentFolder)
	, PixelSize(InPixelSize)
{
}

UTexture2D* FOnyxContentKit::ImportPng(const FString& File, const FString& Folder, const FString& Name)
{
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = File;
	Task->DestinationPath = Root / Folder;
	Task->DestinationName = Name;
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = true;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });
	const FString Path = Root / Folder / Name;
	return LoadObject<UTexture2D>(nullptr, *(Path + TEXT(".") + Name));
}

UTexture2D* FOnyxContentKit::Import(const FString& File, const FString& Folder, const FString& Name, const FOnyxColorGrade* Grade)
{
	UTexture2D* Texture = ImportPng(File, Folder, Name);
	if (Texture && Grade && Grade->Apply(Texture))
	{
		FOnyxAssetUtils::ConfigurePixelArtTexture(Texture);
		FOnyxAssetUtils::SavePackages({ Texture->GetPackage() });
	}
	return Texture;
}

TArray<UTexture2D*> FOnyxContentKit::ImportFrames(const FString& Directory, const FString& Prefix)
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*.png")), true, false);
	// By the number the name ends with: "run-10.png" after "run-2.png", "fire10.png" after "fire9.png".
	auto Number = [](const FString& File)
	{
		const FString Base = FPaths::GetBaseFilename(File);
		int32 Start = Base.Len();
		while (Start > 0 && FChar::IsDigit(Base[Start - 1]))
		{
			--Start;
		}
		return Start < Base.Len() ? FCString::Atoi(*Base.RightChop(Start)) : 0;
	};
	Files.Sort([&Number](const FString& A, const FString& B) { return Number(A) < Number(B); });
	TArray<UTexture2D*> Frames;
	for (const FString& File : Files)
	{
		if (UTexture2D* Frame = ImportPng(Directory / File, TEXT("Frames") / Prefix, FPaths::GetBaseFilename(File).Replace(TEXT("-"), TEXT("_"))))
		{
			Frames.Add(Frame);
		}
	}
	return Frames;
}

UTexture2D* FOnyxContentKit::Strip(const TArray<UTexture2D*>& Frames, const FString& Name, FIntPoint& OutFrameSize)
{
	if (Frames.IsEmpty())
	{
		Error = FText::Format(INVTEXT("The frames for {0} are missing from the packs."), FText::FromString(Name));
		return nullptr;
	}
	FText StripError;
	UTexture2D* Texture = FOnyxFramePacker::PackStrip(Frames, Root / TEXT("Strips"), Name, OutFrameSize, StripError);
	if (!Texture)
	{
		Error = StripError;
	}
	return Texture;
}

UPaperFlipbook* FOnyxContentKit::Flipbook(const TArray<UTexture2D*>& Frames, const FString& Name, float FramesPerSecond)
{
	FIntPoint FrameSize;
	UTexture2D* Texture = Strip(Frames, TEXT("T_") + Name + TEXT("_Strip"), FrameSize);
	if (!Texture)
	{
		return nullptr;
	}
	TArray<FIntPoint> Cells;
	for (int32 Index = 0; Index < Frames.Num(); ++Index)
	{
		Cells.Add(FIntPoint(Index, 0));
	}
	return UOnyxEditorLibrary::CreateFlipbookFromCells(Texture, FrameSize, Cells, Name, Root / TEXT("Effects"), PixelSize, FramesPerSecond);
}

UPaperFlipbook* FOnyxContentKit::DrawnFlipbook(const TArray<FOnyxPixelCanvas>& Frames, const FString& Name, float FramesPerSecond, bool bEffect)
{
	const FOnyxPixelCanvas Sheet = FOnyxPixelCanvas::Strip(Frames);
	UTexture2D* Texture = FOnyxAssetUtils::CreateTexture(Root / TEXT("Effects"), TEXT("T_") + Name + TEXT("_Strip"), Sheet.Pixels, Sheet.Size);
	if (!Texture)
	{
		return nullptr;
	}
	FOnyxAssetUtils::SavePackages({ Texture->GetPackage() });
	if (bEffect)
	{
		// Every frame the same size, around the same middle: a burst grows from its centre instead of from its lowest pixel.
		// Drawn over the finished lighting (Onyx effect material), Paper2D's own see-through material when it is missing.
		UMaterialInterface* SeeThrough = LoadObject<UMaterialInterface>(nullptr, TEXT("/Onyx2D/Materials/M_OnyxEffectSprite.M_OnyxEffectSprite"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!SeeThrough)
		{
			SeeThrough = LoadObject<UMaterialInterface>(nullptr, TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial"));
		}
		FOnyxAssetUtils::ConfigurePixelArtTexture(Texture);
		TArray<UPaperSprite*> Sprites;
		TArray<UPackage*> Packages;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			UPaperSprite* Sprite = FOnyxAssetUtils::CreateSprite(Root / TEXT("Effects/Sprites"), FString::Printf(TEXT("S_%s_%02d"), *Name, Index), Texture,
				FIntPoint(Index * Frames[0].Size.X, 0), Frames[0].Size, 1.f / PixelSize, ESpritePivotMode::Center_Center, SeeThrough);
			if (!Sprite)
			{
				return nullptr;
			}
			Sprites.Add(Sprite);
			Packages.Add(Sprite->GetPackage());
		}
		UPaperFlipbook* Flipbook = FOnyxAssetUtils::CreateFlipbook(Root / TEXT("Effects"), Name, Sprites, FramesPerSecond);
		if (Flipbook)
		{
			Packages.Add(Flipbook->GetPackage());
		}
		FOnyxAssetUtils::SavePackages(Packages);
		return Flipbook;
	}
	TArray<FIntPoint> Cells;
	for (int32 Index = 0; Index < Frames.Num(); ++Index)
	{
		Cells.Add(FIntPoint(Index, 0));
	}
	return UOnyxEditorLibrary::CreateFlipbookFromCells(Texture, Frames[0].Size, Cells, Name, Root / TEXT("Effects"), PixelSize, FramesPerSecond);
}

UPaperSprite* FOnyxContentKit::Prop(const FString& File, const FString& Name, const FOnyxColorGrade* Grade, const FColor* Background)
{
	UTexture2D* Texture = Import(File, TEXT("Props"), TEXT("T_") + Name, Grade);
	if (!Texture)
	{
		Error = FText::Format(INVTEXT("Could not import {0}."), FText::FromString(File));
		return nullptr;
	}
	if (Background)
	{
		ClearColor(Texture, *Background);
	}
	UPaperSprite* Sprite = UOnyxEditorLibrary::CreateSpriteFromCell(Texture, FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY()), FIntPoint::ZeroValue,
		TEXT("S_") + Name, Root / TEXT("Props"), PixelSize);
	UOnyxEditorLibrary::GenerateNormalMap(Texture);
	return Sprite;
}

int32 FOnyxContentKit::ClearColor(UTexture2D* Texture, const FColor& Color)
{
	if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetFormat() != TSF_BGRA8)
	{
		return 0;
	}
	Texture->Modify();
	FTextureSource& Source = Texture->Source;
	const int64 Count = int64(Source.GetSizeX()) * Source.GetSizeY();
	uint8* Pixels = Source.LockMip(0);
	if (!Pixels)
	{
		return 0;
	}
	int32 Cleared = 0;
	for (int64 Index = 0; Index < Count; ++Index)
	{
		uint8* Pixel = Pixels + Index * 4; // B G R A
		if (Pixel[3] > 0 && Pixel[0] == Color.B && Pixel[1] == Color.G && Pixel[2] == Color.R)
		{
			Pixel[3] = 0;
			++Cleared;
		}
	}
	Source.UnlockMip(0);
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
	FOnyxAssetUtils::SavePackages({ Texture->GetPackage() });
	return Cleared;
}

UPaperSprite* FOnyxContentKit::SpriteFrom(UTexture2D* Texture, FIntPoint Offset, FIntPoint Size, const FString& Name)
{
	UPaperSprite* Sprite = Texture ? FOnyxAssetUtils::CreateSprite(Root / TEXT("Props"), Name, Texture, Offset, Size, 1.f / PixelSize, ESpritePivotMode::Bottom_Center) : nullptr;
	if (Sprite)
	{
		FOnyxAssetUtils::SavePackages({ Sprite->GetPackage() });
	}
	else
	{
		Error = FText::Format(INVTEXT("Could not make the {0} sprite."), FText::FromString(Name));
	}
	return Sprite;
}

UOnyxAutotileSet* FOnyxContentKit::MakeAutotileSet(const FOnyxPixelCanvas& Block, const TArray<FOnyxPixelCanvas>& Fills, const FString& Name)
{
	TArray<FOnyxPixelCanvas> Tiles;
	TArray<uint8> MaskToTile;
	FOnyxAutotile::BuildSet(Block, Fills, Tiles, MaskToTile);
	UTexture2D* Atlas = CanvasTexture(FOnyxPixelCanvas::Strip(Tiles), TEXT("Terrain"), TEXT("T_") + Name + TEXT("_Tiles"));
	UOnyxAutotileSet* Set = Atlas ? FOnyxAssetUtils::CreateOrReuse<UOnyxAutotileSet>(Root / TEXT("Terrain"), TEXT("AS_") + Name) : nullptr;
	if (!Set)
	{
		Error = FText::Format(INVTEXT("Could not make the {0} autotile set."), FText::FromString(Name));
		return nullptr;
	}
	Set->Modify();
	Set->Tiles.Reset();
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		Set->Tiles.Add(SpriteFrom(Atlas, FIntPoint(Index * FOnyxAutotile::Tile, 0), FIntPoint(FOnyxAutotile::Tile, FOnyxAutotile::Tile),
			FString::Printf(TEXT("S_%s_%02d"), *Name, Index)));
	}
	Set->FillCount = FMath::Max(Fills.Num(), 1);
	Set->MaskToTile = MaskToTile;
	Set->MarkPackageDirty();
	FOnyxAssetUtils::SavePackages({ Set->GetPackage() });
	return Set;
}

UPaperTileSet* FOnyxContentKit::MakeTileSet(const FString& File, const FString& Name, const FOnyxColorGrade* Grade, const TArray<int32>& Solid,
	const TArray<int32>& OneWay,
	FIntPoint TileSize)
{
	UTexture2D* Sheet = Import(File, TEXT("Textures"), TEXT("T_") + Name.RightChop(3) + TEXT("_Tiles"), Grade);
	FOnyxTileSetSpec Spec;
	Spec.Texture = Sheet;
	Spec.TileSize = TileSize;
	Spec.bSolidCollision = false;
	Spec.Name = Name;
	Spec.Folder = Root;
	FText TileError;
	UPaperTileSet* TileSet = Sheet ? FOnyxTileBuilder::CreateTileSet(Spec, TileError) : nullptr;
	if (!TileSet)
	{
		Fail(TileError.IsEmpty() ? FText::Format(INVTEXT("Could not create the {0} tile set."), FText::FromString(Name)) : TileError);
		return nullptr;
	}
	TArray<int32> Everything;
	for (int32 Index = 0; Index < TileSet->GetTileCount(); ++Index)
	{
		Everything.Add(Index);
	}
	UOnyxEditorLibrary::SetTileCollision(TileSet, Everything, EOnyxTileCollision::None);
	UOnyxEditorLibrary::SetTileCollision(TileSet, Solid, EOnyxTileCollision::Solid);
	UOnyxEditorLibrary::SetTileCollision(TileSet, OneWay, EOnyxTileCollision::OneWay);
	return TileSet;
}

UTexture2D* FOnyxContentKit::CanvasTexture(const FOnyxPixelCanvas& Canvas, const FString& Folder, const FString& Name)
{
	UTexture2D* Texture = FOnyxAssetUtils::CreateTexture(Root / Folder, Name, Canvas.Pixels, Canvas.Size);
	if (Texture)
	{
		FOnyxAssetUtils::SavePackages({ Texture->GetPackage() });
	}
	else
	{
		Error = FText::Format(INVTEXT("Could not create the {0} texture."), FText::FromString(Name));
	}
	return Texture;
}

// --- Characters -----------------------------------------------------------------------------------------------------

UBlueprint* FOnyxContentKit::MakeFoe(const FString& Name, const TMap<FName, UTexture2D*>& Strips, const TMap<FName, float>& Speeds, FIntPoint FrameSize,
	bool bFacesLeft, UPaperFlipbook* DeathEffect, TFunctionRef<void(AOnyxCharacter&, UOnyxPatrolComponent&, UOnyxEnemyAIComponent&)> Setup, int32 FeetRowsFromBottom,
	bool bFitToBody)
{
	for (const TPair<FName, UTexture2D*>& Pair : Strips)
	{
		if (!Pair.Value)
		{
			return nullptr;
		}
	}
	// A plain character with the fighting-foe behaviours (Patrol, Contact Damage, Enemy AI). Not stompable: the spear is
	// how you beat them (an older build of the examples made them stompable, so that goes).
	UBlueprint* Blueprint = UOnyxEditorLibrary::CreateCharacterFromStrips(Strips, Name, Root / TEXT("Characters"), FrameSize,
		int32(UOnyxBehaviourLibrary::FightingFoePreset), OnyxTeams::Enemy, PixelSize, Speeds, FeetRowsFromBottom, bFitToBody);
	RemoveComponent(Blueprint, UOnyxStompableComponent::StaticClass());
	AOnyxCharacter* Enemy = Blueprint ? Cast<AOnyxCharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	UOnyxPatrolComponent* Patrol = FOnyxAssetUtils::FindComponentTemplate<UOnyxPatrolComponent>(Blueprint);
	UOnyxEnemyAIComponent* AI = FOnyxAssetUtils::FindComponentTemplate<UOnyxEnemyAIComponent>(Blueprint);
	if (!Enemy || !Patrol || !AI)
	{
		Fail(FText::Format(INVTEXT("Could not create the {0} enemy."), FText::FromString(Name)));
		return nullptr;
	}
	Enemy->Modify();
	Patrol->Modify();
	AI->Modify();
	Enemy->bArtFacesLeft = bFacesLeft;
	Enemy->DeathEffect = DeathEffect;
	Enemy->Health->MaxHealth = 2.f;
	Enemy->Health->InvulnerabilityTime = 0.f; // every blow lands: a combo is a combo (each hitbox window still hits once)
	AI->Attacks.Reset();
	AI->CoinDrop = 2;
	AI->CoinLook = Coins.Coin;
	AI->CoinCollectEffect = Coins.Sparkle;
	AI->CoinSound = Coins.Sound;
	Setup(*Enemy, *Patrol, *AI);
	Blueprint->MarkPackageDirty();
	Dirty.Add(Blueprint->GetPackage());
	return Blueprint;
}

void FOnyxContentKit::AddGlow(UBlueprint* Blueprint, const FLinearColor& Color, float Intensity, float Radius, const FVector2D& Offset, float Flicker)
{
	USimpleConstructionScript* Construction = Blueprint ? Blueprint->SimpleConstructionScript.Get() : nullptr;
	AOnyxCharacter* Defaults = Blueprint ? Cast<AOnyxCharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Construction || !Defaults || !Defaults->GetSprite())
	{
		return;
	}
	// Rebuilding: replace every glow made before. A new node takes a numbered name while an old one is still about (Glow_1,
	// GlowFlicker_2...), so they are found by kind and name, not by the exact name, or each rebuild would stack another light.
	TArray<USCS_Node*> Old;
	for (USCS_Node* Node : Construction->GetAllNodes())
	{
		const FString Name = Node ? Node->GetVariableName().ToString() : FString();
		if (Node && Node->ComponentClass && Name.StartsWith(TEXT("Glow"))
			&& (Node->ComponentClass->IsChildOf(UOnyxLight2DComponent::StaticClass()) || Node->ComponentClass->IsChildOf(UOnyxLightFlickerComponent::StaticClass())))
		{
			Old.Add(Node);
		}
	}
	for (USCS_Node* Node : Old)
	{
		Construction->RemoveNode(Node);
	}
	if (!Old.IsEmpty())
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint); // the old names free again
		Defaults = Cast<AOnyxCharacter>(Blueprint->GeneratedClass->GetDefaultObject()); // compiling made a new one
		if (!Defaults || !Defaults->GetSprite())
		{
			return;
		}
	}
	USCS_Node* LightNode = Construction->CreateNode(UOnyxLight2DComponent::StaticClass(), TEXT("Glow"));
	UOnyxLight2DComponent* Light = CastChecked<UOnyxLight2DComponent>(LightNode->ComponentTemplate);
	Light->Color = Color;
	Light->Intensity = Intensity;
	Light->Radius = Radius;
	Light->bCastShadows = false;
	Light->bOwnGlow = true; // a character with a Light Guard lights the world with it, not itself
	// Under the sprite, which turns half a circle when the character faces the other way: the glow moves with it.
	Light->SetRelativeLocation(FVector(Offset.X, 0.f, Offset.Y));
	LightNode->SetParent(Defaults->GetSprite());
	Construction->AddNode(LightNode);
	USCS_Node* FlickerNode = Construction->CreateNode(UOnyxLightFlickerComponent::StaticClass(), TEXT("GlowFlicker"));
	UOnyxLightFlickerComponent* Animation = CastChecked<UOnyxLightFlickerComponent>(FlickerNode->ComponentTemplate);
	Animation->Animation = EOnyxLightAnimation::Flicker;
	Animation->Strength = Flicker;
	Animation->Speed = 5.f;
	Construction->AddNode(FlickerNode);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Dirty.AddUnique(Blueprint->GetPackage());
}

UActorComponent* FOnyxContentKit::AddComponent(UBlueprint* Blueprint, UClass* Class, FName Name)
{
	if (UActorComponent* Existing = FOnyxAssetUtils::FindComponentTemplate(Blueprint, Class))
	{
		return Existing;
	}
	USimpleConstructionScript* Construction = Blueprint ? Blueprint->SimpleConstructionScript.Get() : nullptr;
	if (!Construction || !Class)
	{
		return nullptr;
	}
	USCS_Node* Node = Construction->CreateNode(Class, Name);
	Construction->AddNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Node->ComponentTemplate;
}

void FOnyxContentKit::RemoveComponent(UBlueprint* Blueprint, UClass* Class)
{
	USimpleConstructionScript* Construction = Blueprint ? Blueprint->SimpleConstructionScript.Get() : nullptr;
	if (!Construction)
	{
		return;
	}
	bool bRemoved = false;
	const TArray<USCS_Node*> Nodes = Construction->GetAllNodes(); // a copy: removing changes the list
	for (USCS_Node* Node : Nodes)
	{
		if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA(Class))
		{
			Construction->RemoveNodeAndPromoteChildren(Node);
			bRemoved = true;
		}
	}
	if (bRemoved)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}
}

// --- Effects, coins, HUD, game mode ---------------------------------------------------------------------------------

bool FOnyxContentKit::MakeEffects()
{
	if (Effects.HitSpark)
	{
		return true;
	}
	TArray<FOnyxPixelCanvas> SparkFrames;
	TArray<FOnyxPixelCanvas> DustFrames;
	TArray<FOnyxPixelCanvas> FireFrames;
	TArray<FOnyxPixelCanvas> WispFrames;
	TArray<FOnyxPixelCanvas> WispBurstFrames;
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		DustFrames.Add(DrawDust(Frame));
		FireFrames.Add(DrawBurst(Frame, FColor(0xff, 0xf6, 0xc8), FColor(0xff, 0xb0, 0x3a), FColor(0xd8, 0x4a, 0x1c)));
		WispBurstFrames.Add(DrawBurst(Frame, FColor(0xf0, 0xff, 0xff), FColor(0x8c, 0xe8, 0xff), FColor(0x3a, 0x9c, 0xe0)));
	}
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		SparkFrames.Add(DrawHitSpark(Frame));
		WispFrames.Add(DrawWisp(Frame));
	}
	Effects.HitSpark = DrawnFlipbook(SparkFrames, TEXT("FB_HitSpark"), 22.f, true);
	Effects.Dust = DrawnFlipbook(DustFrames, TEXT("FB_Dust"), 16.f, true);
	Effects.FireBurst = DrawnFlipbook(FireFrames, TEXT("FB_FireBurst"), 18.f, true);
	Effects.Wisp = DrawnFlipbook(WispFrames, TEXT("FB_Wisp"), 12.f, true);
	Effects.WispBurst = DrawnFlipbook(WispBurstFrames, TEXT("FB_WispBurst"), 18.f, true);
	TArray<FOnyxPixelCanvas> GatherFrames;
	TArray<FOnyxPixelCanvas> MendedFrames;
	TArray<FOnyxPixelCanvas> ClashFrames;
	for (int32 Frame = 0; Frame < 8; ++Frame)
	{
		GatherFrames.Add(DrawGather(Frame));
	}
	for (int32 Frame = 0; Frame < 6; ++Frame)
	{
		MendedFrames.Add(DrawMended(Frame));
	}
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		ClashFrames.Add(DrawClash(Frame));
	}
	Effects.Gather = DrawnFlipbook(GatherFrames, TEXT("FB_Gather"), 9.f, true);
	Effects.Mended = DrawnFlipbook(MendedFrames, TEXT("FB_Mended"), 16.f, true);
	Effects.Clash = DrawnFlipbook(ClashFrames, TEXT("FB_Clash"), 20.f, true);
	if (!Effects.HitSpark || !Effects.Dust || !Effects.FireBurst || !Effects.Wisp || !Effects.WispBurst || !Effects.Gather || !Effects.Mended || !Effects.Clash)
	{
		Effects.HitSpark = nullptr;
		return Fail(Error.IsEmpty() ? INVTEXT("Could not create the hit, dust and burst effects.") : Error);
	}
	return true;
}

bool FOnyxContentKit::MakeSounds()
{
	if (Sounds.Swing)
	{
		return true;
	}
	using V = FOnyxSoundSynth;
	const FString Folder = Root / TEXT("Sounds");
	auto Make = [&Folder](const TCHAR* Name, std::initializer_list<FOnyxSoundVoice> Voices, float Volume = 0.8f, float Crush = 0.f)
	{
		FOnyxSoundRecipe Recipe;
		Recipe.Voices = Voices;
		Recipe.Volume = Volume;
		Recipe.Crush = Crush;
		Recipe.Seed = GetTypeHash(FString(Name));
		return FOnyxSoundSynth::CreateSound(Recipe, Folder, FString(TEXT("SFX_")) + Name);
	};
	// A blade cutting the air: a swoosh (a band of air sweeping down), a thin ring of steel over it.
	FOnyxSoundVoice Air = V::Voice(EOnyxWave::Noise, 44000.f, 44000.f, 1.f).Shape(0.015f, 0.03f, 0.12f, 2.f).Band(2800.f, 650.f, 3.f);
	FOnyxSoundVoice Edge = V::Voice(EOnyxWave::Noise, 44000.f, 44000.f, 0.35f).Shape(0.01f, 0.f, 0.1f, 3.f).Band(5200.f, 3200.f, 6.f);
	FOnyxSoundVoice Steel = V::Voice(EOnyxWave::Sine, 3300.f, 2700.f, 0.12f).Shape(0.005f, 0.f, 0.14f, 3.f);
	Sounds.Swing = Make(TEXT("Swing"), { Air, Edge, Steel }, 0.9f);
	// A blow landing: a crisp slice over a soft thud (nothing tonal: no boop, no 8-bit crunch).
	Sounds.Hit = Make(TEXT("Hit"), {
		V::Voice(EOnyxWave::Noise, 44000.f, 44000.f, 0.6f).Shape(0.001f, 0.004f, 0.05f, 3.f).Band(5000.f, 2600.f, 1.2f),
		V::Voice(EOnyxWave::Noise, 44000.f, 44000.f, 1.f).Shape(0.001f, 0.01f, 0.13f, 3.f).LowPass(1000.f, 220.f),
		V::Voice(EOnyxWave::Sine, 120.f, 60.f, 0.35f).Shape(0.002f, 0.f, 0.09f, 3.f) }, 0.95f);
	// Blades meeting: a click and two bright tones ringing out against each other.
	FOnyxSoundVoice RingA = V::Voice(EOnyxWave::Sine, 2093.f, 2080.f, 0.5f).Shape(0.001f, 0.f, 0.6f, 4.f);
	RingA.VibratoHz = 7.f;
	RingA.VibratoDepth = 0.003f;
	FOnyxSoundVoice Click = V::Voice(EOnyxWave::Noise, 40000.f, 40000.f, 0.6f).Shape(0.001f, 0.f, 0.03f, 3.f);
	Click.High = 2000.f;
	Sounds.Clash = Make(TEXT("Clash"), { Click, RingA, V::Voice(EOnyxWave::Sine, 2794.f, 2790.f, 0.35f).Shape(0.001f, 0.f, 0.45f, 4.f),
		V::Voice(EOnyxWave::Triangle, 3520.f, 3500.f, 0.2f).Shape(0.001f, 0.f, 0.25f, 4.f) }, 0.75f);
	// Off the ground: cloth and air rising.
	FOnyxSoundVoice Lift = V::Voice(EOnyxWave::Noise, 20000.f, 20000.f, 0.5f).Shape(0.01f, 0.f, 0.13f, 2.f).LowPass(1800.f, 3600.f);
	Lift.High = 300.f;
	Sounds.Jump = Make(TEXT("Jump"), { Lift, V::Voice(EOnyxWave::Triangle, 220.f, 340.f, 0.25f).Shape(0.005f, 0.f, 0.08f, 2.f) }, 0.4f);
	// Down again: a soft thud.
	Sounds.Land = Make(TEXT("Land"), { V::Voice(EOnyxWave::Noise, 8000.f, 8000.f, 0.7f).Shape(0.002f, 0.f, 0.09f, 3.f).LowPass(900.f, 300.f),
		V::Voice(EOnyxWave::Sine, 110.f, 60.f, 0.6f).Shape(0.002f, 0.f, 0.08f, 2.f) }, 0.5f);
	// A footfall: soft, padded, low.
	FOnyxSoundVoice Fall = V::Voice(EOnyxWave::Noise, 44000.f, 44000.f, 1.f).Shape(0.002f, 0.004f, 0.06f, 3.f).LowPass(700.f, 300.f);
	Fall.High = 80.f;
	Sounds.Step = Make(TEXT("Step"), { Fall, V::Voice(EOnyxWave::Sine, 95.f, 70.f, 0.25f).Shape(0.002f, 0.f, 0.04f, 3.f) }, 0.5f);
	// A dash: air torn past, rising then falling away.
	FOnyxSoundVoice Rush = V::Voice(EOnyxWave::Noise, 25000.f, 25000.f, 0.6f).Shape(0.03f, 0.05f, 0.18f, 2.f).LowPass(800.f, 3200.f);
	Rush.High = 400.f;
	FOnyxSoundVoice Tail = V::Voice(EOnyxWave::Noise, 15000.f, 15000.f, 0.4f).Shape(0.01f, 0.f, 0.15f, 2.f).LowPass(3000.f, 600.f);
	Tail.Delay = 0.08f;
	Sounds.Dash = Make(TEXT("Dash"), { Rush, Tail }, 0.5f);
	// Hurt: a harsh falling tone over a blow.
	FOnyxSoundVoice Pain = V::Voice(EOnyxWave::Square, 330.f, 140.f, 0.5f).Shape(0.002f, 0.f, 0.22f, 2.f).LowPass(2500.f, 1200.f);
	Pain.Duty = 0.3f;
	Sounds.Hurt = Make(TEXT("Hurt"), { Pain, V::Voice(EOnyxWave::Noise, 30000.f, 30000.f, 0.5f).Shape(0.001f, 0.f, 0.12f, 3.f).LowPass(4000.f, 800.f),
		V::Voice(EOnyxWave::Sine, 90.f, 45.f, 0.6f).Shape(0.002f, 0.f, 0.2f, 2.f) }, 0.8f, 0.3f);
	// Falling: a long tone sinking, wavering.
	FOnyxSoundVoice Sink = V::Voice(EOnyxWave::Square, 440.f, 55.f, 0.5f).Shape(0.005f, 0.f, 0.9f, 1.5f).LowPass(2000.f, 400.f);
	Sink.VibratoHz = 7.f;
	Sink.VibratoDepth = 0.04f;
	Sounds.Death = Make(TEXT("Death"), { Sink, V::Voice(EOnyxWave::Noise, 12000.f, 12000.f, 0.4f).Shape(0.005f, 0.f, 0.7f, 2.f).LowPass(1500.f, 200.f) }, 0.75f, 0.2f);
	// A foe going down: a burst and a falling growl.
	Sounds.FoeDeath = Make(TEXT("FoeDeath"), { V::Voice(EOnyxWave::Noise, 20000.f, 20000.f, 0.7f).Shape(0.002f, 0.f, 0.35f, 2.5f).LowPass(3000.f, 300.f),
		V::Voice(EOnyxWave::Saw, 300.f, 60.f, 0.35f).Shape(0.005f, 0.f, 0.3f, 2.f).LowPass(1500.f, 500.f) }, 0.7f, 0.2f);
	// A coin: two quick bright notes.
	FOnyxSoundVoice NoteA = V::Voice(EOnyxWave::Square, 1318.f, 1318.f, 0.35f).Shape(0.001f, 0.05f, 0.04f, 2.f);
	NoteA.Duty = 0.25f;
	FOnyxSoundVoice NoteB = V::Voice(EOnyxWave::Square, 1975.f, 1975.f, 0.35f).Shape(0.001f, 0.02f, 0.25f, 3.f);
	NoteB.Duty = 0.25f;
	NoteB.Delay = 0.06f;
	Sounds.Coin = Make(TEXT("Coin"), { NoteA, NoteB }, 0.45f);
	// Energy gathering: a shimmer rising.
	FOnyxSoundVoice Rise = V::Voice(EOnyxWave::Sine, 440.f, 880.f, 0.3f).Shape(0.3f, 0.3f, 0.3f, 2.f);
	Rise.TremoloHz = 12.f;
	Rise.TremoloDepth = 0.5f;
	FOnyxSoundVoice Hush = V::Voice(EOnyxWave::Noise, 20000.f, 20000.f, 0.15f).Shape(0.4f, 0.f, 0.4f, 2.f).LowPass(2500.f, 2500.f);
	Hush.High = 800.f;
	Sounds.Gather = Make(TEXT("Gather"), { Rise, Hush, V::Voice(EOnyxWave::Triangle, 660.f, 1320.f, 0.15f).Shape(0.3f, 0.f, 0.5f, 2.f) }, 0.4f);
	// A wound mended: a bright chord climbing, a sparkle.
	FOnyxSoundVoice Sparkle = V::Voice(EOnyxWave::Noise, 40000.f, 40000.f, 0.1f).Shape(0.005f, 0.f, 0.3f, 3.f);
	Sparkle.High = 5000.f;
	TArray<FOnyxSoundVoice> Chord = { Sparkle };
	const float Notes[] = { 880.f, 1108.f, 1320.f, 1760.f };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FOnyxSoundVoice Note = V::Voice(EOnyxWave::Sine, Notes[Index], Notes[Index], 0.3f).Shape(0.003f, 0.f, 0.5f + Index * 0.08f, 3.f);
		Note.Delay = Index * 0.06f;
		Chord.Add(Note);
	}
	{
		FOnyxSoundRecipe Recipe;
		Recipe.Voices = Chord;
		Recipe.Volume = 0.55f;
		Sounds.Mended = FOnyxSoundSynth::CreateSound(Recipe, Folder, TEXT("SFX_Mended"));
	}
	// A resting place: a bell.
	FOnyxSoundVoice Bell = V::Voice(EOnyxWave::Sine, 523.f, 523.f, 0.5f).Shape(0.002f, 0.f, 1.6f, 3.f);
	Bell.TremoloHz = 4.f;
	Bell.TremoloDepth = 0.15f;
	Sounds.Checkpoint = Make(TEXT("Checkpoint"), { Bell, V::Voice(EOnyxWave::Sine, 1046.f, 1046.f, 0.25f).Shape(0.002f, 0.f, 1.2f, 3.f),
		V::Voice(EOnyxWave::Sine, 1568.f, 1568.f, 0.15f).Shape(0.002f, 0.f, 0.8f, 3.f) }, 0.55f);
	// Something thrown: a whoosh dropping in pitch.
	Sounds.Shot = Make(TEXT("Shot"), { V::Voice(EOnyxWave::Noise, 20000.f, 20000.f, 0.5f).Shape(0.005f, 0.f, 0.18f, 2.f).LowPass(2500.f, 600.f),
		V::Voice(EOnyxWave::Sine, 520.f, 180.f, 0.4f).Shape(0.003f, 0.f, 0.15f, 2.f) }, 0.55f);
	// Something bursting: a soft boom.
	Sounds.Burst = Make(TEXT("Burst"), { V::Voice(EOnyxWave::Noise, 15000.f, 15000.f, 0.7f).Shape(0.002f, 0.f, 0.25f, 2.5f).LowPass(1800.f, 250.f),
		V::Voice(EOnyxWave::Sine, 140.f, 50.f, 0.4f).Shape(0.002f, 0.f, 0.15f, 2.f) }, 0.6f);
	Coins.Sound = Sounds.Coin;
	if (!Sounds.Swing || !Sounds.Hit || !Sounds.Clash || !Sounds.Jump || !Sounds.Land || !Sounds.Step || !Sounds.Dash || !Sounds.Hurt || !Sounds.Death
		|| !Sounds.FoeDeath || !Sounds.Coin || !Sounds.Gather || !Sounds.Mended || !Sounds.Checkpoint || !Sounds.Shot || !Sounds.Burst)
	{
		Sounds.Swing = nullptr;
		return Fail(INVTEXT("Could not make the sound effects (see the log)."));
	}
	return true;
}

bool FOnyxContentKit::MakeCoins(FCoins& Out)
{
	TArray<FOnyxPixelCanvas> CoinFrames;
	TArray<FOnyxPixelCanvas> SparkleFrames;
	for (int32 Frame = 0; Frame < 6; ++Frame)
	{
		CoinFrames.Add(DrawCoin(Frame));
	}
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		SparkleFrames.Add(DrawSparkle(Frame));
	}
	Out.Coin = DrawnFlipbook(CoinFrames, TEXT("FB_Coin"), 10.f);
	Out.Sparkle = DrawnFlipbook(SparkleFrames, TEXT("FB_CoinSparkle"), 16.f);
	const FOnyxPixelCanvas DropArt = DrawDrop();
	DropIcon = FOnyxAssetUtils::CreateTexture(Root / TEXT("UI"), TEXT("T_HUD_Drop"), DropArt.Pixels, DropArt.Size);
	CoinIcon = FOnyxAssetUtils::CreateTexture(Root / TEXT("UI"), TEXT("T_HUD_Coin"), CoinFrames[0].Pixels, CoinFrames[0].Size);
	if (!Out.Coin || !Out.Sparkle || !DropIcon || !CoinIcon)
	{
		return Fail(INVTEXT("Could not create the coin art."));
	}
	FOnyxAssetUtils::SavePackages({ DropIcon->GetPackage(), CoinIcon->GetPackage() });
	Out.Sound = Sounds.Coin;
	Coins = Out;
	return true;
}

UWidgetBlueprint* FOnyxContentKit::MakeHUD(const FString& Name, const FHudLook& Look)
{
	// Health as a row of drops of light, the energy cells under it and the coins under them, top left. A child of the Onyx
	// HUD with a layout of its own: Onyx2D fills the parts named PipRow, EnergyRow, CoinPanel and CoinText.
	const FString AssetName = FString::Printf(TEXT("WBP_%sHUD"), *Name);
	const FString Path = Root / TEXT("UI") / AssetName;
	ClearAssetPath(Path);
	UWidgetBlueprint* Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(CreatePackage(*Path), FName(AssetName), BPTYPE_Normal,
		UOnyxHUDWidget::StaticClass(), UCanvasPanel::StaticClass(), NAME_None, false);
	UCanvasPanel* Canvas = Blueprint ? Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget) : nullptr;
	if (!Canvas)
	{
		Fail(FText::Format(INVTEXT("Could not create {0}."), FText::FromString(AssetName)));
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Blueprint);
#if WITH_EDITORONLY_DATA
	Canvas->bExpandedInDesigner = true;
#endif
	UVerticalBox* Status = AddWidget<UVerticalBox>(*Blueprint, Canvas, TEXT("Status"));
	if (UCanvasPanelSlot* StatusSlot = Cast<UCanvasPanelSlot>(Status->Slot))
	{
		StatusSlot->SetAnchors(FAnchors(0.f, 0.f));
		StatusSlot->SetPosition(FVector2D(56.f, 44.f));
		StatusSlot->SetAutoSize(true);
	}
	AddWidget<UHorizontalBox>(*Blueprint, Status, TEXT("PipRow"));
	UHorizontalBox* EnergyRow = AddWidget<UHorizontalBox>(*Blueprint, Status, TEXT("EnergyRow"));
	if (UVerticalBoxSlot* EnergySlot = Cast<UVerticalBoxSlot>(EnergyRow->Slot))
	{
		EnergySlot->SetPadding(FMargin(4.f, 10.f, 0.f, 0.f));
	}
	UHorizontalBox* CoinPanel = AddWidget<UHorizontalBox>(*Blueprint, Status, TEXT("CoinPanel"));
	if (UVerticalBoxSlot* CoinSlot = Cast<UVerticalBoxSlot>(CoinPanel->Slot))
	{
		CoinSlot->SetPadding(FMargin(2.f, 12.f, 0.f, 0.f));
	}
	UImage* Icon = AddWidget<UImage>(*Blueprint, CoinPanel, TEXT("CoinIcon"));
	Icon->SetBrushFromTexture(CoinIcon);
	Icon->SetDesiredSizeOverride(FVector2D(30.f, 30.f));
	if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(Icon->Slot))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}
	UTextBlock* Count = AddWidget<UTextBlock>(*Blueprint, CoinPanel, TEXT("CoinText"));
	Count->SetText(INVTEXT("0"));
	FSlateFontInfo Font = Count->GetFont();
	Font.Size = 24;
	Count->SetFont(Font);
	Count->SetColorAndOpacity(FSlateColor(Look.TextColor));
	Count->SetShadowOffset(FVector2D(0.f, 2.f));
	Count->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.02f, 0.8f));
	if (UHorizontalBoxSlot* CountSlot = Cast<UHorizontalBoxSlot>(Count->Slot))
	{
		CountSlot->SetVerticalAlignment(VAlign_Center);
	}

	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
	UOnyxHUDWidget* Defaults = Blueprint->GeneratedClass ? Cast<UOnyxHUDWidget>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (Blueprint->Status == BS_Error || !Defaults)
	{
		Fail(FText::Format(INVTEXT("{0} has errors (see the log)."), FText::FromString(AssetName)));
		return nullptr;
	}
	// Each health point is a drop of light, three screen pixels per art pixel.
	Defaults->Modify();
	Defaults->PipTexture = Look.Pip ? Look.Pip : DropIcon;
	Defaults->PipTextureSize = Look.PipSize;
	Defaults->EmptyPipColor = Look.EmptyPipColor;
	Defaults->EnergyColor = Look.EnergyColor;
	Blueprint->MarkPackageDirty();
	Dirty.Add(Blueprint->GetPackage());
	return Blueprint;
}

UBlueprint* FOnyxContentKit::MakeGameMode(const FString& Name, UBlueprint* Hero, const FHudLook& Look)
{
	UWidgetBlueprint* HUD = MakeHUD(Name, Look);
	UBlueprint* GameMode = HUD && Hero ? FOnyxAssetUtils::CreateBlueprint(Root, FString::Printf(TEXT("BP_%sGameMode"), *Name), AOnyxGameMode::StaticClass()) : nullptr;
	if (!GameMode)
	{
		Fail(Error.IsEmpty() ? INVTEXT("Could not create the game mode.") : Error);
		return nullptr;
	}
	AOnyxGameMode* Defaults = CastChecked<AOnyxGameMode>(GameMode->GeneratedClass->GetDefaultObject());
	Defaults->Modify();
	Defaults->DefaultPawnClass = Hero->GeneratedClass;
	Defaults->HUDWidgetClass = FSoftClassPath(HUD->GeneratedClass);
	GameMode->MarkPackageDirty();
	Dirty.Add(GameMode->GetPackage());
	return GameMode;
}

// --- Placing ----------------------------------------------------------------------------------------------------------

void FOnyxContentKit::FindMapCorner(UWorld* World)
{
	for (TActorIterator<APaperTileMapActor> It(World); It; ++It)
	{
		MapCorner = It->GetRenderComponent()->GetTileCornerPosition(0, 0, 0, true);
	}
}

FVector FOnyxContentKit::WorldAt(float PixelX, float PixelY, float Depth) const
{
	return FVector(MapCorner.X + PixelX * PixelSize, Depth, MapCorner.Z - PixelY * PixelSize);
}

APaperSpriteActor* FOnyxContentKit::PlaceSprite(UWorld* World, UPaperSprite* Sprite, float PixelX, float BottomPixelY, const FString& Label, FName Layer, float Depth,
	const FLinearColor& Color)
{
	if (!Sprite)
	{
		return nullptr;
	}
	APaperSpriteActor* Actor = Spawn<APaperSpriteActor>(World, WorldAt(PixelX, BottomPixelY, Depth), Label);
	UPaperSpriteComponent* Component = Actor->GetRenderComponent();
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetSprite(Sprite);
	Component->SetSpriteColor(Color);
	UOnyx2DLibrary::ConfigureSpriteLighting(Component, false);
	Actor->Tags = { FName(TEXT("Onyx.Brush.") + Sprite->GetPathName()) };
	if (!Layer.IsNone())
	{
		Actor->Tags.Add(FName(TEXT("Onyx.Layer.") + Layer.ToString()));
	}
	return Actor;
}

FOnyxContentKit::FLightLook FOnyxContentKit::Torch()
{
	return { EOnyxLightPreset::Torch, FLinearColor(1.f, 0.6f, 0.3f), 1.3f, 320.f, 0.28f, 0.6f, true };
}

FOnyxContentKit::FLightLook FOnyxContentKit::Candles()
{
	return { EOnyxLightPreset::Candle, FLinearColor(1.f, 0.72f, 0.42f), 0.9f, 190.f, 0.35f, 0.45f, false };
}

FOnyxContentKit::FLightLook FOnyxContentKit::Moonlight()
{
	return { EOnyxLightPreset::Window, FLinearColor(0.62f, 0.78f, 1.f), 1.f, 460.f, 0.f, 0.f, false };
}

AOnyxLight2D* FOnyxContentKit::AddLight(UWorld* World, const FVector2D& Pixel, const FLightLook& Look, const FString& Label)
{
	AOnyxLight2D* Light = Spawn<AOnyxLight2D>(World, WorldAt(Pixel.X, Pixel.Y), Label);
	Light->ApplyPreset(Look.Preset); // its flicker
	Light->Light2D->Color = Look.Color;
	Light->Light2D->Intensity = Look.Intensity;
	Light->Light2D->Radius = Look.Radius;
	Light->Light2D->bCastShadows = Look.bShadows;
	Light->bShowHalo = Look.HaloSize > 0.f;
	Light->HaloSize = FMath::Max(Look.HaloSize, 0.05f);
	Light->HaloIntensity = Look.HaloIntensity;
	Light->Preset = EOnyxLightPreset::Custom; // the values above are its own now
	Light->RerunConstructionScripts();
	return Light;
}

AOnyxPickup* FOnyxContentKit::PlaceCoin(UWorld* World, const FVector2D& Pixel, const FCoins& CoinArt, const FString& Label)
{
	AOnyxPickup* Pickup = Spawn<AOnyxPickup>(World, WorldAt(Pixel.X, Pixel.Y), Label);
	Pickup->Type = EOnyxPickupType::Coin;
	Pickup->Flipbook = CoinArt.Coin;
	Pickup->CollectEffect = CoinArt.Sparkle;
	Pickup->CollectSound = CoinArt.Sound;
	Pickup->Amount = 1;
	Pickup->BobHeight = 6.f;
	Pickup->RerunConstructionScripts();
	return Pickup;
}

void FOnyxContentKit::ClearAssetPath(const FString& PackageName)
{
	TArray<FAssetData> Found;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPackageName(FName(PackageName), Found);
	if (!Found.IsEmpty())
	{
		ObjectTools::DeleteAssets(Found, false);
	}
	if (UPackage* Package = FindPackage(nullptr, *PackageName))
	{
		TArray<UObject*> Left;
		GetObjectsWithOuter(Package, Left, EGetObjectsFlags::None);
		for (UObject* Object : Left)
		{
			Object->Rename(*MakeUniqueObjectName(GetTransientPackage(), Object->GetClass(), Object->GetFName()).ToString(), GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
		}
	}
}

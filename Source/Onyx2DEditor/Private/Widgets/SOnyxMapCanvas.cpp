// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxMapCanvas.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerStart.h"
#include "Builders/OnyxAssetUtils.h"
#include "Camera/OnyxCameraComponent.h"
#include "Engine/Blueprint.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "Lighting/OnyxLightingEnvironment.h"
#include "World/OnyxParallaxBackground.h"
#include "Level/OnyxBrushLibrary.h"
#include "Level/OnyxMapPainter.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "PaperTileMap.h"
#include "Rendering/SlateRenderer.h"
#include "PaperTileSet.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Level/OnyxActorPreview.h"
#include "Styling/SlateIconFinder.h"
#include "ToolsWindow/OnyxToolWidgets.h"

namespace
{
	constexpr float MinZoom = 0.25f;
	constexpr float MaxZoom = 32.f;
	const FLinearColor BackgroundColor(0.018f, 0.02f, 0.026f);
	const FLinearColor MapColor(0.045f, 0.05f, 0.062f);
	const FLinearColor CanvasGridColor(1.f, 1.f, 1.f, 0.055f);

	void DrawRect(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Size, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Min)), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
	}

	void DrawOutline(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Size, const FLinearColor& Color, float Thickness = 1.f)
	{
		const TArray<FVector2f> Points = { Min, Min + FVector2f(Size.X, 0.f), Min + Size, Min + FVector2f(0.f, Size.Y), Min };
		FSlateDrawElement::MakeLines(Out, LayerId, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	/** Edges of the map's border, grabbed to grow or shrink it. */
	enum EOnyxMapEdge : int32
	{
		MapEdgeLeft = 1,
		MapEdgeRight = 2,
		MapEdgeTop = 4,
		MapEdgeBottom = 8,
	};
	/** How far outside the border (screen pixels) the mouse grabs it. */
	constexpr float MapEdgeReach = 12.f;
	const FLinearColor MapEdgeRemoveColor(0.95f, 0.22f, 0.2f, 0.32f);

	EMouseCursor::Type MapEdgeCursor(int32 Edges)
	{
		const bool bHorizontal = (Edges & (MapEdgeLeft | MapEdgeRight)) != 0;
		const bool bVertical = (Edges & (MapEdgeTop | MapEdgeBottom)) != 0;
		if (bHorizontal && bVertical)
		{
			const bool bFalling = ((Edges & MapEdgeLeft) != 0) == ((Edges & MapEdgeTop) != 0); // top-left or bottom-right corner
			return bFalling ? EMouseCursor::ResizeSouthEast : EMouseCursor::ResizeSouthWest;
		}
		return bHorizontal ? EMouseCursor::ResizeLeftRight : EMouseCursor::ResizeUpDown;
	}

	/** A short label in a dark box near Position, kept inside the canvas. */
	void DrawCanvasLabel(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& Geometry, const FString& Text, FVector2f Position)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		const FVector2f TextSize(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, LabelFont));
		const FVector2f Padding(8.f, 4.f);
		const FVector2f BoxSize = TextSize + Padding * 2.f;
		const FVector2f Area(Geometry.GetLocalSize());
		Position.X = FMath::Clamp(Position.X, 4.f, FMath::Max(Area.X - BoxSize.X - 4.f, 4.f));
		Position.Y = FMath::Clamp(Position.Y, 4.f, FMath::Max(Area.Y - BoxSize.Y - 4.f, 4.f));
		DrawRect(Out, LayerId, Geometry, Position, BoxSize, FLinearColor(0.015f, 0.018f, 0.025f, 0.94f));
		FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position + Padding)), Text, LabelFont,
			ESlateDrawEffect::None, FLinearColor::White);
	}
}

// Sets the view from the console (scripted captures, no mouse): named, unity builds merge files.
namespace OnyxCanvasView
{
	float PendingZoom = 0.f;
	FIntPoint PendingCell(INDEX_NONE, INDEX_NONE);

	FAutoConsoleCommand ZoomCommand(TEXT("Onyx2D.PainterZoom"),
		TEXT("Zooms the Level Painter's canvas: Onyx2D.PainterZoom <zoom> [column row] (centred on that cell, else on the map's middle)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			PendingZoom = Args.IsEmpty() ? 0.f : FCString::Atof(*Args[0]);
			PendingCell = Args.Num() >= 3 ? FIntPoint(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2])) : FIntPoint(INDEX_NONE, INDEX_NONE);
		}));
}

void SOnyxMapCanvas::Construct(const FArguments& InArgs)
{
	Painter = InArgs._Painter;
	ShowGrid = InArgs._ShowGrid;
	ShowCollision = InArgs._ShowCollision;
	ShowParallax = InArgs._ShowParallax;
	SelectedImageLayer = InArgs._SelectedImageLayer;
	HiddenImageLayers = InArgs._HiddenImageLayers;
	HiddenObjectLayers = InArgs._HiddenObjectLayers;
	HiddenLayers = InArgs._HiddenLayers;
	PreviewStamp = InArgs._PreviewStamp;
	PreviewBrush = InArgs._PreviewBrush;
	PreviewRect = InArgs._PreviewRect;
	SelectedActor = InArgs._SelectedActor;
	OnStroke = InArgs._OnStroke;
	OnKey = InArgs._OnKey;
	OnQueryCursor = InArgs._OnQueryCursor;
	OnResizeMap = InArgs._OnResizeMap;
	TileBrush.DrawAs = ESlateBrushDrawType::Image;
	SpriteBrush.DrawAs = ESlateBrushDrawType::Image;
	ParallaxBrush.DrawAs = ESlateBrushDrawType::Image;
	SetClipping(EWidgetClipping::ClipToBounds);
}

float SOnyxMapCanvas::CellPixels() const
{
	return Painter->GetTileSize().X * Zoom;
}

FVector2f SOnyxMapCanvas::CellToLocal(const FIntPoint& Cell) const
{
	const FIntPoint Tile = Painter->GetTileSize();
	return FVector2f(Pan.X + Cell.X * Tile.X * Zoom, Pan.Y + Cell.Y * Tile.Y * Zoom);
}

FIntPoint SOnyxMapCanvas::LocalToCell(const FVector2D& Local) const
{
	const FIntPoint Tile = Painter->GetTileSize();
	return FIntPoint(FMath::FloorToInt((Local.X - Pan.X) / (Tile.X * Zoom)), FMath::FloorToInt((Local.Y - Pan.Y) / (Tile.Y * Zoom)));
}

void SOnyxMapCanvas::FitTo(const FVector2D& Area) const
{
	const FIntPoint Size = Painter->GetMapSize();
	const FIntPoint Tile = Painter->GetTileSize();
	if (Size.X <= 0 || Size.Y <= 0 || Area.X <= 1.f || Area.Y <= 1.f)
	{
		return;
	}
	const FVector2D MapPixels(Size.X * Tile.X, Size.Y * Tile.Y);
	Zoom = FMath::Clamp(FMath::Min((Area.X - 48.f) / MapPixels.X, (Area.Y - 48.f) / MapPixels.Y), MinZoom, MaxZoom);
	Pan = (Area - MapPixels * Zoom) * 0.5f;
	bFitRequested = false;
	bKeepFitted = true;
	FittedArea = Area;
}

void SOnyxMapCanvas::DrawTile(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FPaperTileInfo& Tile,
	const FVector2f& Position, float Size, const FLinearColor& Tint) const
{
	UTexture2D* Texture = Tile.TileSet ? Tile.TileSet->GetTileSheetTexture() : nullptr;
	FVector2D UV;
	if (!Texture || !Tile.TileSet->GetTileUV(Tile.GetTileIndex(), UV))
	{
		return;
	}
	// UVs are measured against the texture as the GPU has it (a sheet padded to a power of two is bigger than the picture).
	const FVector2f TextureSize(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
	const FIntPoint TileSize = Tile.TileSet->GetTileSize();
	if (TextureSize.X <= 0.f || TextureSize.Y <= 0.f)
	{
		return;
	}
	TileBrush.SetResourceObject(Texture);
	TileBrush.ImageSize = FVector2D(TileSize);
	// Flips swap the UV corners; a diagonal flip (Tiled's rotations) also turns the quad a quarter.
	FVector2f UVMin(UV.X / TextureSize.X, UV.Y / TextureSize.Y);
	FVector2f UVMax((UV.X + TileSize.X) / TextureSize.X, (UV.Y + TileSize.Y) / TextureSize.Y);
	const bool bDiagonal = Tile.HasFlag(EPaperTileFlags::FlipDiagonal);
	if (Tile.HasFlag(EPaperTileFlags::FlipHorizontal) != bDiagonal)
	{
		Swap(UVMin.X, UVMax.X);
	}
	if (Tile.HasFlag(EPaperTileFlags::FlipVertical))
	{
		Swap(UVMin.Y, UVMax.Y);
	}
	TileBrush.SetUVRegion(FBox2f(UVMin, UVMax));
	const float Height = Size * TileSize.Y / FMath::Max(TileSize.X, 1);
	const FVector2f QuadSize(Size, Height);
	const FSlateRenderTransform Turn = bDiagonal ? FSlateRenderTransform(FQuat2f(UE_HALF_PI)) : FSlateRenderTransform();
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		Geometry.ToPaintGeometry(QuadSize, FSlateLayoutTransform(Position), Turn, FVector2f(0.5f, 0.5f)), &TileBrush, ESlateDrawEffect::None, Tint);
}

void SOnyxMapCanvas::DrawBrush(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FOnyxPainterBrush& Brush,
	const FVector2f& CellMin, const FVector2f& CellSize, float Opacity) const
{
	const FOnyxBrushLibrary& Library = FOnyxBrushLibrary::Get();
	UTexture2D* Texture = nullptr;
	FIntRect Rect;
	if (Library.GetPreviewSprite(Brush, Texture, Rect, FPlatformTime::Seconds()))
	{
		// The sprite itself, scaled with the map and standing on the cell bottom (it may be bigger than a cell).
		const FVector2f TextureSize(FMath::Max(Texture->GetSurfaceWidth(), 1.f), FMath::Max(Texture->GetSurfaceHeight(), 1.f));
		const float PixelScale = CellSize.X / FMath::Max(Painter->GetTileSize().X, 1);
		const FVector2f Size(Rect.Width() * PixelScale, Rect.Height() * PixelScale);
		SpriteBrush.SetResourceObject(Texture);
		SpriteBrush.ImageSize = FVector2D(Rect.Size());
		SpriteBrush.SetUVRegion(FBox2f(FVector2f(float(Rect.Min.X) / TextureSize.X, float(Rect.Min.Y) / TextureSize.Y),
			FVector2f(float(Rect.Max.X) / TextureSize.X, float(Rect.Max.Y) / TextureSize.Y)));
		const FVector2f Position(CellMin.X + (CellSize.X - Size.X) * 0.5f, CellMin.Y + CellSize.Y - Size.Y);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), &SpriteBrush,
			ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
		return;
	}

	// Gameplay pieces and Blueprints: colored box + icon, like LDtk entities.
	const float Inset = FMath::Min(CellSize.X * 0.08f, 3.f);
	DrawRect(OutDrawElements, LayerId, Geometry, CellMin + FVector2f(Inset), CellSize - FVector2f(Inset * 2.f), Brush.Color.CopyWithNewOpacity(0.28f * Opacity));
	DrawOutline(OutDrawElements, LayerId + 1, Geometry, CellMin + FVector2f(Inset), CellSize - FVector2f(Inset * 2.f), Brush.Color.CopyWithNewOpacity(Opacity), 1.5f);
	if (const FSlateBrush* Icon = Library.GetIcon(Brush))
	{
		const float IconSize = FMath::Min(CellSize.X, CellSize.Y) * 0.7f;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
			Geometry.ToPaintGeometry(FVector2f(IconSize), FSlateLayoutTransform(CellMin + (CellSize - FVector2f(IconSize)) * 0.5f)), Icon,
			ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
	}
}

bool SOnyxMapCanvas::GetAreaRect(const FOnyxPainterBrush& Brush, const AActor* Actor, const FVector2f& MapMin, FVector2f& OutMin, FVector2f& OutMax) const
{
	if (!Brush.IsArea() || !Actor)
	{
		return false;
	}
	const FBox Area = Brush.GetArea(Actor);
	OutMin = MapMin + FVector2f(Painter->WorldToMapPixels(FVector(Area.Min.X, 0.f, Area.Max.Z))) * Zoom;
	OutMax = MapMin + FVector2f(Painter->WorldToMapPixels(FVector(Area.Max.X, 0.f, Area.Min.Z))) * Zoom;
	return OutMax.X > OutMin.X && OutMax.Y > OutMin.Y;
}

void SOnyxMapCanvas::DrawArea(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FOnyxPainterBrush& Brush, const AActor* Actor,
	const FVector2f& Min, const FVector2f& Max, float Opacity, bool bSelected) const
{
	const FVector2f Size = Max - Min;
	if (Size.X < 1.f || Size.Y < 1.f)
	{
		return;
	}
	// The actor's own colours (a preview uses the class defaults).
	const AActor* LookSource = Actor ? Actor : (Brush.Class ? Brush.Class->GetDefaultObject<AActor>() : nullptr);
	const FOnyxAreaLook Look = Brush.GetAreaLook && LookSource ? Brush.GetAreaLook(LookSource) : FOnyxAreaLook();
	auto Fade = [Opacity](FLinearColor Color, float MinAlpha)
	{
		Color.A = FMath::Max(Color.A, MinAlpha) * Opacity;
		return Color;
	};

	// Top edge: rippling slowly for water (in map space, so panning doesn't move the waves).
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Size.X / 5.f), 1, 600);
	const float Time = float(FMath::Fmod(FPlatformTime::Seconds(), 1000.0));
	const float Amplitude = Look.bWavyEdge ? FMath::Min(1.2f * Zoom, Size.Y * 0.1f) : 0.f;
	TArray<FVector2f> Edge;
	Edge.Reserve(Steps + 1);
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float X = Min.X + Size.X * Step / Steps;
		const float MapX = (X - float(Pan.X)) / Zoom;
		const float Wave = Amplitude * (0.7f * FMath::Sin(Time * 2.f + MapX * 0.12f) + 0.3f * FMath::Sin(Time * 3.1f - MapX * 0.2f));
		Edge.Add(FVector2f(X, Min.Y + Wave));
	}
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), Edge, ESlateDrawEffect::None, Fade(Look.Edge, 0.9f), true, FMath::Clamp(Zoom * 0.75f, 1.5f, 3.f));

	// Body: light under the top edge, deep at the bottom. Its top follows the edge, so the ripples and the fill line up.
	const FLinearColor TopColor = Fade(Look.Top, 0.45f);
	const FLinearColor BottomColor = Fade(Look.Bottom, 0.7f);
	const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indexes;
	Vertices.Reserve(Edge.Num() * 2);
	Indexes.Reserve(Edge.Num() * 6);
	for (int32 Index = 0; Index < Edge.Num(); ++Index)
	{
		const FVector2f& Top = Edge[Index];
		const FLinearColor AtTop = FMath::Lerp(TopColor, BottomColor, FMath::Clamp((Top.Y - Min.Y) / Size.Y, 0.f, 1.f));
		// Slate packs vertex colours as sRGB.
		Vertices.Add(FSlateVertex::Make(Transform, Top, FVector2f(0.5f), AtTop.ToFColor(true)));
		Vertices.Add(FSlateVertex::Make(Transform, FVector2f(Top.X, Max.Y), FVector2f(0.5f), BottomColor.ToFColor(true)));
		if (Index > 0)
		{
			const SlateIndex Base = SlateIndex((Index - 1) * 2);
			Indexes.Append({ Base, SlateIndex(Base + 1), SlateIndex(Base + 2), SlateIndex(Base + 2), SlateIndex(Base + 1), SlateIndex(Base + 3) });
		}
	}
	if (Indexes.Num() > 0 && FSlateApplication::IsInitialized())
	{
		const FSlateResourceHandle White = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("GenericWhiteBox"));
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, White, Vertices, Indexes, nullptr, 0, 0);
	}

	// Name and size in cells, when there is room.
	const FIntPoint Tile = Painter->GetTileSize();
	const FVector2f Cells(Size.X / (Tile.X * Zoom), Size.Y / (Tile.Y * Zoom));
	if (Size.X > 70.f && Size.Y > 22.f)
	{
		const FText Label = FText::Format(INVTEXT("{0}  {1} x {2}"), Brush.Label, FMath::RoundToInt(Cells.X), FMath::RoundToInt(Cells.Y));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(FVector2f(Size.X, 16.f), FSlateLayoutTransform(Min + FVector2f(6.f, 6.f))),
			Label, FCoreStyle::GetDefaultFontStyle("Bold", 8), ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.8f * Opacity));
	}

	if (bSelected)
	{
		// Outline and handles: drag an edge or a corner to resize, the inside to move.
		DrawOutline(OutDrawElements, LayerId + 1, Geometry, Min - FVector2f(1.f), Size + FVector2f(2.f), OnyxTools::Accent(), 2.f);
		const float Handle = 7.f;
		for (const FVector2f& Point : { Min, FVector2f(Min.X + Size.X * 0.5f, Min.Y), FVector2f(Max.X, Min.Y), FVector2f(Max.X, Min.Y + Size.Y * 0.5f), Max,
			FVector2f(Min.X + Size.X * 0.5f, Max.Y), FVector2f(Min.X, Max.Y), FVector2f(Min.X, Min.Y + Size.Y * 0.5f) })
		{
			DrawRect(OutDrawElements, LayerId + 2, Geometry, Point - FVector2f(Handle * 0.5f), FVector2f(Handle), OnyxTools::Accent());
			DrawRect(OutDrawElements, LayerId + 2, Geometry, Point - FVector2f(Handle * 0.5f - 1.5f), FVector2f(Handle - 3.f), FLinearColor(0.05f, 0.06f, 0.08f));
		}
	}
}

bool SOnyxMapCanvas::DrawActorSprites(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const AActor* Actor,
	const FVector2f& MapMin, FVector2f& OutMin, FVector2f& OutMax) const
{
	TArray<FOnyxSpriteDraw> Draws;
	OnyxActorPreview::Gather(Actor, FPlatformTime::Seconds(), Draws);
	if (Draws.IsEmpty())
	{
		return false;
	}
	auto ToLocal = [&](double X, double Z)
	{
		const FVector2D Pixels = Painter->WorldToMapPixels(FVector(X, 0.0, Z));
		return MapMin + FVector2f(Pixels) * Zoom;
	};
	ActorBrushes.SetNum(FMath::Max(ActorBrushes.Num(), Draws.Num()));
	bool bFirst = true;
	for (int32 Index = 0; Index < Draws.Num(); ++Index)
	{
		const FOnyxSpriteDraw& Draw = Draws[Index];
		const FVector2f TopLeft = ToLocal(Draw.WorldMin.X, Draw.WorldMax.Y);
		const FVector2f BottomRight = ToLocal(Draw.WorldMax.X, Draw.WorldMin.Y);
		const FVector2f Min(FMath::Min(TopLeft.X, BottomRight.X), FMath::Min(TopLeft.Y, BottomRight.Y));
		const FVector2f Max(FMath::Max(TopLeft.X, BottomRight.X), FMath::Max(TopLeft.Y, BottomRight.Y));
		FSlateBrush& Brush = ActorBrushes[Index];
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Draw.Texture);
		Brush.SetUVRegion(Draw.bFlipX ? FBox2f(FVector2f(Draw.UV.Max.X, Draw.UV.Min.Y), FVector2f(Draw.UV.Min.X, Draw.UV.Max.Y)) : Draw.UV);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(Max - Min, FSlateLayoutTransform(Min)), &Brush, ESlateDrawEffect::None, Draw.Color);
		OutMin = bFirst ? Min : FVector2f(FMath::Min(OutMin.X, Min.X), FMath::Min(OutMin.Y, Min.Y));
		OutMax = bFirst ? Max : FVector2f(FMath::Max(OutMax.X, Max.X), FMath::Max(OutMax.Y, Max.Y));
		bFirst = false;
	}
	return true;
}

FVector2D SOnyxMapCanvas::GetGameViewHalfSize(UWorld* World) const
{
	if (ViewSizeWorld.Get() == World && ViewHalfSize.X > 0.0)
	{
		return ViewHalfSize;
	}
	float ViewHeight = FOnyxCameraSettings().ViewHeight;
	const AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	UClass* Mode = Settings && Settings->DefaultGameMode ? Settings->DefaultGameMode.Get() : nullptr;
	if (!Mode)
	{
		Mode = LoadClass<AGameModeBase>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode());
	}
	UClass* PawnClass = Mode ? Mode->GetDefaultObject<AGameModeBase>()->DefaultPawnClass.Get() : nullptr;
	const UOnyxCameraComponent* Camera = nullptr;
	for (UClass* Class = PawnClass; Class && !Camera; Class = Class->GetSuperClass())
	{
		// A Blueprint's components are templates in it; a C++ pawn's are on its defaults.
		if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Class))
		{
			Camera = FOnyxAssetUtils::FindComponentTemplate<UOnyxCameraComponent>(Blueprint);
		}
		else if (const AActor* Defaults = Class->GetDefaultObject<AActor>())
		{
			Camera = Defaults->FindComponentByClass<UOnyxCameraComponent>();
		}
	}
	if (Camera)
	{
		ViewHeight = Camera->Settings.ViewHeight;
	}
	ViewSizeWorld = World;
	ViewHalfSize = FVector2D(ViewHeight * 0.5 * 16.0 / 9.0, ViewHeight * 0.5); // a 16:9 screen
	return ViewHalfSize;
}

// Where an image layer is (named: unity builds merge files).
namespace OnyxCanvasBackdrop
{
	/** One copy of the layer's picture is this wide (cm); 0 for a plain colour or a mist. */
	double PictureWidth(const FOnyxParallaxLayer& Layer)
	{
		// The picture's shape from its imported size, as the game builds it (the GPU size is a placeholder while it compiles).
		const UTexture2D* Texture = Layer.Material.IsNull() ? Layer.Texture.Get() : nullptr;
		const FIntPoint Pixels = Texture ? Texture->GetImportedSize() : FIntPoint::ZeroValue;
		return Pixels.X > 0 && Pixels.Y > 0 ? Layer.Height * Pixels.X / Pixels.Y : 0.0;
	}

	/** The middle of the layer's picture (world X, Z), moved by its Factor of the camera's travel (Delta). */
	FVector2D Middle(const AOnyxParallaxBackground& Background, const FOnyxParallaxLayer& Layer, const FVector& Delta)
	{
		const FVector Anchor = Background.GetActorLocation();
		return FVector2D(Anchor.X + Layer.OffsetX + Delta.X * Layer.Factor.X, Anchor.Z + Layer.OffsetZ + Delta.Z * Layer.Factor.Y);
	}

	/** The layer's own picture (world X / Z box): a repeating layer's copy nearest NearX (the view's middle); a colour band's whole width. */
	FBox2D OwnPicture(const AOnyxParallaxBackground& Background, const FOnyxParallaxLayer& Layer, const FVector& Delta, double NearX)
	{
		const FVector2D At = Middle(Background, Layer, Delta);
		const double Width = PictureWidth(Layer);
		const double Half = Layer.Height * 0.5;
		if (Width <= 0.0)
		{
			return FBox2D(FVector2D(At.X - Background.TotalWidth * 0.5, At.Y - Half), FVector2D(At.X + Background.TotalWidth * 0.5, At.Y + Half));
		}
		if (!Layer.bRepeat)
		{
			return FBox2D(FVector2D(At.X - Width * 0.5, At.Y - Half), FVector2D(At.X + Width * 0.5, At.Y + Half));
		}
		// Copies start at the strip's left edge (as the material tiles it).
		const double Left = At.X - Background.TotalWidth * 0.5;
		const double Copies = FMath::Max(FMath::CeilToDouble(Background.TotalWidth / Width), 1.0);
		const double CopyLeft = Left + FMath::Clamp(FMath::FloorToDouble((NearX - Left) / Width), 0.0, Copies - 1.0) * Width;
		return FBox2D(FVector2D(CopyLeft, At.Y - Half), FVector2D(CopyLeft + Width, At.Y + Half));
	}
}

const AOnyxParallaxBackground* SOnyxMapCanvas::FindParallax() const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const AOnyxParallaxBackground* Background = ParallaxActor.Get();
	if (!Background || Background->GetWorld() != World)
	{
		Background = nullptr;
		for (TActorIterator<AOnyxParallaxBackground> It(World); It; ++It)
		{
			Background = *It;
			break;
		}
		ParallaxActor = Background;
	}
	return Background;
}

FVector SOnyxMapCanvas::GetParallaxCameraDelta() const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !Painter || !Painter->FindMapActor() || !ShowParallax.Get())
	{
		return FVector::ZeroVector;
	}
	const FIntPoint MapCells = Painter->GetMapSize();
	const FIntPoint TileSize = Painter->GetTileSize();
	const FVector CornerA = Painter->MapPixelsToWorld(FVector2D::ZeroVector);
	const FVector CornerB = Painter->MapPixelsToWorld(FVector2D(MapCells.X * TileSize.X, MapCells.Y * TileSize.Y));
	const FVector MapLow(FMath::Min(CornerA.X, CornerB.X), 0.0, FMath::Min(CornerA.Z, CornerB.Z));
	const FVector MapHigh(FMath::Max(CornerA.X, CornerB.X), 0.0, FMath::Max(CornerA.Z, CornerB.Z));
	const FVector2D Half = GetGameViewHalfSize(World);
	auto Confine = [&](FVector Point)
	{
		const double LowX = MapLow.X + Half.X;
		const double HighX = MapHigh.X - Half.X;
		const double LowZ = MapLow.Z + Half.Y;
		const double HighZ = MapHigh.Z - Half.Y;
		Point.X = LowX <= HighX ? FMath::Clamp(Point.X, LowX, HighX) : (MapLow.X + MapHigh.X) * 0.5;
		Point.Z = LowZ <= HighZ ? FMath::Clamp(Point.Z, LowZ, HighZ) : (MapLow.Z + MapHigh.Z) * 0.5;
		return Point;
	};
	const FVector Camera = Confine(Painter->MapPixelsToWorld(GetViewCenterMapPixels()));
	FVector Start = Camera;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Start = Confine(It->GetActorLocation());
		break;
	}
	return Camera - Start;
}

TOptional<FBox2D> SOnyxMapCanvas::GetImageLayerRect(int32 LayerIndex) const
{
	const AOnyxParallaxBackground* Background = FindParallax();
	if (!Background || !Background->BackgroundLayers.IsValidIndex(LayerIndex) || !Painter || !Painter->FindMapActor())
	{
		return {};
	}
	const double ViewX = Painter->MapPixelsToWorld(GetViewCenterMapPixels()).X;
	const FBox2D Picture = OnyxCanvasBackdrop::OwnPicture(*Background, Background->BackgroundLayers[LayerIndex], GetParallaxCameraDelta(), ViewX);
	const FVector2D A = Painter->WorldToMapPixels(FVector(Picture.Min.X, 0.0, Picture.Max.Y));
	const FVector2D B = Painter->WorldToMapPixels(FVector(Picture.Max.X, 0.0, Picture.Min.Y));
	return FBox2D(A.ComponentMin(B), A.ComponentMax(B));
}

int32 SOnyxMapCanvas::DrawParallax(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2D& Area,
	const FVector2f& MapMin, const FVector2f& MapPixels) const
{
	const AOnyxParallaxBackground* Background = FindParallax();
	const double CmPerPixel = (Painter->MapPixelsToWorld(FVector2D(1.0, 0.0)) - Painter->MapPixelsToWorld(FVector2D::ZeroVector)).Size();
	if (!Background || CmPerPixel <= 0.0)
	{
		return LayerId;
	}
	// Inside the map only: the game's camera never looks past it.
	const FVector2f ClipMin = MapMin.ComponentMax(FVector2f::ZeroVector);
	const FVector2f ClipMax = (MapMin + MapPixels).ComponentMin(FVector2f(Area));
	if (ClipMax.X <= ClipMin.X || ClipMax.Y <= ClipMin.Y)
	{
		return LayerId;
	}
	const FVector Delta = GetParallaxCameraDelta();
	FLinearColor Mood = FLinearColor::White;
	for (TActorIterator<AOnyxLightingEnvironment> It(Background->GetWorld()); It; ++It)
	{
		Mood = It->GetBaseMood().BackgroundTint;
		break;
	}
	auto ToLocal = [&](double X, double Z) { return MapMin + FVector2f(Painter->WorldToMapPixels(FVector(X, 0.0, Z))) * Zoom; };

	// The farthest first (what is nearer covers it, as in the game).
	const TSet<int32> Hidden = HiddenImageLayers.Get();
	TArray<int32> Order;
	for (int32 Index = 0; Index < Background->BackgroundLayers.Num(); ++Index)
	{
		if (Background->BackgroundLayers[Index].bVisible && !Hidden.Contains(Index))
		{
			Order.Add(Index);
		}
	}
	const TArray<FOnyxParallaxLayer>& Layers = Background->BackgroundLayers;
	Order.StableSort([&Layers](int32 A, int32 B) { return Layers[A].Depth > Layers[B].Depth; });

	OutDrawElements.PushClip(FSlateClippingZone(FSlateRect(Geometry.LocalToAbsolute(FVector2D(ClipMin)), Geometry.LocalToAbsolute(FVector2D(ClipMax)))));
	int32 Draw = LayerId;
	for (const int32 Index : Order)
	{
		const FOnyxParallaxLayer& Layer = Layers[Index];
		const FVector2D At = OnyxCanvasBackdrop::Middle(*Background, Layer, Delta);
		const double Top = At.Y + Layer.Height * 0.5;
		const double PictureBottom = At.Y - Layer.Height * 0.5;
		const double Bottom = PictureBottom - Layer.ExtendBelow;
		FLinearColor Color(Layer.Tint.R * Mood.R, Layer.Tint.G * Mood.G, Layer.Tint.B * Mood.B, Layer.Tint.A);
		const double Width = OnyxCanvasBackdrop::PictureWidth(Layer);
		if (Width <= 0.0)
		{
			// A plain colour, or a mist drawn by its own material: a band of its tint (mist is patchy: about half of it).
			Color.A *= Layer.Material.IsNull() ? 1.f : 0.5f;
			const FVector2f A = ToLocal(At.X - Background->TotalWidth * 0.5, Top);
			const FVector2f B = ToLocal(At.X + Background->TotalWidth * 0.5, Bottom);
			DrawRect(OutDrawElements, Draw++, Geometry, A.ComponentMin(B), (B - A).GetAbs(), Color);
			continue;
		}
		UTexture2D* Texture = Layer.Texture;
		const float TextureHeight = Texture->GetSurfaceHeight();
		// Once where it is placed, or copies side by side across the strip (the last one cut where the strip ends).
		const double StripLeft = Layer.bRepeat ? At.X - Background->TotalWidth * 0.5 : At.X - Width * 0.5;
		const double StripRight = Layer.bRepeat ? At.X + Background->TotalWidth * 0.5 : At.X + Width * 0.5;
		const float CopyLocal = float(Width / CmPerPixel) * Zoom;
		if (CopyLocal < 0.5f)
		{
			continue;
		}
		const float StripLeftLocal = ToLocal(StripLeft, Top).X;
		const int32 Copies = FMath::Max(FMath::CeilToInt((StripRight - StripLeft) / Width), 1);
		const int32 First = FMath::Max(FMath::FloorToInt((ClipMin.X - StripLeftLocal) / CopyLocal), 0);
		const int32 Last = FMath::Min(FMath::FloorToInt((ClipMax.X - StripLeftLocal) / CopyLocal), Copies - 1);
		if (Last - First > 600)
		{
			continue;
		}
		const float TopLocal = ToLocal(At.X, Top).Y;
		const float PictureBottomLocal = ToLocal(At.X, PictureBottom).Y;
		const float BottomLocal = ToLocal(At.X, Bottom).Y;
		ParallaxBrush.SetResourceObject(Texture);
		for (int32 Copy = First; Copy <= Last; ++Copy)
		{
			const double CopyLeft = StripLeft + Copy * Width;
			const double Shown = FMath::Min(Width, StripRight - CopyLeft) / Width; // the part of the last copy inside the strip
			const float X = StripLeftLocal + Copy * CopyLocal;
			const float W = CopyLocal * float(Shown);
			ParallaxBrush.SetUVRegion(FBox2f(FVector2f(0.f, 0.f), FVector2f(float(Shown), 1.f)));
			FSlateDrawElement::MakeBox(OutDrawElements, Draw, Geometry.ToPaintGeometry(FVector2f(W, PictureBottomLocal - TopLocal), FSlateLayoutTransform(FVector2f(X, TopLocal))),
				&ParallaxBrush, ESlateDrawEffect::None, Color);
			if (BottomLocal > PictureBottomLocal + 0.5f && TextureHeight > 0.f)
			{
				// Below the picture its last row carries on down (Extend Below).
				ParallaxBrush.SetUVRegion(FBox2f(FVector2f(0.f, (TextureHeight - 1.f) / TextureHeight), FVector2f(float(Shown), 1.f)));
				FSlateDrawElement::MakeBox(OutDrawElements, Draw, Geometry.ToPaintGeometry(FVector2f(W, BottomLocal - PictureBottomLocal),
					FSlateLayoutTransform(FVector2f(X, PictureBottomLocal))), &ParallaxBrush, ESlateDrawEffect::None, Color);
			}
		}
		++Draw;
	}
	OutDrawElements.PopClip();
	return FMath::Max(Draw, LayerId + 1);
}

int32 SOnyxMapCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Area = AllottedGeometry.GetLocalSize();
	DrawRect(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, FVector2f(Area), BackgroundColor);

	const FIntPoint MapSize = Painter ? Painter->GetMapSize() : FIntPoint::ZeroValue;
	if (MapSize.X <= 0 || MapSize.Y <= 0)
	{
		return LayerId + 1;
	}
	LastArea = Area;
	FollowMapCorner();
	if (bFitRequested || (bKeepFitted && !Area.Equals(FittedArea, 1.0)))
	{
		FitTo(Area);
	}
	if (OnyxCanvasView::PendingZoom > 0.f)
	{
		const FIntPoint Size = Painter->GetMapSize();
		const FIntPoint Cell = Painter->IsValidCell(OnyxCanvasView::PendingCell) ? OnyxCanvasView::PendingCell : Size / 2;
		const FIntPoint CellPixels = Painter->GetTileSize();
		Zoom = FMath::Clamp(OnyxCanvasView::PendingZoom, MinZoom, MaxZoom);
		Pan = Area * 0.5 - FVector2D((Cell.X + 0.5) * CellPixels.X, (Cell.Y + 0.5) * CellPixels.Y) * Zoom;
		bKeepFitted = false;
		OnyxCanvasView::PendingZoom = 0.f;
	}

	const FIntPoint Tile = Painter->GetTileSize();
	const FVector2f CellSize(Tile.X * Zoom, Tile.Y * Zoom);
	const FVector2f MapMin = CellToLocal(FIntPoint::ZeroValue);
	const FVector2f MapPixels(MapSize.X * CellSize.X, MapSize.Y * CellSize.Y);

	// Map area with a soft drop shadow.
	DrawRect(OutDrawElements, LayerId + 1, AllottedGeometry, MapMin + FVector2f(6.f, 6.f), MapPixels, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	DrawRect(OutDrawElements, LayerId + 2, AllottedGeometry, MapMin, MapPixels, MapColor);

	// The image layers, the tiles over them.
	const int32 AfterBackdrop = DrawParallax(OutDrawElements, LayerId + 3, AllottedGeometry, Area, MapMin, MapPixels);
	const bool bImages = AfterBackdrop > LayerId + 3;
	if (bImages)
	{
		LayerId = AfterBackdrop - 2; // tiles and the rest draw above it
	}

	// Visible cell range only.
	const FIntPoint First = LocalToCell(FVector2D::ZeroVector).ComponentMax(FIntPoint::ZeroValue);
	const FIntPoint Last = LocalToCell(Area).ComponentMin(MapSize - FIntPoint(1, 1));

	// Tile layers, back (last) to front (0); the active one is fully opaque.
	const TSet<int32> Hidden = HiddenLayers.Get();
	int32 DrawLayer = LayerId + 3;
	for (int32 LayerIndex = Painter->GetLayerCount() - 1; LayerIndex >= 0; --LayerIndex, DrawLayer += 2)
	{
		if (Hidden.Contains(LayerIndex))
		{
			continue;
		}
		const bool bActive = LayerIndex == Painter->GetLayer();
		const FLinearColor Tint = bActive ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.72f);
		for (int32 Y = First.Y; Y <= Last.Y; ++Y)
		{
			for (int32 X = First.X; X <= Last.X; ++X)
			{
				const FPaperTileInfo Info = Painter->GetCell(FIntPoint(X, Y), LayerIndex);
				if (Info.IsValid())
				{
					const FVector2f Cell = CellToLocal(FIntPoint(X, Y));
					if (!bActive && bImages)
					{
						// A faded wall still hides the pictures behind it, as in the game: its shape in the map's colour first.
						DrawTile(OutDrawElements, DrawLayer, AllottedGeometry, Info, Cell, CellSize.X, FLinearColor(MapColor.R, MapColor.G, MapColor.B, 1.f));
					}
					DrawTile(OutDrawElements, DrawLayer + 1, AllottedGeometry, Info, Cell, CellSize.X, Tint);
				}
			}
		}
	}

	// Collision overlay: what characters will actually collide with.
	if (ShowCollision.Get())
	{
		for (int32 Y = First.Y; Y <= Last.Y; ++Y)
		{
			for (int32 X = First.X; X <= Last.X; ++X)
			{
				const EOnyxTileCollision Type = Painter->GetCellCollision(FIntPoint(X, Y));
				if (Type == EOnyxTileCollision::None)
				{
					continue;
				}
				const FLinearColor Color = FOnyxTileCollision::GetColor(Type);
				const FVector2f Min = CellToLocal(FIntPoint(X, Y));
				if (Type == EOnyxTileCollision::OneWay)
				{
					DrawRect(OutDrawElements, DrawLayer, AllottedGeometry, Min, FVector2f(CellSize.X, FMath::Max(CellSize.Y * 0.2f, 2.f)), Color.CopyWithNewOpacity(0.85f));
				}
				else if (Type == EOnyxTileCollision::SlopeUp || Type == EOnyxTileCollision::SlopeDown)
				{
					// The slope's line, the solid side shaded faintly.
					DrawRect(OutDrawElements, DrawLayer, AllottedGeometry, Min, CellSize, Color.CopyWithNewOpacity(0.14f));
					const bool bUp = Type == EOnyxTileCollision::SlopeUp;
					TArray<FVector2f> Slope = { Min + FVector2f(0.f, bUp ? CellSize.Y : 0.f), Min + FVector2f(CellSize.X, bUp ? 0.f : CellSize.Y) };
					FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(), Slope, ESlateDrawEffect::None, Color.CopyWithNewOpacity(0.9f), true, 2.f);
				}
				else
				{
					DrawRect(OutDrawElements, DrawLayer, AllottedGeometry, Min, CellSize, Color.CopyWithNewOpacity(0.32f));
					DrawOutline(OutDrawElements, DrawLayer, AllottedGeometry, Min + FVector2f(0.5f), CellSize - FVector2f(1.f), Color.CopyWithNewOpacity(0.7f), 1.f);
				}
			}
		}
		++DrawLayer;
	}

	// Grid.
	if (ShowGrid.Get() && CellSize.X >= 6.f)
	{
		TArray<FVector2f> Line;
		Line.SetNum(2);
		for (int32 X = First.X; X <= Last.X + 1; ++X)
		{
			Line[0] = FVector2f(MapMin.X + X * CellSize.X, MapMin.Y);
			Line[1] = FVector2f(Line[0].X, MapMin.Y + MapPixels.Y);
			FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, CanvasGridColor, false, 1.f);
		}
		for (int32 Y = First.Y; Y <= Last.Y + 1; ++Y)
		{
			Line[0] = FVector2f(MapMin.X, MapMin.Y + Y * CellSize.Y);
			Line[1] = FVector2f(MapMin.X + MapPixels.X, Line[0].Y);
			FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, CanvasGridColor, false, 1.f);
		}
	}
	DrawOutline(OutDrawElements, DrawLayer, AllottedGeometry, MapMin, MapPixels, FLinearColor(1.f, 1.f, 1.f, 0.18f));
	++DrawLayer;

	// Objects: decor first, then entities on top.
	const AActor* Selected = SelectedActor.Get().Get();
	const TSet<FName> HiddenObjects = HiddenObjectLayers.Get();
	const FOnyxBrushLibrary& Library = FOnyxBrushLibrary::Get();
	TArray<FOnyxPlacedObject> Objects;
	Painter->ForEachObject([&Objects](const FOnyxPlacedObject& Object) { Objects.Add(Object); });
	// Draw back to front, like the level: background scenery, gameplay objects, foreground decor; in each layer, areas
	// (water) under the objects, so what is in the water can still be seen and picked.
	auto Order = [](const FOnyxPlacedObject& Object)
	{
		const int32 Layer = Object.Layer == FOnyxBrushLibrary::BackgroundLayer ? 0 : (Object.Layer == FOnyxBrushLibrary::DecorLayer ? 2 : 1);
		return Layer * 2 + (Object.Area.IsSet() ? 0 : 1);
	};
	Objects.StableSort([&Order](const FOnyxPlacedObject& A, const FOnyxPlacedObject& B) { return Order(A) < Order(B); });
	const int32 AreaLayer = DrawLayer;
	const int32 ObjectLayer = DrawLayer + 3;
	for (const FOnyxPlacedObject& Object : Objects)
	{
		const TSharedPtr<FOnyxPainterBrush> Brush = Library.FindFor(Object.Actor);
		if (!Brush.IsValid() || HiddenObjects.Contains(Object.Layer))
		{
			continue;
		}
		FVector2f AreaMin, AreaMax;
		if (Object.Area.IsSet() && GetAreaRect(*Brush, Object.Actor, MapMin, AreaMin, AreaMax))
		{
			DrawArea(OutDrawElements, AreaLayer, AllottedGeometry, *Brush, Object.Actor, AreaMin, AreaMax, 1.f, Object.Actor == Selected);
			continue;
		}
		const FVector2f Min = CellToLocal(Object.Cell);
		// The actor as it looks in the level (its sprites and flipbooks, animated); a marker when it has no art.
		FVector2f BoundsMin = Min;
		FVector2f BoundsMax = Min + CellSize;
		if (!DrawActorSprites(OutDrawElements, ObjectLayer, AllottedGeometry, Object.Actor, MapMin, BoundsMin, BoundsMax))
		{
			DrawBrush(OutDrawElements, ObjectLayer, AllottedGeometry, *Brush, Min, CellSize, 1.f);
		}
		if (Object.Actor == Selected)
		{
			DrawOutline(OutDrawElements, ObjectLayer + 3, AllottedGeometry, BoundsMin - FVector2f(2.f), BoundsMax - BoundsMin + FVector2f(4.f), OnyxTools::Accent(), 2.5f);
		}
	}
	DrawLayer += 7;

	// Rectangle tool area (an area brush shows the object it will make).
	const TSharedPtr<FOnyxPainterBrush> Preview = PreviewBrush.Get();
	const bool bAreaPreview = Preview.IsValid() && Preview->IsArea();
	if (const TOptional<FIntRect> Rect = PreviewRect.Get())
	{
		const FVector2f Min = CellToLocal(Rect->Min);
		const FVector2f Size(float(Rect->Width() + 1) * CellSize.X, float(Rect->Height() + 1) * CellSize.Y);
		if (bAreaPreview)
		{
			DrawArea(OutDrawElements, DrawLayer, AllottedGeometry, *Preview, nullptr, Min, Min + Size, 0.75f, false);
		}
		DrawRect(OutDrawElements, DrawLayer, AllottedGeometry, Min, Size, OnyxTools::Accent().CopyWithNewOpacity(bAreaPreview ? 0.06f : 0.18f));
		DrawOutline(OutDrawElements, DrawLayer + 3, AllottedGeometry, Min, Size, OnyxTools::Accent(), 2.f);
	}

	// Brush preview under the cursor.
	if (bAreaPreview && !PreviewRect.Get().IsSet() && bHasHover && Painter->IsValidCell(HoverCell) && !bPanning)
	{
		// Where a click would put it: its default size, from the cell under the cursor.
		const FVector2f Min = CellToLocal(HoverCell);
		const FVector2f Size(Preview->DefaultAreaCells.X * CellSize.X, Preview->DefaultAreaCells.Y * CellSize.Y);
		DrawArea(OutDrawElements, DrawLayer, AllottedGeometry, *Preview, nullptr, Min, Min + Size, 0.5f, false);
		DrawOutline(OutDrawElements, DrawLayer + 3, AllottedGeometry, Min, Size, FLinearColor(1.f, 1.f, 1.f, 0.6f), 1.f);
	}
	else if (bHasHover && Painter->IsValidCell(HoverCell) && !bPanning)
	{
		const FVector2f Min = CellToLocal(HoverCell);
		const FOnyxStamp Stamp = PreviewStamp.Get();
		FVector2f OutlineSize = CellSize;
		if (Stamp.IsValid())
		{
			const FIntPoint PaintSize = Stamp.GetPaintSize();
			for (int32 Y = 0; Y < PaintSize.Y; ++Y)
			{
				for (int32 X = 0; X < PaintSize.X; ++X)
				{
					const FIntPoint Cell = HoverCell + FIntPoint(X, Y);
					if (Painter->IsValidCell(Cell))
					{
						DrawTile(OutDrawElements, DrawLayer + 2, AllottedGeometry, Stamp.GetTile(X, Y, Cell), CellToLocal(Cell), CellSize.X, FLinearColor(1.f, 1.f, 1.f, 0.65f));
					}
				}
			}
			OutlineSize = FVector2f(CellSize.X * PaintSize.X, CellSize.Y * PaintSize.Y);
		}
		else if (const TSharedPtr<FOnyxPainterBrush> Brush = PreviewBrush.Get())
		{
			DrawBrush(OutDrawElements, DrawLayer + 2, AllottedGeometry, *Brush, Min, CellSize, 0.6f);
		}
		DrawOutline(OutDrawElements, DrawLayer + 3, AllottedGeometry, Min, OutlineSize, FLinearColor(1.f, 1.f, 1.f, 0.85f), 1.5f);
	}
	DrawMapEdges(OutDrawElements, DrawLayer + 4, AllottedGeometry, MapMin, MapMin + MapPixels, CellSize);
	if (const TOptional<FBox2D> Picture = GetImageLayerRect(SelectedImageLayer.Get()))
	{
		// The image layer being edited: its picture outlined, a handle on each corner to resize it.
		const FVector2f Min = MapMin + FVector2f(Picture->Min) * Zoom;
		const FVector2f Max = MapMin + FVector2f(Picture->Max) * Zoom;
		DrawOutline(OutDrawElements, DrawLayer + 4, AllottedGeometry, Min, Max - Min, OnyxTools::Accent(), 2.f);
		for (const FVector2f& Corner : { Min, FVector2f(Max.X, Min.Y), Max, FVector2f(Min.X, Max.Y) })
		{
			DrawRect(OutDrawElements, DrawLayer + 5, AllottedGeometry, Corner - FVector2f(5.f), FVector2f(10.f), OnyxTools::Accent());
			DrawRect(OutDrawElements, DrawLayer + 6, AllottedGeometry, Corner - FVector2f(3.f), FVector2f(6.f), FLinearColor::White);
		}
	}
	return DrawLayer + 7;
}

void SOnyxMapCanvas::FollowMapCorner() const
{
	UPaperTileMap* Map = Painter->GetTileMap();
	const TOptional<FVector> Corner = Painter->GetMapCornerInLevel();
	if (Map == CornerMap.Get() && Corner.IsSet() && CornerInLevel.IsSet() && !Corner->Equals(*CornerInLevel, 0.01))
	{
		// Where the old corner is on the map now: move the view by as much, so nothing on screen jumps.
		Pan -= Painter->WorldToMapPixels(*CornerInLevel) * Zoom;
	}
	CornerMap = Map;
	CornerInLevel = Corner;
}

int32 SOnyxMapCanvas::GetMapEdgesAt(const FVector2D& Local) const
{
	const FIntPoint Size = Painter ? Painter->GetMapSize() : FIntPoint::ZeroValue;
	if (!OnResizeMap.IsBound() || Size.X <= 0 || Size.Y <= 0)
	{
		return 0;
	}
	const FVector2f Min = CellToLocal(FIntPoint::ZeroValue);
	const FVector2f Max = CellToLocal(Size);
	if (Local.X < Min.X - MapEdgeReach || Local.X > Max.X + MapEdgeReach || Local.Y < Min.Y - MapEdgeReach || Local.Y > Max.Y + MapEdgeReach)
	{
		return 0;
	}
	int32 Edges = 0;
	Edges |= Local.X < Min.X ? MapEdgeLeft : (Local.X > Max.X ? MapEdgeRight : 0);
	Edges |= Local.Y < Min.Y ? MapEdgeTop : (Local.Y > Max.Y ? MapEdgeBottom : 0);
	return Edges;
}

FOnyxMapEdges SOnyxMapCanvas::GetDraggedEdges(const FVector2D& Local) const
{
	const FIntPoint Size = Painter->GetMapSize();
	const FIntPoint Tile = Painter->GetTileSize();
	const FVector2D Moved = (Local - MapGrabLocal) / FMath::Max(Zoom, 0.01f);
	const int32 Columns = FMath::RoundToInt(Moved.X / FMath::Max(Tile.X, 1));
	const int32 Rows = FMath::RoundToInt(Moved.Y / FMath::Max(Tile.Y, 1));
	// Out adds cells, in removes them; at least one cell stays.
	const int32 MaxSize = FOnyxMapPainter::MaxMapSize;
	FOnyxMapEdges Edges;
	Edges.Left = (GrabbedMapEdges & MapEdgeLeft) != 0 ? FMath::Clamp(-Columns, 1 - Size.X, MaxSize - Size.X) : 0;
	Edges.Right = (GrabbedMapEdges & MapEdgeRight) != 0 ? FMath::Clamp(Columns, 1 - Size.X, MaxSize - Size.X) : 0;
	Edges.Top = (GrabbedMapEdges & MapEdgeTop) != 0 ? FMath::Clamp(-Rows, 1 - Size.Y, MaxSize - Size.Y) : 0;
	Edges.Bottom = (GrabbedMapEdges & MapEdgeBottom) != 0 ? FMath::Clamp(Rows, 1 - Size.Y, MaxSize - Size.Y) : 0;
	return Edges;
}

void SOnyxMapCanvas::DrawMapEdges(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2f& MapMin,
	const FVector2f& MapMax, const FVector2f& CellSize) const
{
	if (!OnResizeMap.IsBound())
	{
		return;
	}
	const FLinearColor Accent = OnyxTools::Accent();
	const int32 Lit = GrabbedMapEdges != 0 ? GrabbedMapEdges : (bHasHover && !bStroking && !bPanning ? GetMapEdgesAt(HoverLocal) : 0);

	// A grip in the middle of each side, just outside the border; the side under the mouse lights up.
	const FVector2f Center = (MapMin + MapMax) * 0.5f;
	const float Grip = FMath::Clamp(FMath::Min(MapMax.X - MapMin.X, MapMax.Y - MapMin.Y) * 0.3f, 14.f, 44.f);
	struct FGrip
	{
		int32 Edge;
		FVector2f Min;
		FVector2f Size;
		FVector2f From;
		FVector2f To;
	};
	const FGrip Grips[] = {
		{ MapEdgeLeft, FVector2f(MapMin.X - 8.f, Center.Y - Grip * 0.5f), FVector2f(4.f, Grip), MapMin, FVector2f(MapMin.X, MapMax.Y) },
		{ MapEdgeRight, FVector2f(MapMax.X + 4.f, Center.Y - Grip * 0.5f), FVector2f(4.f, Grip), FVector2f(MapMax.X, MapMin.Y), MapMax },
		{ MapEdgeTop, FVector2f(Center.X - Grip * 0.5f, MapMin.Y - 8.f), FVector2f(Grip, 4.f), MapMin, FVector2f(MapMax.X, MapMin.Y) },
		{ MapEdgeBottom, FVector2f(Center.X - Grip * 0.5f, MapMax.Y + 4.f), FVector2f(Grip, 4.f), FVector2f(MapMin.X, MapMax.Y), MapMax },
	};
	for (const FGrip& Each : Grips)
	{
		const bool bLit = (Lit & Each.Edge) != 0;
		DrawRect(OutDrawElements, LayerId, Geometry, Each.Min, Each.Size, bLit ? Accent : FLinearColor(1.f, 1.f, 1.f, 0.28f));
		if (bLit && GrabbedMapEdges == 0)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), TArray<FVector2f>{ Each.From, Each.To }, ESlateDrawEffect::None, Accent, true, 2.f);
		}
	}
	if (GrabbedMapEdges == 0)
	{
		if (Lit != 0)
		{
			DrawCanvasLabel(OutDrawElements, LayerId + 2, Geometry, TEXT("Drag out to add cells, in to remove them"), FVector2f(HoverLocal) + FVector2f(16.f, 18.f));
		}
		return;
	}

	// Dragging: cells to add (accent, with their grid) and cells to remove (red), then the new border.
	const FOnyxMapEdges& Edges = DraggedEdges;
	const FVector2f NewMin = MapMin - FVector2f(Edges.Left * CellSize.X, Edges.Top * CellSize.Y);
	const FVector2f NewMax = MapMax + FVector2f(Edges.Right * CellSize.X, Edges.Bottom * CellSize.Y);
	const FLinearColor GridColor(1.f, 1.f, 1.f, 0.08f);
	auto Strip = [&](float X0, float Y0, float X1, float Y1, bool bAdd)
	{
		if (X1 - X0 < 0.5f || Y1 - Y0 < 0.5f)
		{
			return;
		}
		DrawRect(OutDrawElements, LayerId, Geometry, FVector2f(X0, Y0), FVector2f(X1 - X0, Y1 - Y0), bAdd ? Accent.CopyWithNewOpacity(0.16f) : MapEdgeRemoveColor);
		if (!bAdd || CellSize.X < 6.f || CellSize.Y < 6.f)
		{
			return;
		}
		TArray<FVector2f> Line;
		Line.SetNum(2);
		for (float X = X0 + CellSize.X; X < X1 - 0.5f; X += CellSize.X)
		{
			Line[0] = FVector2f(X, Y0);
			Line[1] = FVector2f(X, Y1);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, false, 1.f);
		}
		for (float Y = Y0 + CellSize.Y; Y < Y1 - 0.5f; Y += CellSize.Y)
		{
			Line[0] = FVector2f(X0, Y);
			Line[1] = FVector2f(X1, Y);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, false, 1.f);
		}
	};
	// Top and bottom strips span the whole width; the side strips only the rows in between, so corners aren't drawn twice.
	const float SideTop = FMath::Max(NewMin.Y, MapMin.Y);
	const float SideBottom = FMath::Min(NewMax.Y, MapMax.Y);
	Edges.Top > 0 ? Strip(NewMin.X, NewMin.Y, NewMax.X, MapMin.Y, true) : Strip(MapMin.X, MapMin.Y, MapMax.X, NewMin.Y, false);
	Edges.Bottom > 0 ? Strip(NewMin.X, MapMax.Y, NewMax.X, NewMax.Y, true) : Strip(MapMin.X, NewMax.Y, MapMax.X, MapMax.Y, false);
	Edges.Left > 0 ? Strip(NewMin.X, SideTop, MapMin.X, SideBottom, true) : Strip(MapMin.X, SideTop, NewMin.X, SideBottom, false);
	Edges.Right > 0 ? Strip(MapMax.X, SideTop, NewMax.X, SideBottom, true) : Strip(NewMax.X, SideTop, MapMax.X, SideBottom, false);
	DrawOutline(OutDrawElements, LayerId + 1, Geometry, NewMin, NewMax - NewMin, Accent, 2.f);

	TArray<FString> Changes;
	auto Describe = [&Changes](int32 Count, const TCHAR* Unit, const TCHAR* Where)
	{
		if (Count != 0)
		{
			Changes.Add(FString::Printf(TEXT("%s%d %s%s %s"), Count > 0 ? TEXT("+") : TEXT("-"), FMath::Abs(Count), Unit, FMath::Abs(Count) == 1 ? TEXT("") : TEXT("s"), Where));
		}
	};
	Describe(Edges.Left, TEXT("column"), TEXT("on the left"));
	Describe(Edges.Right, TEXT("column"), TEXT("on the right"));
	Describe(Edges.Top, TEXT("row"), TEXT("at the top"));
	Describe(Edges.Bottom, TEXT("row"), TEXT("at the bottom"));
	const FIntPoint NewSize = Edges.Apply(Painter->GetMapSize());
	const FString Label = Changes.IsEmpty() ? FString::Printf(TEXT("%d x %d  (drag out to add, in to remove)"), NewSize.X, NewSize.Y)
		: FString::Printf(TEXT("%s  =  %d x %d"), *FString::Join(Changes, TEXT(", ")), NewSize.X, NewSize.Y);
	DrawCanvasLabel(OutDrawElements, LayerId + 2, Geometry, Label, FVector2f(HoverLocal) + FVector2f(16.f, 18.f));
}

FReply SOnyxMapCanvas::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FKey Button = Event.GetEffectingButton();
	if (Button == EKeys::MiddleMouseButton || (Button == EKeys::LeftMouseButton && bSpaceHeld))
	{
		bPanning = true;
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
	}
	if (Button == EKeys::LeftMouseButton && Painter && Painter->GetMapSize().X > 0)
	{
		const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		// A tool showing its own cursor here (the handles of the selected water) keeps the click.
		const bool bToolWantsIt = OnQueryCursor.IsBound() && OnQueryCursor.Execute((Local - Pan) / FMath::Max(Zoom, 0.01f)).IsSet();
		if (const int32 Edges = bToolWantsIt ? 0 : GetMapEdgesAt(Local))
		{
			GrabbedMapEdges = Edges;
			MapGrabLocal = Local;
			DraggedEdges = FOnyxMapEdges();
			HoverLocal = Local;
			HoverCell = LocalToCell(Local);
			return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
		}
	}
	if ((Button == EKeys::LeftMouseButton || Button == EKeys::RightMouseButton) && Painter && Painter->GetMapSize().X > 0)
	{
		bStroking = true;
		bStrokeSecondary = Button == EKeys::RightMouseButton;
		HoverLocal = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		HoverCell = LocalToCell(HoverLocal);
		OnStroke.ExecuteIfBound(EOnyxStrokePhase::Begin, HoverCell, bStrokeSecondary);
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SOnyxMapCanvas::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (GrabbedMapEdges != 0)
	{
		const FOnyxMapEdges Edges = DraggedEdges;
		GrabbedMapEdges = 0;
		DraggedEdges = FOnyxMapEdges();
		if (!Edges.IsZero())
		{
			OnResizeMap.ExecuteIfBound(Edges);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (bPanning)
	{
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (bStroking)
	{
		bStroking = false;
		OnStroke.ExecuteIfBound(EOnyxStrokePhase::End, HoverCell, bStrokeSecondary);
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SOnyxMapCanvas::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bPanning)
	{
		Pan += Event.GetCursorDelta() / FMath::Max(Geometry.Scale, 0.01f);
		bKeepFitted = false;
		return FReply::Handled();
	}
	if (!Painter || Painter->GetMapSize().X <= 0)
	{
		return FReply::Unhandled();
	}
	HoverLocal = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	bHasHover = true;
	if (GrabbedMapEdges != 0)
	{
		DraggedEdges = GetDraggedEdges(HoverLocal);
		return FReply::Handled();
	}
	const FIntPoint Cell = LocalToCell(HoverLocal);
	if (Cell != HoverCell)
	{
		HoverCell = Cell;
		if (bStroking)
		{
			OnStroke.ExecuteIfBound(EOnyxStrokePhase::Move, HoverCell, bStrokeSecondary);
		}
	}
	return FReply::Handled();
}

FReply SOnyxMapCanvas::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const float OldZoom = Zoom;
	Zoom = FMath::Clamp(Zoom * (Event.GetWheelDelta() > 0.f ? 1.2f : 1.f / 1.2f), MinZoom, MaxZoom);
	bKeepFitted = false;
	// Keep the point under the cursor fixed.
	Pan = Local - (Local - Pan) * (Zoom / OldZoom);
	return FReply::Handled();
}

void SOnyxMapCanvas::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	if (!bStroking && GrabbedMapEdges == 0)
	{
		bHasHover = false;
	}
}

void SOnyxMapCanvas::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
	GrabbedMapEdges = 0; // a border drag that loses the mouse changes nothing
	DraggedEdges = FOnyxMapEdges();
}

FReply SOnyxMapCanvas::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (GrabbedMapEdges != 0 && Event.GetKey() == EKeys::Escape)
	{
		GrabbedMapEdges = 0; // cancel the border drag
		DraggedEdges = FOnyxMapEdges();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (Event.GetKey() == EKeys::SpaceBar)
	{
		bSpaceHeld = true;
		return FReply::Handled();
	}
	return OnKey.IsBound() ? OnKey.Execute(Event) : FReply::Unhandled();
}

FReply SOnyxMapCanvas::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::SpaceBar)
	{
		bSpaceHeld = false;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FCursorReply SOnyxMapCanvas::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	if (bPanning)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}
	if (GrabbedMapEdges != 0)
	{
		return FCursorReply::Cursor(MapEdgeCursor(GrabbedMapEdges));
	}
	if (bSpaceHeld)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
	if (OnQueryCursor.IsBound() && bHasHover)
	{
		if (const TOptional<EMouseCursor::Type> Cursor = OnQueryCursor.Execute(GetHoverMapPixels()))
		{
			return FCursorReply::Cursor(*Cursor);
		}
	}
	if (const int32 Edges = bHasHover && !bStroking ? GetMapEdgesAt(HoverLocal) : 0)
	{
		return FCursorReply::Cursor(MapEdgeCursor(Edges));
	}
	return FCursorReply::Cursor(EMouseCursor::Crosshairs);
}

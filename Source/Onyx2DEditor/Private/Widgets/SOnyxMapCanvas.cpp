// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxMapCanvas.h"
#include "Level/OnyxBrushLibrary.h"
#include "Level/OnyxMapPainter.h"
#include "Engine/Texture2D.h"
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
}

void SOnyxMapCanvas::Construct(const FArguments& InArgs)
{
	Painter = InArgs._Painter;
	ShowGrid = InArgs._ShowGrid;
	ShowCollision = InArgs._ShowCollision;
	HiddenObjectLayers = InArgs._HiddenObjectLayers;
	HiddenLayers = InArgs._HiddenLayers;
	PreviewStamp = InArgs._PreviewStamp;
	PreviewBrush = InArgs._PreviewBrush;
	PreviewRect = InArgs._PreviewRect;
	SelectedActor = InArgs._SelectedActor;
	OnStroke = InArgs._OnStroke;
	OnKey = InArgs._OnKey;
	OnQueryCursor = InArgs._OnQueryCursor;
	TileBrush.DrawAs = ESlateBrushDrawType::Image;
	SpriteBrush.DrawAs = ESlateBrushDrawType::Image;
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
	const FIntPoint TextureSize = Texture->GetImportedSize();
	const FIntPoint TileSize = Tile.TileSet->GetTileSize();
	if (TextureSize.X <= 0 || TextureSize.Y <= 0)
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
		const FIntPoint TextureSize = Texture->GetImportedSize();
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

	// Body: light under the top edge, deep at the bottom.
	FSlateDrawElement::MakeGradient(OutDrawElements, LayerId, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Min)),
		{ FSlateGradientStop(FVector2f::ZeroVector, Fade(Look.Top, 0.45f)), FSlateGradientStop(FVector2f(0.f, Size.Y), Fade(Look.Bottom, 0.7f)) }, Orient_Horizontal);

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
	if (bFitRequested)
	{
		FitTo(Area);
	}

	const FIntPoint Tile = Painter->GetTileSize();
	const FVector2f CellSize(Tile.X * Zoom, Tile.Y * Zoom);
	const FVector2f MapMin = CellToLocal(FIntPoint::ZeroValue);
	const FVector2f MapPixels(MapSize.X * CellSize.X, MapSize.Y * CellSize.Y);

	// Map area with a soft drop shadow.
	DrawRect(OutDrawElements, LayerId + 1, AllottedGeometry, MapMin + FVector2f(6.f, 6.f), MapPixels, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	DrawRect(OutDrawElements, LayerId + 2, AllottedGeometry, MapMin, MapPixels, MapColor);

	// Visible cell range only.
	const FIntPoint First = LocalToCell(FVector2D::ZeroVector).ComponentMax(FIntPoint::ZeroValue);
	const FIntPoint Last = LocalToCell(Area).ComponentMin(MapSize - FIntPoint(1, 1));

	// Tile layers, back (last) to front (0); the active one is fully opaque.
	const TSet<int32> Hidden = HiddenLayers.Get();
	int32 DrawLayer = LayerId + 3;
	for (int32 LayerIndex = Painter->GetLayerCount() - 1; LayerIndex >= 0; --LayerIndex, ++DrawLayer)
	{
		if (Hidden.Contains(LayerIndex))
		{
			continue;
		}
		const FLinearColor Tint = LayerIndex == Painter->GetLayer() ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.72f);
		for (int32 Y = First.Y; Y <= Last.Y; ++Y)
		{
			for (int32 X = First.X; X <= Last.X; ++X)
			{
				const FPaperTileInfo Info = Painter->GetCell(FIntPoint(X, Y), LayerIndex);
				if (Info.IsValid())
				{
					DrawTile(OutDrawElements, DrawLayer, AllottedGeometry, Info, CellToLocal(FIntPoint(X, Y)), CellSize.X, Tint);
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
	return DrawLayer + 3;
}

FReply SOnyxMapCanvas::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FKey Button = Event.GetEffectingButton();
	if (Button == EKeys::MiddleMouseButton || (Button == EKeys::LeftMouseButton && bSpaceHeld))
	{
		bPanning = true;
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
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
		return FReply::Handled();
	}
	if (!Painter || Painter->GetMapSize().X <= 0)
	{
		return FReply::Unhandled();
	}
	HoverLocal = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const FIntPoint Cell = LocalToCell(HoverLocal);
	bHasHover = true;
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
	// Keep the point under the cursor fixed.
	Pan = Local - (Local - Pan) * (Zoom / OldZoom);
	return FReply::Handled();
}

void SOnyxMapCanvas::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	if (!bStroking)
	{
		bHasHover = false;
	}
}

FReply SOnyxMapCanvas::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
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
	return FCursorReply::Cursor(EMouseCursor::Crosshairs);
}

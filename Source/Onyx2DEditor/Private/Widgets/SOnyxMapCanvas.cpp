// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxMapCanvas.h"
#include "Level/OnyxBrushLibrary.h"
#include "Level/OnyxMapPainter.h"
#include "Engine/Texture2D.h"
#include "PaperTileSet.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
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
	if (Library.GetPreviewSprite(Brush, Texture, Rect))
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
	// Draw back to front, like the level: background scenery, gameplay objects, foreground decor.
	auto Order = [](FName Layer) { return Layer == FOnyxBrushLibrary::BackgroundLayer ? 0 : (Layer == FOnyxBrushLibrary::DecorLayer ? 2 : 1); };
	Objects.StableSort([&Order](const FOnyxPlacedObject& A, const FOnyxPlacedObject& B) { return Order(A.Layer) < Order(B.Layer); });
	for (const FOnyxPlacedObject& Object : Objects)
	{
		const TSharedPtr<FOnyxPainterBrush> Brush = Library.FindFor(Object.Actor);
		if (!Brush.IsValid() || HiddenObjects.Contains(Object.Layer))
		{
			continue;
		}
		const FVector2f Min = CellToLocal(Object.Cell);
		DrawBrush(OutDrawElements, DrawLayer, AllottedGeometry, *Brush, Min, CellSize, 1.f);
		if (Object.Actor == Selected)
		{
			DrawOutline(OutDrawElements, DrawLayer + 3, AllottedGeometry, Min - FVector2f(2.f), CellSize + FVector2f(4.f), OnyxTools::Accent(), 2.5f);
		}
	}
	DrawLayer += 4;

	// Rectangle tool area.
	if (const TOptional<FIntRect> Rect = PreviewRect.Get())
	{
		const FVector2f Min = CellToLocal(Rect->Min);
		const FVector2f Size(float(Rect->Width() + 1) * CellSize.X, float(Rect->Height() + 1) * CellSize.Y);
		DrawRect(OutDrawElements, DrawLayer, AllottedGeometry, Min, Size, OnyxTools::Accent().CopyWithNewOpacity(0.18f));
		DrawOutline(OutDrawElements, DrawLayer + 1, AllottedGeometry, Min, Size, OnyxTools::Accent(), 2.f);
	}

	// Brush preview under the cursor.
	if (bHasHover && Painter->IsValidCell(HoverCell) && !bPanning)
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
		HoverCell = LocalToCell(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
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
	const FIntPoint Cell = LocalToCell(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
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
	return FCursorReply::Cursor(bSpaceHeld ? EMouseCursor::GrabHand : EMouseCursor::Crosshairs);
}

// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxTilePalette.h"
#include "Engine/Texture2D.h"
#include "PaperTileSet.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "World/OnyxTileCollision.h"

namespace
{
	constexpr float Padding = 8.f;
	/** Between the tiles' slots: each tile reads as its own, never bleeding into the next. */
	constexpr float Gap = 2.f;
	const FIntPoint NoTile(INDEX_NONE, INDEX_NONE);

	void Outline(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Size, const FLinearColor& Color, float Thickness)
	{
		const TArray<FVector2f> Points = { Min, Min + FVector2f(Size.X, 0.f), Min + Size, Min + FVector2f(0.f, Size.Y), Min };
		FSlateDrawElement::MakeLines(Out, LayerId, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	FIntPoint SheetSize(const UPaperTileSet* Set)
	{
		const UTexture2D* Texture = Set ? Set->GetTileSheetTexture() : nullptr;
		return Texture ? Texture->GetImportedSize() : FIntPoint::ZeroValue;
	}

	/** The texture as the GPU has it (a power-of-two padded sheet is bigger than the picture): what UVs are measured in. */
	FVector2f SurfaceSize(const UPaperTileSet* Set)
	{
		const UTexture2D* Texture = Set ? Set->GetTileSheetTexture() : nullptr;
		if (!Texture)
		{
			return FVector2f::ZeroVector;
		}
		const FVector2f Surface(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
		return Surface.X > 0.f && Surface.Y > 0.f ? Surface : FVector2f(SheetSize(Set));
	}

	FIntPoint TileCount(const UPaperTileSet* Set)
	{
		return Set ? FIntPoint(Set->GetTileCountX(), Set->GetTileCountY()) : FIntPoint::ZeroValue;
	}
}

void SOnyxTilePalette::Construct(const FArguments& InArgs)
{
	TileSet = InArgs._TileSet;
	Selection = InArgs._Selection;
	Zoom = InArgs._Zoom;
	ShowCollision = InArgs._ShowCollision;
	OnTilesSelected = InArgs._OnTilesSelected;
	SheetBrush.DrawAs = ESlateBrushDrawType::Image;
}

float SOnyxTilePalette::GetScale(float Width) const
{
	const UPaperTileSet* Set = TileSet.Get();
	const FIntPoint Count = TileCount(Set);
	if (Count.X <= 0 || !Set || Set->GetTileSize().X <= 0)
	{
		return 1.f;
	}
	const float Requested = Zoom.Get();
	if (Requested > 0.f)
	{
		return Requested;
	}
	// Fit the columns to the width; whole screen pixels per art pixel when that loses little space, so pixel art stays crisp.
	const float Fit = (Width - Padding * 2.f - Gap * (Count.X - 1)) / float(Count.X * Set->GetTileSize().X);
	return Fit >= 2.f ? FMath::FloorToFloat(Fit) : FMath::Max(Fit, 0.25f);
}

FVector2D SOnyxTilePalette::ComputeDesiredSize(float) const
{
	const FIntPoint Count = TileCount(TileSet.Get());
	if (Count.X <= 0 || Count.Y <= 0)
	{
		return FVector2D(200.f, 120.f);
	}
	const FVector2f Pitch = TilePitch(GetScale(LastWidth));
	const FVector2f Content(Pitch.X * Count.X - Gap, Pitch.Y * Count.Y - Gap);
	// Fit mode follows the panel width; zoomed in, the tiles can grow and scroll.
	return FVector2D(Zoom.Get() > 0.f ? Content.X + Padding * 2.f : 120.f, Content.Y + Padding * 2.f);
}

FVector2f SOnyxTilePalette::TilePitch(float Scale) const
{
	const UPaperTileSet* Set = TileSet.Get();
	return Set ? FVector2f(Set->GetTileSize()) * Scale + FVector2f(Gap) : FVector2f(Gap);
}

FVector2f SOnyxTilePalette::TileTopLeft(const FIntPoint& Tile, float Scale) const
{
	// Laid out by tile, not by where the tile sits in the sheet: the extruded edges of a padded sheet never show.
	const FVector2f Pitch = TilePitch(Scale);
	return FVector2f(Padding + Tile.X * Pitch.X, Padding + Tile.Y * Pitch.Y);
}

FIntPoint SOnyxTilePalette::TileAt(const FGeometry& Geometry, const FVector2D& ScreenPosition) const
{
	const UPaperTileSet* Set = TileSet.Get();
	if (!Set || SheetSize(Set).X <= 0)
	{
		return NoTile;
	}
	const FVector2f Local = FVector2f(Geometry.AbsoluteToLocal(ScreenPosition)) - FVector2f(Padding);
	const FVector2f Pitch = TilePitch(GetScale(Geometry.GetLocalSize().X));
	const FIntPoint Tile(FMath::FloorToInt(Local.X / Pitch.X), FMath::FloorToInt(Local.Y / Pitch.Y));
	const FIntPoint Count = TileCount(Set);
	if (Local.X < 0.f || Local.Y < 0.f || Tile.X >= Count.X || Tile.Y >= Count.Y)
	{
		return NoTile;
	}
	return Tile; // the gap after a tile counts as that tile: no dead lines when dragging across
}

int32 SOnyxTilePalette::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Area = AllottedGeometry.GetLocalSize();
	LastWidth = Area.X;
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.018f, 0.02f, 0.026f));

	UPaperTileSet* Set = TileSet.Get();
	const FIntPoint TextureSize = SheetSize(Set);
	if (TextureSize.X <= 0)
	{
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Area.X, 20.f), FSlateLayoutTransform(FVector2f(10.f, 10.f))),
			INVTEXT("Pick a tile set above"), OnyxTools::Font(9), ESlateDrawEffect::None, OnyxTools::Muted());
		return LayerId + 1;
	}

	const float Scale = GetScale(Area.X);
	const FVector2f TileDraw = FVector2f(Set->GetTileSize()) * Scale;
	const FVector2f Surface = SurfaceSize(Set);
	SheetBrush.SetResourceObject(Set->GetTileSheetTexture());
	SheetBrush.ImageSize = FVector2D(TileDraw);
	for (int32 Y = 0; Y < Set->GetTileCountY(); ++Y)
	{
		for (int32 X = 0; X < Set->GetTileCountX(); ++X)
		{
			// Its slot (an empty tile still shows where it is), then only that tile: its own square of the sheet.
			const FVector2f Min = TileTopLeft(FIntPoint(X, Y), Scale);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(TileDraw, FSlateLayoutTransform(Min)),
				FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.05f, 0.056f, 0.066f));
			const FIntPoint UV = Set->GetTileUVFromTileXY(FIntPoint(X, Y));
			const FVector2f UVMin(UV.X / Surface.X, UV.Y / Surface.Y);
			const FVector2f UVMax((UV.X + Set->GetTileSize().X) / Surface.X, (UV.Y + Set->GetTileSize().Y) / Surface.Y);
			SheetBrush.SetUVRegion(FBox2f(UVMin, UVMax));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(TileDraw, FSlateLayoutTransform(Min)), &SheetBrush);
		}
	}

	// Collision types.
	if (ShowCollision.Get())
	{
		for (int32 Y = 0; Y < Set->GetTileCountY(); ++Y)
		{
			for (int32 X = 0; X < Set->GetTileCountX(); ++X)
			{
				const int32 Index = Y * Set->GetTileCountX() + X;
				const EOnyxTileCollision Type = FOnyxTileCollision::Get(Set, Index);
				const FVector2f Min = TileTopLeft(FIntPoint(X, Y), Scale);
				const FLinearColor Color = FOnyxTileCollision::GetColor(Type);
				if (Type == EOnyxTileCollision::None)
				{
					FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(TileDraw, FSlateLayoutTransform(Min)),
						FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.45f));
					continue;
				}
				const FVector2f Size = Type == EOnyxTileCollision::OneWay ? FVector2f(TileDraw.X, FMath::Max(TileDraw.Y * 0.25f, 2.f)) : TileDraw;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Min)),
					FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color.CopyWithNewOpacity(0.4f));
				Outline(OutDrawElements, LayerId + 3, AllottedGeometry, Min + FVector2f(1.f), Size - FVector2f(2.f), Color, 1.5f);
			}
		}
	}

	// Drag preview or current selection.
	FIntRect Shown = Selection.Get();
	if (DragStart.IsSet() && HoverTile != NoTile)
	{
		Shown = FIntRect(DragStart->ComponentMin(HoverTile), DragStart->ComponentMax(HoverTile) + FIntPoint(1, 1));
	}
	if (Shown.Width() > 0 && Shown.Height() > 0)
	{
		const FVector2f Min = TileTopLeft(Shown.Min, Scale);
		const FVector2f Max = TileTopLeft(Shown.Max - FIntPoint(1, 1), Scale) + TileDraw;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(Max - Min, FSlateLayoutTransform(Min)),
			FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, OnyxTools::Accent().CopyWithNewOpacity(0.18f));
		Outline(OutDrawElements, LayerId + 4, AllottedGeometry, Min - FVector2f(1.f), Max - Min + FVector2f(2.f), OnyxTools::Accent(), 2.5f);
	}
	if (HoverTile != NoTile && !DragStart.IsSet())
	{
		Outline(OutDrawElements, LayerId + 4, AllottedGeometry, TileTopLeft(HoverTile, Scale), TileDraw, FLinearColor(1.f, 1.f, 1.f, 0.7f), 1.5f);
	}
	return LayerId + 4;
}

void SOnyxTilePalette::EmitSelection(const FIntPoint& A, const FIntPoint& B)
{
	const FIntPoint Min = A.ComponentMin(B);
	OnTilesSelected.ExecuteIfBound(Min, A.ComponentMax(B) - Min + FIntPoint(1, 1));
}

FReply SOnyxTilePalette::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	const FIntPoint Tile = TileAt(Geometry, Event.GetScreenSpacePosition());
	if (Tile == NoTile)
	{
		return FReply::Handled();
	}
	DragStart = Tile;
	HoverTile = Tile;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SOnyxTilePalette::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!DragStart.IsSet())
	{
		return FReply::Unhandled();
	}
	EmitSelection(*DragStart, HoverTile != NoTile ? HoverTile : *DragStart);
	DragStart.Reset();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SOnyxTilePalette::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FIntPoint Tile = TileAt(Geometry, Event.GetScreenSpacePosition());
	if (Tile != NoTile || !DragStart.IsSet())
	{
		HoverTile = Tile;
	}
	return FReply::Handled();
}

void SOnyxTilePalette::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	if (!DragStart.IsSet())
	{
		HoverTile = NoTile;
	}
}

FCursorReply SOnyxTilePalette::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	return FCursorReply::Cursor(HoverTile != NoTile ? EMouseCursor::Hand : EMouseCursor::Default);
}

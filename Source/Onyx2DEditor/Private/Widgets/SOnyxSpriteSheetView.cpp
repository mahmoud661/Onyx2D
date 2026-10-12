// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxSpriteSheetView.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "ToolsWindow/OnyxToolWidgets.h"

namespace
{
	constexpr float PreferredWidth = 460.f;
	constexpr int32 MaxGridLines = 512;
}

void SOnyxSpriteSheetView::Construct(const FArguments& InArgs)
{
	Texture = InArgs._Texture;
	CellSize = InArgs._CellSize;
	Margin = InArgs._Margin;
	Spacing = InArgs._Spacing;
	HighlightRows = InArgs._HighlightRows;
	HighlightRects = InArgs._HighlightRects;
	MaxHeight = InArgs._MaxHeight;
}

void SOnyxSpriteSheetView::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(BrushTexture);
}

FVector2D SOnyxSpriteSheetView::ComputeDesiredSize(float) const
{
	const UTexture2D* Sheet = Texture.Get();
	if (!Sheet)
	{
		return FVector2D(PreferredWidth, 90.f);
	}
	const FIntPoint Size = Sheet->GetImportedSize();
	const float Height = Size.X > 0 ? PreferredWidth * Size.Y / Size.X : 90.f;
	return FVector2D(PreferredWidth, FMath::Clamp(Height, 60.f, MaxHeight));
}

int32 SOnyxSpriteSheetView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* White = FAppStyle::GetBrush("WhiteBrush");
	const FVector2D Area = AllottedGeometry.GetLocalSize();

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor(0.02f, 0.02f, 0.025f));

	UTexture2D* Sheet = Texture.Get();
	if (!Sheet)
	{
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Area.X, 20.f), FSlateLayoutTransform(FVector2f(12.f, Area.Y * 0.5f - 8.f))),
			INVTEXT("Pick a texture in the Details panel to preview it here."), OnyxTools::Font(10), ESlateDrawEffect::None, OnyxTools::Muted());
		return LayerId + 1;
	}

	if (BrushTexture != Sheet)
	{
		BrushTexture = Sheet;
		TextureBrush = FSlateBrush();
		TextureBrush.SetResourceObject(Sheet);
		TextureBrush.DrawAs = ESlateBrushDrawType::Image;
	}

	const FIntPoint TextureSize = Sheet->GetImportedSize();
	if (TextureSize.X <= 0 || TextureSize.Y <= 0)
	{
		return LayerId + 1;
	}
	const float Scale = FMath::Min(Area.X / TextureSize.X, Area.Y / TextureSize.Y);
	const FVector2f DrawSize(TextureSize.X * Scale, TextureSize.Y * Scale);
	const FVector2f Origin((Area.X - DrawSize.X) * 0.5f, (Area.Y - DrawSize.Y) * 0.5f);
	TextureBrush.ImageSize = FVector2D(TextureSize);

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Origin)), &TextureBrush);

	const FIntPoint Cell = CellSize.Get();
	const int32 CellMargin = Margin.Get();
	const int32 CellSpacing = Spacing.Get();
	if (Cell.X <= 0 || Cell.Y <= 0)
	{
		return LayerId + 2;
	}
	const int32 Columns = FMath::Max((TextureSize.X - CellMargin * 2 + CellSpacing) / (Cell.X + CellSpacing), 0);
	const int32 RowCount = FMath::Max((TextureSize.Y - CellMargin * 2 + CellSpacing) / (Cell.Y + CellSpacing), 0);

	// Highlighted rows (animations in use).
	for (const int32 Row : HighlightRows.Get())
	{
		if (Row >= 0 && Row < RowCount)
		{
			const float Top = (CellMargin + Row * (Cell.Y + CellSpacing)) * Scale;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
				AllottedGeometry.ToPaintGeometry(FVector2f(DrawSize.X, Cell.Y * Scale), FSlateLayoutTransform(Origin + FVector2f(0.f, Top))),
				White, ESlateDrawEffect::None, OnyxTools::Accent().CopyWithNewOpacity(0.18f));
		}
	}

	// Grid.
	const FLinearColor GridColor(1.f, 1.f, 1.f, 0.28f);
	TArray<FVector2f> Line;
	Line.SetNum(2);
	for (int32 Column = 0; Column <= FMath::Min(Columns, MaxGridLines); ++Column)
	{
		const float X = Origin.X + (CellMargin + Column * (Cell.X + CellSpacing)) * Scale;
		Line[0] = FVector2f(X, Origin.Y);
		Line[1] = FVector2f(X, Origin.Y + DrawSize.Y);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, false, 1.f);
	}
	for (int32 Row = 0; Row <= FMath::Min(RowCount, MaxGridLines); ++Row)
	{
		const float Y = Origin.Y + (CellMargin + Row * (Cell.Y + CellSpacing)) * Scale;
		Line[0] = FVector2f(Origin.X, Y);
		Line[1] = FVector2f(Origin.X + DrawSize.X, Y);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, false, 1.f);

		if (Row < RowCount && Cell.Y * Scale >= 10.f)
		{
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 4,
				AllottedGeometry.ToPaintGeometry(FVector2f(30.f, 14.f), FSlateLayoutTransform(FVector2f(Origin.X + 3.f, Y + 1.f))),
				FString::FromInt(Row), OnyxTools::Font(8, true), ESlateDrawEffect::None, FLinearColor(1.f, 0.9f, 0.4f));
		}
	}
	// Outlined frames.
	const FLinearColor Outline = OnyxTools::Accent();
	TArray<FVector2f> Box;
	Box.SetNum(5);
	for (const FIntRect& Rect : HighlightRects.Get())
	{
		const FVector2f Min = Origin + FVector2f(Rect.Min.X * Scale, Rect.Min.Y * Scale);
		const FVector2f Max = Origin + FVector2f(Rect.Max.X * Scale, Rect.Max.Y * Scale);
		Box[0] = Min;
		Box[1] = FVector2f(Max.X, Min.Y);
		Box[2] = Max;
		Box[3] = FVector2f(Min.X, Max.Y);
		Box[4] = Min;
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 5, AllottedGeometry.ToPaintGeometry(), Box, ESlateDrawEffect::None, Outline, true, 2.f);
	}
	return LayerId + 5;
}

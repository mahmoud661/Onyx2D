// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxSpriteSlicer.h"
#include "Animation/OnyxClip.h"
#include "Builders/OnyxAssetUtils.h"
#include "Builders/OnyxImageAnalysis.h"
#include "Builders/OnyxNormalMaps.h"
#include "DesktopPlatformModule.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "PropertyCustomizationHelpers.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "ToolsWindow/OnyxToolWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

using namespace OnyxTools;

/** The sheet with its grid; click cells (in play order) or drag across them to select frames. */
class SOnyxSlicerCanvas : public SLeafWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SOnyxSlicerCanvas) {}
		SLATE_ATTRIBUTE(UTexture2D*, Texture)
		SLATE_ATTRIBUTE(FIntPoint, CellSize)
		SLATE_ATTRIBUTE(int32, Margin)
		SLATE_ATTRIBUTE(int32, Spacing)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Texture = InArgs._Texture;
		CellSize = InArgs._CellSize;
		Margin = InArgs._Margin;
		Spacing = InArgs._Spacing;
	}

	TArray<FIntPoint> Selection;

	FIntPoint GetGrid() const
	{
		const UTexture2D* Tex = Texture.Get();
		const FIntPoint Cell = CellSize.Get();
		if (!Tex || Cell.X <= 0 || Cell.Y <= 0)
		{
			return FIntPoint::ZeroValue;
		}
		const FIntPoint Size(Tex->Source.GetSizeX(), Tex->Source.GetSizeY());
		const int32 Gap = Spacing.Get();
		return FIntPoint((Size.X - Margin.Get() * 2 + Gap) / (Cell.X + Gap), (Size.Y - Margin.Get() * 2 + Gap) / (Cell.Y + Gap));
	}

	FIntRect CellRect(FIntPoint Cell) const
	{
		const FIntPoint Size = CellSize.Get();
		const FIntPoint Min(Margin.Get() + Cell.X * (Size.X + Spacing.Get()), Margin.Get() + Cell.Y * (Size.Y + Spacing.Get()));
		return FIntRect(Min, Min + Size);
	}

	bool IsEmptyCell(FIntPoint Cell) const
	{
		RefreshEmptyCells();
		const int32 Index = Cell.Y * CachedGrid.X + Cell.X;
		return EmptyCells.IsValidIndex(Index) && EmptyCells[Index];
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Collector.AddReferencedObject(BrushTexture); }
	virtual FString GetReferencerName() const override { return TEXT("SOnyxSlicerCanvas"); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(520.f, 360.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FSlateBrush* White = FAppStyle::GetBrush("WhiteBrush");
		const FVector2f Size = Geometry.GetLocalSize();
		FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor(0.04f, 0.04f, 0.045f));
		UTexture2D* Tex = Texture.Get();
		if (!Tex)
		{
			FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2f(500.f, 20.f), FSlateLayoutTransform(FVector2f(16.f, 16.f))),
				FString(TEXT("Pick a sprite sheet above, or Import PNG.")), Font(10), ESlateDrawEffect::None, Muted());
			return LayerId + 1;
		}
		if (BrushTexture != Tex)
		{
			Brush.SetResourceObject(Tex);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			BrushTexture = Tex;
		}
		const FVector2f TexSize(Tex->Source.GetSizeX(), Tex->Source.GetSizeY());
		Scale = FMath::Max(0.05f, FMath::Min((Size.X - 16.f) / TexSize.X, (Size.Y - 16.f) / TexSize.Y));
		if (Scale >= 1.f)
		{
			Scale = FMath::FloorToFloat(Scale); // whole pixels: crisp
		}
		Origin = FVector2f(8.f, 8.f);
		FSlateDrawElement::MakeBox(Out, LayerId + 1, Geometry.ToPaintGeometry(TexSize * Scale, FSlateLayoutTransform(Origin)), &Brush, ESlateDrawEffect::None, FLinearColor::White);

		const FIntPoint Grid = GetGrid();
		RefreshEmptyCells();
		for (int32 Row = 0; Row < Grid.Y; ++Row)
		{
			for (int32 Column = 0; Column < Grid.X; ++Column)
			{
				const FIntRect Rect = CellRect(FIntPoint(Column, Row));
				const FVector2f Min = Origin + FVector2f(Rect.Min) * Scale;
				const FVector2f CellDraw = FVector2f(Rect.Size()) * Scale;
				const FIntPoint Cell(Column, Row);
				const int32 Order = Selection.IndexOfByKey(Cell);
				if (Order != INDEX_NONE)
				{
					FSlateDrawElement::MakeBox(Out, LayerId + 2, Geometry.ToPaintGeometry(CellDraw, FSlateLayoutTransform(Min)), White, ESlateDrawEffect::None, Accent().CopyWithNewOpacity(0.35f));
					FSlateDrawElement::MakeText(Out, LayerId + 3, Geometry.ToPaintGeometry(FVector2f(40.f, 16.f), FSlateLayoutTransform(Min + FVector2f(3.f, 1.f))),
						FString::FromInt(Order + 1), Font(9, true), ESlateDrawEffect::None, FLinearColor::White);
				}
				else if (IsEmptyCell(Cell))
				{
					FSlateDrawElement::MakeBox(Out, LayerId + 2, Geometry.ToPaintGeometry(CellDraw, FSlateLayoutTransform(Min)), White, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.45f));
				}
				const TArray<FVector2f> Outline = { Min, Min + FVector2f(CellDraw.X, 0.f), Min + CellDraw, Min + FVector2f(0.f, CellDraw.Y), Min };
				FSlateDrawElement::MakeLines(Out, LayerId + 3, Geometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None,
					Order != INDEX_NONE ? Accent() : FLinearColor(1.f, 1.f, 1.f, 0.18f), false, 1.f);
			}
		}
		return LayerId + 4;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}
		const TOptional<FIntPoint> Cell = CellAt(MyGeometry, MouseEvent);
		if (!Cell.IsSet())
		{
			return FReply::Handled();
		}
		DragStart = *Cell;
		PreDrag = Selection;
		bAdditive = MouseEvent.IsShiftDown();
		bDragging = false;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!HasMouseCapture())
		{
			return FReply::Unhandled();
		}
		const TOptional<FIntPoint> Cell = CellAt(MyGeometry, MouseEvent);
		if (Cell.IsSet() && (bDragging || *Cell != DragStart))
		{
			// Drag: every non-empty cell from the start to here, in reading order (like selecting text).
			bDragging = true;
			Selection = bAdditive ? PreDrag : TArray<FIntPoint>();
			const FIntPoint Grid = GetGrid();
			const int32 From = DragStart.Y * Grid.X + DragStart.X;
			const int32 To = Cell->Y * Grid.X + Cell->X;
			const int32 Step = To >= From ? 1 : -1;
			for (int32 Index = From; ; Index += Step)
			{
				const FIntPoint Each(Index % Grid.X, Index / Grid.X);
				if (!IsEmptyCell(Each))
				{
					Selection.AddUnique(Each);
				}
				if (Index == To)
				{
					break;
				}
			}
		}
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (HasMouseCapture() && !bDragging)
		{
			// Click: toggle the cell; frames play in the order they were clicked.
			if (Selection.Contains(DragStart))
			{
				Selection.Remove(DragStart);
			}
			else
			{
				Selection.Add(DragStart);
			}
		}
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

private:
	TOptional<FIntPoint> CellAt(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
	{
		if (!Texture.Get() || Scale <= 0.f)
		{
			return {};
		}
		const FVector2f Pixel = (MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()) - Origin) / Scale;
		const FIntPoint Grid = GetGrid();
		const FIntPoint Size = CellSize.Get();
		const FIntPoint Cell(FMath::FloorToInt((Pixel.X - Margin.Get()) / (Size.X + Spacing.Get())), FMath::FloorToInt((Pixel.Y - Margin.Get()) / (Size.Y + Spacing.Get())));
		if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= Grid.X || Cell.Y >= Grid.Y)
		{
			return {};
		}
		return Cell;
	}

	void RefreshEmptyCells() const
	{
		UTexture2D* Tex = Texture.Get();
		const FIntPoint Grid = GetGrid();
		if (Tex == CachedTexture.Get() && Grid == CachedGrid && CellSize.Get() == CachedCell && Margin.Get() == CachedMargin && Spacing.Get() == CachedSpacing)
		{
			return;
		}
		CachedTexture = Tex;
		CachedGrid = Grid;
		CachedCell = CellSize.Get();
		CachedMargin = Margin.Get();
		CachedSpacing = Spacing.Get();
		EmptyCells.Init(false, Grid.X * Grid.Y);
		TArray<uint8> Alpha;
		FIntPoint Size;
		if (!Tex || !FOnyxImageAnalysis::ReadAlpha(Tex, Alpha, Size))
		{
			return;
		}
		for (int32 Row = 0; Row < Grid.Y; ++Row)
		{
			for (int32 Column = 0; Column < Grid.X; ++Column)
			{
				const FIntRect Rect = CellRect(FIntPoint(Column, Row));
				bool bEmpty = true;
				for (int32 Y = Rect.Min.Y; bEmpty && Y < FMath::Min(Rect.Max.Y, Size.Y); ++Y)
				{
					for (int32 X = Rect.Min.X; X < FMath::Min(Rect.Max.X, Size.X); ++X)
					{
						if (Alpha[Y * Size.X + X] > 8)
						{
							bEmpty = false;
							break;
						}
					}
				}
				EmptyCells[Row * Grid.X + Column] = bEmpty;
			}
		}
	}

	TAttribute<UTexture2D*> Texture;
	TAttribute<FIntPoint> CellSize;
	TAttribute<int32> Margin;
	TAttribute<int32> Spacing;
	mutable FSlateBrush Brush;
	mutable TObjectPtr<UTexture2D> BrushTexture = nullptr;
	mutable float Scale = 1.f;
	mutable FVector2f Origin = FVector2f::ZeroVector;
	mutable TArray<bool> EmptyCells;
	mutable TWeakObjectPtr<UTexture2D> CachedTexture;
	mutable FIntPoint CachedGrid = FIntPoint::ZeroValue;
	mutable FIntPoint CachedCell = FIntPoint::ZeroValue;
	mutable int32 CachedMargin = -1;
	mutable int32 CachedSpacing = -1;
	FIntPoint DragStart = FIntPoint::ZeroValue;
	TArray<FIntPoint> PreDrag;
	bool bAdditive = false;
	bool bDragging = false;
};

void SOnyxSpriteSlicer::Open(UTexture2D* InTexture, const FString& InFolder, const FString& InNamePrefix, FName InRole, FOnOnyxClipSliced InOnCreated)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(INVTEXT("New Clip from Sprite Sheet"))
		.ClientSize(FVector2D(860.f, 640.f))
		.SupportsMinimize(false);
	Window->SetContent(SNew(SOnyxSpriteSlicer).Texture(InTexture).Folder(InFolder).NamePrefix(InNamePrefix).Role(InRole).OnCreated(InOnCreated));
	FSlateApplication::Get().AddWindow(Window);
}

void SOnyxSpriteSlicer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Texture);
}

void SOnyxSpriteSlicer::Construct(const FArguments& InArgs)
{
	Folder = InArgs._Folder.IsEmpty() ? TEXT("/Game/Characters") : InArgs._Folder;
	NamePrefix = InArgs._NamePrefix;
	OnCreated = InArgs._OnCreated;
	if (!InArgs._Role.IsNone())
	{
		Role = InArgs._Role.ToString();
	}

	auto IntBox = [](const FText& Label, TFunction<int32()> Get, TFunction<void(int32)> Set, int32 Min, const FText& Tooltip)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock).Text(Label)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(52.f)
				[
					SNew(SNumericEntryBox<int32>).ToolTipText(Tooltip).MinValue(Min)
					.Value_Lambda([Get]() { return Get(); })
					.OnValueCommitted_Lambda([Set, Min](int32 Value, ETextCommit::Type) { Set(FMath::Max(Value, Min)); })
				]
			];
	};

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(8.f)
		[
			SNew(SVerticalBox)
			// Sheet + grid.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UTexture2D::StaticClass())
					.ObjectPath_Lambda([this]() { return Texture ? Texture->GetPathName() : FString(); })
					.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetTexture(Cast<UTexture2D>(Asset.GetAsset())); })
					.AllowClear(false)
					.ThumbnailPool(nullptr)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton).Text(INVTEXT("Import PNG...")).ToolTipText(INVTEXT("Bring a sprite sheet in from disk."))
					.OnClicked_Lambda([this]() { ImportPng(); return FReply::Handled(); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[IntBox(INVTEXT("Frame W"), [this]() { return CellSize.X; }, [this](int32 V) { CellSize.X = V; }, 1, INVTEXT("Frame width in pixels."))]
				+ SHorizontalBox::Slot().AutoWidth()[IntBox(INVTEXT("H"), [this]() { return CellSize.Y; }, [this](int32 V) { CellSize.Y = V; }, 1, INVTEXT("Frame height in pixels."))]
				+ SHorizontalBox::Slot().AutoWidth()[IntBox(INVTEXT("Gap"), [this]() { return Spacing; }, [this](int32 V) { Spacing = V; }, 0, INVTEXT("Pixels between frames."))]
				+ SHorizontalBox::Slot().AutoWidth()[IntBox(INVTEXT("Border"), [this]() { return Margin; }, [this](int32 V) { Margin = V; }, 0, INVTEXT("Pixels around the sheet."))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton).Text(INVTEXT("Detect Grid")).ToolTipText(INVTEXT("Find the frame size from the transparent gaps."))
					.OnClicked_Lambda([this]() { DetectGrid(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton).Text(INVTEXT("Clear Selection"))
					.OnClicked_Lambda([this]() { Canvas->Selection.Reset(); return FReply::Handled(); })
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(Canvas, SOnyxSlicerCanvas)
				.Texture_Lambda([this]() { return Texture.Get(); })
				.CellSize_Lambda([this]() { return CellSize; })
				.Margin_Lambda([this]() { return Margin; })
				.Spacing_Lambda([this]() { return Spacing; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				Hint(INVTEXT("Click frames in the order they play (click again to remove), or drag across cells to select a run. Shift + drag adds to the selection. Empty cells are skipped."))
			]
			// Name and create.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(STextBlock).Text(INVTEXT("Role"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(140.f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([this]() { return FText::FromString(Role); })
						.ToolTipText(INVTEXT("Idle, Run, Jump, Fall, Crouch, Dash, WallSlide, Attack1, Hurt and Death are played automatically; any other name from Blueprint."))
						.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type) { Role = FOnyxAssetUtils::SanitizeName(Text.ToString()); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 4.f, 0.f)
				[
					SNew(STextBlock).Text(INVTEXT("FPS"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(50.f)
					[
						SNew(SNumericEntryBox<float>).MinValue(0.5f)
						.Value_Lambda([this]() { return FramesPerSecond; })
						.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { FramesPerSecond = FMath::Max(Value, 0.5f); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bLoop ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bLoop = State == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(INVTEXT("Loop"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 4.f, 0.f)
				[
					SNew(STextBlock).Text(INVTEXT("Pixel Size"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(50.f)
					[
						SNew(SNumericEntryBox<float>).MinValue(0.1f).ToolTipText(INVTEXT("World size of one art pixel in cm (same as your tiles: 4 by default)."))
						.Value_Lambda([this]() { return CentimetersPerPixel; })
						.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { CentimetersPerPixel = FMath::Max(Value, 0.1f); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f)
				[
					SNew(STextBlock).Text(this, &SOnyxSpriteSlicer::GetStatus).ColorAndOpacity(Muted())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.Text(INVTEXT("Create Clip"))
					.IsEnabled_Lambda([this]() { return Texture && !Canvas->Selection.IsEmpty() && !Role.IsEmpty(); })
					.OnClicked(this, &SOnyxSpriteSlicer::Create)
				]
			]
		]
	];

	SetTexture(InArgs._Texture);
}

void SOnyxSpriteSlicer::SetTexture(UTexture2D* NewTexture)
{
	Texture = NewTexture;
	if (Canvas.IsValid())
	{
		Canvas->Selection.Reset();
	}
	if (Texture)
	{
		FOnyxAssetUtils::ConfigurePixelArtTexture(Texture);
		DetectGrid();
	}
}

void SOnyxSpriteSlicer::DetectGrid()
{
	if (const TOptional<FIntPoint> Detected = Texture ? FOnyxImageAnalysis::DetectFrameSize(Texture) : TOptional<FIntPoint>())
	{
		CellSize = *Detected;
		Margin = 0;
		Spacing = 0;
	}
	else if (Texture)
	{
		// A single strip: square frames as tall as the image.
		const int32 Height = Texture->Source.GetSizeY();
		CellSize = FIntPoint(Height, Height);
	}
}

void SOnyxSpriteSlicer::ImportPng()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	TArray<FString> Files;
	if (Desktop && Desktop->OpenFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()), TEXT("Import a sprite sheet"),
		TEXT(""), TEXT(""), TEXT("PNG images (*.png)|*.png"), EFileDialogFlags::None, Files) && !Files.IsEmpty())
	{
		const FString Name = TEXT("T_") + FOnyxAssetUtils::SanitizeName(FPaths::GetBaseFilename(Files[0]));
		if (UTexture2D* Imported = FOnyxAssetUtils::ImportTexture(Files[0], Folder / TEXT("Textures"), Name))
		{
			FOnyxAssetUtils::SavePackages({ Imported->GetPackage() });
			SetTexture(Imported);
		}
	}
}

FText SOnyxSpriteSlicer::GetStatus() const
{
	if (!Texture)
	{
		return INVTEXT("No sheet yet");
	}
	const int32 Frames = Canvas.IsValid() ? Canvas->Selection.Num() : 0;
	return FText::Format(INVTEXT("{0} frame(s) selected, {1} s"), Frames, FText::AsNumber(Frames / FMath::Max(FramesPerSecond, 0.5f)));
}

FReply SOnyxSpriteSlicer::Create()
{
	if (!Texture || Canvas->Selection.IsEmpty() || !FOnyxAssetUtils::IsValidContentFolder(Folder))
	{
		return FReply::Handled();
	}
	const FString Prefix = NamePrefix.IsEmpty() ? FOnyxAssetUtils::SanitizeName(Texture->GetName().Replace(TEXT("T_"), TEXT(""))) : NamePrefix;
	const FString Anim = FOnyxAssetUtils::SanitizeName(Role);
	TArray<UPackage*> Packages;

	// Normal map so 2D lights shade the frames in relief.
	UTexture2D* NormalMap = FOnyxNormalMaps::CreateFor(Texture, FOnyxNormalMapSettings());
	if (NormalMap)
	{
		Packages.Add(NormalMap->GetPackage());
	}
	TArray<UPaperSprite*> Sprites;
	const float PixelsPerUnit = 1.f / FMath::Max(CentimetersPerPixel, 0.01f);
	// The clip's frames end at its feet line, so the character stands on its feet.
	TArray<FIntRect> FrameRects;
	for (int32 Index = 0; Index < Canvas->Selection.Num(); ++Index)
	{
		FrameRects.Add(Canvas->CellRect(Canvas->Selection[Index]));
	}
	FrameRects = FOnyxImageAnalysis::CropToFeet(Texture, FrameRects);
	for (int32 Index = 0; Index < Canvas->Selection.Num(); ++Index)
	{
		const FIntRect Rect = FrameRects[Index];
		if (UPaperSprite* Sprite = FOnyxAssetUtils::CreateSprite(Folder / TEXT("Sprites"), FString::Printf(TEXT("S_%s_%s_%02d"), *Prefix, *Anim, Index), Texture,
			Rect.Min, Rect.Size(), PixelsPerUnit, ESpritePivotMode::Bottom_Center))
		{
			if (NormalMap)
			{
				FOnyxNormalMaps::Assign(Sprite, NormalMap);
			}
			Sprites.Add(Sprite);
			Packages.Add(Sprite->GetPackage());
		}
	}
	UPaperFlipbook* Flipbook = FOnyxAssetUtils::CreateFlipbook(Folder / TEXT("Clips"), FString::Printf(TEXT("FB_%s_%s"), *Prefix, *Anim), Sprites, FramesPerSecond);
	UOnyxClip* Clip = Flipbook ? FOnyxAssetUtils::CreateOrReuse<UOnyxClip>(Folder / TEXT("Clips"), FString::Printf(TEXT("CL_%s_%s"), *Prefix, *Anim)) : nullptr;
	if (!Clip)
	{
		Notify(INVTEXT("Could not create the clip (a different asset already uses the name)."), false);
		return FReply::Handled();
	}
	Clip->Modify();
	Clip->Flipbook = Flipbook;
	Clip->bLoop = bLoop;
	Clip->MarkPackageDirty();
	Packages.Add(Flipbook->GetPackage());
	Packages.Add(Clip->GetPackage());
	FOnyxAssetUtils::SavePackages(Packages);

	OnCreated.ExecuteIfBound(Clip, FName(*Anim));
	Notify(FText::Format(INVTEXT("Clip {0}: {1} frames"), FText::FromString(Anim), Sprites.Num()), true);
	// Ready for the next clip from the same sheet.
	Canvas->Selection.Reset();
	return FReply::Handled();
}

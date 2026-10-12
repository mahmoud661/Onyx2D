// Copyright Onyx2D. All Rights Reserved.

#include "ToolsWindow/OnyxToolWidgets.h"
#include "Framework/Notifications/NotificationManager.h"
#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Hides every category except the allowed ones. */
	class FOnyxCategoryFilter : public IDetailCustomization
	{
	public:
		explicit FOnyxCategoryFilter(const TArray<FName>& InAllowed) : Allowed(InAllowed) {}

		virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override
		{
			TArray<FName> Names;
			Builder.GetCategoryNames(Names);
			Names.Append({ TEXT("TransformCommon"), TEXT("Transform") }); // added by the actor customization, not always listed
			for (const FName& Name : Names)
			{
				// An allowed entry matches any category containing it ("Onyx" keeps "Onyx2D", "Onyx 2D"...).
				const FString Category = Name.ToString();
				if (!Allowed.ContainsByPredicate([&Category](const FName& Entry) { return Category.Contains(Entry.ToString()); }))
				{
					Builder.HideCategory(Name);
				}
			}
		}

	private:
		TArray<FName> Allowed;
	};
}

namespace OnyxTools
{
	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FLinearColor Accent()
	{
		return FAppStyle::GetSlateColor("SelectionColor").GetSpecifiedColor();
	}

	FLinearColor Muted()
	{
		return FLinearColor(0.5f, 0.5f, 0.52f);
	}

	TSharedRef<IDetailsView> MakeDetailsView(UObject* Object, bool bAllowSearch)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs Args;
		Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		Args.bAllowSearch = bAllowSearch;
		Args.bHideSelectionTip = true;
		Args.bShowOptions = false;
		Args.bShowPropertyMatrixButton = false;
		Args.bAllowFavoriteSystem = false;
		TSharedRef<IDetailsView> View = PropertyEditor.CreateDetailView(Args);
		View->SetObject(Object);
		return View;
	}

	void ShowOnlyCategories(const TSharedRef<IDetailsView>& View, UClass* Class, const TArray<FName>& Categories)
	{
		View->RegisterInstancedCustomPropertyLayout(Class, FOnGetDetailCustomizationInstance::CreateLambda([Categories]()
		{
			return MakeShared<FOnyxCategoryFilter>(Categories);
		}));
	}

	TSharedRef<SWidget> ToolbarStrip(const TSharedRef<SWidget>& Toolbar, const TSharedPtr<SWidget>& Trailing)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				Toolbar
			];
		if (Trailing.IsValid())
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[Trailing.ToSharedRef()];
		}
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(4.f, 2.f))
			[
				Row
			];
	}

	TSharedRef<SWidget> Section(const TAttribute<FText>& Title, const TSharedRef<SWidget>& Content, const TSharedPtr<SWidget>& HeaderExtra)
	{
		TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Title).Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
			];
		if (HeaderExtra.IsValid())
		{
			Header->AddSlot().AutoWidth().VAlign(VAlign_Center)[HeaderExtra.ToSharedRef()];
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
				.Padding(FMargin(10.f, 6.f))
				[
					Header
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				.Padding(0.f)
				[
					Content
				]
			];
	}

	TSharedRef<SWidget> StatusBar(const TAttribute<FText>& Text)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
			.Padding(FMargin(10.f, 4.f))
			[
				SNew(STextBlock).Text(Text).Font(Font(9)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	TSharedRef<SWidget> Hint(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).AutoWrapText(true).Font(Font(9)).ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}

	void Notify(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = bSuccess ? 4.f : 7.f;
		Info.bUseSuccessFailIcons = true;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}
}

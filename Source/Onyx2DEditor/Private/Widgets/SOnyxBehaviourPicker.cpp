// Copyright Onyx2D. All Rights Reserved.

#include "Widgets/SOnyxBehaviourPicker.h"
#include "Character/OnyxBehaviourLibrary.h"
#include "Combat/OnyxCombatTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText BehaviourName(EOnyxBehaviour Behaviour)
	{
		return StaticEnum<EOnyxBehaviour>()->GetDisplayNameTextByValue(int64(Behaviour));
	}

	FText BehaviourTooltip(EOnyxBehaviour Behaviour)
	{
		return StaticEnum<EOnyxBehaviour>()->GetToolTipTextByIndex(StaticEnum<EOnyxBehaviour>()->GetIndexByValue(int64(Behaviour)));
	}
}

FText SOnyxBehaviourPicker::Describe(int32 Value)
{
	TArray<FString> Names;
	for (const EOnyxBehaviour Behaviour : UOnyxBehaviourLibrary::GetAll())
	{
		if (Value & int32(Behaviour))
		{
			Names.Add(BehaviourName(Behaviour).ToString());
		}
	}
	return Names.IsEmpty() ? INVTEXT("No behaviours") : FText::FromString(FString::Join(Names, TEXT(", ")));
}

FText SOnyxBehaviourPicker::TeamName(uint8 InTeam)
{
	const UEnum* Enum = StaticEnum<EOnyxTeam>();
	return Enum->IsValidEnumValue(InTeam) ? Enum->GetDisplayNameTextByValue(InTeam) : FText::Format(INVTEXT("Team {0}"), InTeam);
}

void SOnyxBehaviourPicker::Construct(const FArguments& InArgs)
{
	Behaviours = InArgs._Behaviours;
	Team = InArgs._Team;
	OnBehavioursChanged = InArgs._OnBehavioursChanged;
	OnTeamChanged = InArgs._OnTeamChanged;

	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SComboButton)
			.ToolTipText(INVTEXT("What the character does. Each behaviour is a component you can also add, remove or tune later in its Blueprint."))
			.OnGetMenuContent(this, &SOnyxBehaviourPicker::MakeMenu)
			.ButtonContent()
			[
				SNew(STextBlock).Text_Lambda([this]() { return Describe(Behaviours.Get()); })
			]
		];
	if (InArgs._ShowTeam)
	{
		Box->AddSlot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
		[
			SNew(SComboButton)
			.ToolTipText(INVTEXT("Who it fights for: attacks and contact damage only hit other teams."))
			.OnGetMenuContent(this, &SOnyxBehaviourPicker::MakeTeamMenu)
			.ButtonContent()
			[
				SNew(STextBlock).Text_Lambda([this]() { return FText::Format(INVTEXT("Team: {0}"), TeamName(Team.Get())); })
			]
		];
	}
	ChildSlot[Box];
}

void SOnyxBehaviourPicker::SetBehaviours(int32 Value)
{
	OnBehavioursChanged.ExecuteIfBound(Value);
}

TSharedRef<SWidget> SOnyxBehaviourPicker::MakeMenu()
{
	FMenuBuilder Menu(false, nullptr);
	Menu.BeginSection(NAME_None, INVTEXT("Behaviours"));
	for (const EOnyxBehaviour Behaviour : UOnyxBehaviourLibrary::GetAll())
	{
		const int32 Bit = int32(Behaviour);
		Menu.AddMenuEntry(BehaviourName(Behaviour), BehaviourTooltip(Behaviour),
			FSlateIconFinder::FindIconForClass(UOnyxBehaviourLibrary::GetComponentClass(Behaviour)),
			FUIAction(FExecuteAction::CreateLambda([this, Bit]() { SetBehaviours(Behaviours.Get() ^ Bit); }), FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Bit]() { return (Behaviours.Get() & Bit) != 0; })),
			NAME_None, EUserInterfaceActionType::ToggleButton);
	}
	Menu.EndSection();

	Menu.BeginSection(NAME_None, INVTEXT("Presets"));
	struct FPreset { FText Label; FText Tooltip; int32 Value; uint8 PresetTeam; };
	const FPreset Presets[] = {
		{ INVTEXT("Hero"), INVTEXT("Player Control, Camera, Respawn; team Player."), int32(UOnyxBehaviourLibrary::PlayerPreset), OnyxTeams::Player },
		{ INVTEXT("Patrolling Foe"), INVTEXT("Patrol, Contact Damage, Stompable; team Enemy."), int32(UOnyxBehaviourLibrary::PatrolFoePreset), OnyxTeams::Enemy },
		{ INVTEXT("Nothing"), INVTEXT("No behaviours: add your own components or logic."), 0, OnyxTeams::Neutral },
	};
	for (const FPreset& Preset : Presets)
	{
		const int32 Value = Preset.Value;
		const uint8 PresetTeam = Preset.PresetTeam;
		Menu.AddMenuEntry(Preset.Label, Preset.Tooltip, FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Value, PresetTeam]()
		{
			SetBehaviours(Value);
			OnTeamChanged.ExecuteIfBound(PresetTeam);
		})));
	}
	Menu.EndSection();
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SOnyxBehaviourPicker::MakeTeamMenu()
{
	FMenuBuilder Menu(true, nullptr);
	for (const uint8 Each : { OnyxTeams::Player, OnyxTeams::Enemy, OnyxTeams::Neutral })
	{
		Menu.AddMenuEntry(TeamName(Each), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Each]() { OnTeamChanged.ExecuteIfBound(Each); }), FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Each]() { return Team.Get() == Each; })),
			NAME_None, EUserInterfaceActionType::RadioButton);
	}
	return Menu.MakeWidget();
}

// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnOnyxBehavioursChanged, int32 /*Behaviours*/);
DECLARE_DELEGATE_OneParam(FOnOnyxTeamChanged, uint8 /*Team*/);

/**
 * Picks a character's behaviours (Player Control, Camera, Respawn, Patrol, Contact Damage, Stompable: each a component)
 * and its team. A drop-down with a check box per behaviour plus quick presets.
 */
class SOnyxBehaviourPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOnyxBehaviourPicker) : _Behaviours(0), _Team(2), _ShowTeam(true) {}
		SLATE_ATTRIBUTE(int32, Behaviours)
		SLATE_ATTRIBUTE(uint8, Team)
		SLATE_ARGUMENT(bool, ShowTeam)
		SLATE_EVENT(FOnOnyxBehavioursChanged, OnBehavioursChanged)
		SLATE_EVENT(FOnOnyxTeamChanged, OnTeamChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** "Player Control, Camera, Respawn" / "No behaviours". */
	static FText Describe(int32 Behaviours);
	static FText TeamName(uint8 Team);

private:
	TSharedRef<SWidget> MakeMenu();
	TSharedRef<SWidget> MakeTeamMenu();
	void SetBehaviours(int32 Value);

	TAttribute<int32> Behaviours;
	TAttribute<uint8> Team;
	FOnOnyxBehavioursChanged OnBehavioursChanged;
	FOnOnyxTeamChanged OnTeamChanged;
};

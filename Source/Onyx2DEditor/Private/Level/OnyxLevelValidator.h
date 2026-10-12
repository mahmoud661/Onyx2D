// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EOnyxIssueSeverity : uint8
{
	Error,
	Warning,
	Info,
	Ok
};

/** One finding of the level check, optionally with a one-click fix. */
struct FOnyxValidationIssue
{
	EOnyxIssueSeverity Severity = EOnyxIssueSeverity::Info;
	FText Message;
	FText FixLabel;
	TFunction<void()> Fix;
	TWeakObjectPtr<AActor> Actor;

	bool HasFix() const { return (bool)Fix; }
};

/** Checks the open level and the project for common 2D setup mistakes. */
class FOnyxLevelValidator
{
public:
	static TArray<TSharedPtr<FOnyxValidationIssue>> Validate();

	/** Renderer / game mode settings that differ from the recommended 2D setup. Empty = already configured. */
	static TArray<FString> GetProjectDifferences();

	/** Applies the recommended 2D renderer and game mode settings to DefaultEngine.ini. */
	static void ConfigureProjectFor2D();
};

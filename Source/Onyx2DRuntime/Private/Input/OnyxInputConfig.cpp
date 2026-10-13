// Copyright Onyx2D. All Rights Reserved.

#include "Input/OnyxInputConfig.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, const TCHAR* Description, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		Action->ActionDescription = FText::FromString(Description);
		return Action;
	}

	void Map(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bNegate = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}
}

UOnyxInputConfig* UOnyxInputConfig::CreateDefault(UObject* Outer)
{
	UOnyxInputConfig* Config = NewObject<UOnyxInputConfig>(Outer, TEXT("OnyxDefaultInput"));
	Config->Move = MakeAction(Config, TEXT("IA_Move"), TEXT("Move"), EInputActionValueType::Axis1D);
	Config->Vertical = MakeAction(Config, TEXT("IA_Vertical"), TEXT("Swim Up / Down"), EInputActionValueType::Axis1D);
	Config->Jump = MakeAction(Config, TEXT("IA_Jump"), TEXT("Jump"), EInputActionValueType::Boolean);
	Config->Dash = MakeAction(Config, TEXT("IA_Dash"), TEXT("Dash"), EInputActionValueType::Boolean);
	Config->Attack = MakeAction(Config, TEXT("IA_Attack"), TEXT("Attack"), EInputActionValueType::Boolean);
	Config->Down = MakeAction(Config, TEXT("IA_Down"), TEXT("Drop Down"), EInputActionValueType::Boolean);
	Config->Interact = MakeAction(Config, TEXT("IA_Interact"), TEXT("Interact"), EInputActionValueType::Boolean);
	Config->Pause = MakeAction(Config, TEXT("IA_Pause"), TEXT("Pause"), EInputActionValueType::Boolean);

	UInputMappingContext* Context = NewObject<UInputMappingContext>(Config, TEXT("IMC_OnyxDefault"));
	Config->MappingContext = Context;

	Map(Context, Config->Move, EKeys::D);
	Map(Context, Config->Move, EKeys::A, true);
	Map(Context, Config->Move, EKeys::Right);
	Map(Context, Config->Move, EKeys::Left, true);
	Map(Context, Config->Move, EKeys::Gamepad_LeftX);
	Map(Context, Config->Move, EKeys::Gamepad_DPad_Right);
	Map(Context, Config->Move, EKeys::Gamepad_DPad_Left, true);

	Map(Context, Config->Vertical, EKeys::Gamepad_LeftY);
	Map(Context, Config->Vertical, EKeys::Gamepad_DPad_Up);

	Map(Context, Config->Jump, EKeys::SpaceBar);
	Map(Context, Config->Jump, EKeys::W);
	Map(Context, Config->Jump, EKeys::Up);
	Map(Context, Config->Jump, EKeys::Gamepad_FaceButton_Bottom);

	Map(Context, Config->Dash, EKeys::LeftShift);
	Map(Context, Config->Dash, EKeys::L);
	Map(Context, Config->Dash, EKeys::Gamepad_RightShoulder);
	Map(Context, Config->Dash, EKeys::Gamepad_FaceButton_Right);

	Map(Context, Config->Attack, EKeys::J);
	Map(Context, Config->Attack, EKeys::LeftMouseButton);
	Map(Context, Config->Attack, EKeys::Gamepad_FaceButton_Left);

	Map(Context, Config->Down, EKeys::S);
	Map(Context, Config->Down, EKeys::Down);
	Map(Context, Config->Down, EKeys::Gamepad_DPad_Down);

	Map(Context, Config->Interact, EKeys::E);
	Map(Context, Config->Interact, EKeys::Gamepad_FaceButton_Top);

	Map(Context, Config->Pause, EKeys::Escape);
	Map(Context, Config->Pause, EKeys::P);
	Map(Context, Config->Pause, EKeys::Gamepad_Special_Right);

	return Config;
}

UOnyxInputSubsystem* UOnyxInputSubsystem::Get(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UOnyxInputSubsystem>() : nullptr;
}

void UOnyxInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DefaultConfig = UOnyxInputConfig::CreateDefault(this);
	ActiveConfig = DefaultConfig;
}

void UOnyxInputSubsystem::SetInputConfig(UOnyxInputConfig* Config)
{
	UOnyxInputConfig* NewConfig = Config ? Config : DefaultConfig.Get();
	if (NewConfig == ActiveConfig)
	{
		ApplyMappingContext();
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (ActiveConfig && ActiveConfig->MappingContext)
		{
			EnhancedInput->RemoveMappingContext(ActiveConfig->MappingContext);
		}
	}
	ActiveConfig = NewConfig;
	ApplyMappingContext();
}

void UOnyxInputSubsystem::ApplyMappingContext()
{
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (EnhancedInput && ActiveConfig && ActiveConfig->MappingContext && !EnhancedInput->HasMappingContext(ActiveConfig->MappingContext))
	{
		EnhancedInput->AddMappingContext(ActiveConfig->MappingContext, 0);
	}
}

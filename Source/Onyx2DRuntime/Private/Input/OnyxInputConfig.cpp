// Copyright Onyx2D. All Rights Reserved.

#include "Input/OnyxInputConfig.h"
#include "Core/Onyx2DSettings.h"
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
	Config->Up = MakeAction(Config, TEXT("IA_Up"), TEXT("Aim Up"), EInputActionValueType::Boolean);
	Config->Mend = MakeAction(Config, TEXT("IA_Mend"), TEXT("Mend"), EInputActionValueType::Boolean);
	Config->Interact = MakeAction(Config, TEXT("IA_Interact"), TEXT("Interact"), EInputActionValueType::Boolean);
	Config->Shoot = MakeAction(Config, TEXT("IA_Shoot"), TEXT("Shoot"), EInputActionValueType::Boolean);
	Config->Throw = MakeAction(Config, TEXT("IA_Throw"), TEXT("Throw"), EInputActionValueType::Boolean);
	Config->Aim = MakeAction(Config, TEXT("IA_Aim"), TEXT("Aim"), EInputActionValueType::Axis2D);
	Config->Pause = MakeAction(Config, TEXT("IA_Pause"), TEXT("Pause"), EInputActionValueType::Boolean);
	Config->HotbarNext = MakeAction(Config, TEXT("IA_HotbarNext"), TEXT("Next Item"), EInputActionValueType::Boolean);
	Config->HotbarPrevious = MakeAction(Config, TEXT("IA_HotbarPrevious"), TEXT("Previous Item"), EInputActionValueType::Boolean);
	Config->HotbarSlot = MakeAction(Config, TEXT("IA_HotbarSlot"), TEXT("Hotbar Slot"), EInputActionValueType::Axis1D);

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
	Map(Context, Config->Jump, EKeys::Gamepad_FaceButton_Bottom);
	// W and Up jump, or aim up (up-slashes) when the project wants Space to be the only jump.
	UInputAction* UpKeys = UOnyx2DSettings::Get()->bUpKeysJump ? Config->Jump.Get() : Config->Up.Get();
	Map(Context, UpKeys, EKeys::W);
	Map(Context, UpKeys, EKeys::Up);

	Map(Context, Config->Mend, EKeys::K);
	Map(Context, Config->Mend, EKeys::F);
	Map(Context, Config->Mend, EKeys::Gamepad_LeftShoulder);
	Map(Context, Config->Mend, EKeys::Gamepad_LeftTrigger);

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

	// Above Attack, Mend and Dash (J K L): I shoots, U throws.
	Map(Context, Config->Shoot, EKeys::I);
	Map(Context, Config->Shoot, EKeys::RightMouseButton);
	Map(Context, Config->Shoot, EKeys::Gamepad_RightTrigger);
	Map(Context, Config->Throw, EKeys::U);
	Map(Context, Config->Throw, EKeys::MiddleMouseButton);
	Map(Context, Config->Throw, EKeys::Gamepad_LeftThumbstick);
	Map(Context, Config->Aim, EKeys::Gamepad_Right2D);

	Map(Context, Config->HotbarNext, EKeys::MouseScrollDown);
	Map(Context, Config->HotbarNext, EKeys::R);
	Map(Context, Config->HotbarNext, EKeys::Gamepad_RightTrigger);
	Map(Context, Config->HotbarPrevious, EKeys::MouseScrollUp);
	Map(Context, Config->HotbarPrevious, EKeys::Q);
	// The number keys give their number (0 is the tenth).
	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NumberKeys); ++Index)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Config->HotbarSlot, NumberKeys[Index]);
		UInputModifierScalar* Scale = NewObject<UInputModifierScalar>(Context);
		Scale->Scalar = FVector(Index + 1);
		Mapping.Modifiers.Add(Scale);
	}

	Map(Context, Config->Pause, EKeys::Escape);
	Map(Context, Config->Pause, EKeys::P);
	Map(Context, Config->Pause, EKeys::Gamepad_Special_Right);

	// From above, W and Up walk up (Down already walks down): laid over the keys above, so they don't jump as well.
	UInputMappingContext* TopDown = NewObject<UInputMappingContext>(Config, TEXT("IMC_OnyxTopDown"));
	Config->TopDownMappingContext = TopDown;
	Map(TopDown, Config->Up, EKeys::W);
	Map(TopDown, Config->Up, EKeys::Up);

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
		if (ActiveConfig && ActiveConfig->TopDownMappingContext)
		{
			EnhancedInput->RemoveMappingContext(ActiveConfig->TopDownMappingContext);
		}
	}
	ActiveConfig = NewConfig;
	ApplyMappingContext();
}

void UOnyxInputSubsystem::SetTopDown(bool bTopDown)
{
	bTopDownKeys = bTopDown;
	ApplyMappingContext();
}

void UOnyxInputSubsystem::ApplyMappingContext()
{
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (EnhancedInput && ActiveConfig && ActiveConfig->MappingContext && !EnhancedInput->HasMappingContext(ActiveConfig->MappingContext))
	{
		EnhancedInput->AddMappingContext(ActiveConfig->MappingContext, 0);
	}
	if (UInputMappingContext* TopDown = EnhancedInput && ActiveConfig ? ActiveConfig->TopDownMappingContext.Get() : nullptr)
	{
		// Above the controls, so its keys win over what they do there.
		const bool bApplied = EnhancedInput->HasMappingContext(TopDown);
		if (bTopDownKeys && !bApplied)
		{
			EnhancedInput->AddMappingContext(TopDown, 1);
		}
		else if (!bTopDownKeys && bApplied)
		{
			EnhancedInput->RemoveMappingContext(TopDown);
		}
	}
}

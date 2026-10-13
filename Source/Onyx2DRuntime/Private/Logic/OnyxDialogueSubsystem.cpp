// Copyright Onyx2D. All Rights Reserved.

#include "Logic/OnyxDialogueSubsystem.h"
#include "Audio/OnyxAudioSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

namespace
{
	/** A press within this many seconds of a conversation opening is the press that opened it. */
	constexpr float OpenGrace = 0.15f;
	/** Seconds a message takes to fade in and out. */
	constexpr float MessageFade = 0.25f;
	/** The voice blip plays every this many letters. */
	constexpr int32 LettersPerVoice = 3;

	// Previews of the dialogue UI, from the console: Onyx2D.Message Hello / Onyx2D.Talk Mossy: Hi | How are you?
	FAutoConsoleCommandWithWorldAndArgs MessageCommand(TEXT("Onyx2D.Message"), TEXT("Shows a message on screen: Onyx2D.Message <text>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(World))
			{
				Dialogue->ShowMessage(FText::FromString(FString::Join(Args, TEXT(" "))), 4.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs TalkCommand(TEXT("Onyx2D.Talk"),
		TEXT("Opens a conversation: Onyx2D.Talk <speaker>: <line> | <line> ... (a line without a speaker keeps the last one)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UOnyxDialogueSubsystem* Dialogue = UOnyxDialogueSubsystem::Get(World);
			if (!Dialogue)
			{
				return;
			}
			TArray<FString> Parts;
			FString::Join(Args, TEXT(" ")).ParseIntoArray(Parts, TEXT("|"));
			TArray<FOnyxDialogueLine> Lines;
			FText Speaker;
			for (const FString& Part : Parts)
			{
				FString Name;
				FString Text = Part.TrimStartAndEnd();
				if (Text.Split(TEXT(":"), &Name, &Text) && !Name.Contains(TEXT(" ")))
				{
					Speaker = FText::FromString(Name.TrimStartAndEnd());
				}
				else
				{
					Text = Part.TrimStartAndEnd();
				}
				FOnyxDialogueLine& Line = Lines.AddDefaulted_GetRef();
				Line.Speaker = Speaker;
				Line.Text = FText::FromString(Text.TrimStartAndEnd());
			}
			Dialogue->StartDialogue(Lines);
		}));
}

UOnyxDialogueSubsystem* UOnyxDialogueSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UOnyxDialogueSubsystem>() : nullptr;
}

bool UOnyxDialogueSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UOnyxDialogueSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOnyxDialogueSubsystem, STATGROUP_Tickables);
}

void UOnyxDialogueSubsystem::ShowMessage(const FText& NewMessage, float Duration)
{
	Message = NewMessage;
	MessageAge = 0.f;
	MessageDuration = FMath::Max(Duration, 0.5f);
	++MessageSerial;
	OnMessageShown.Broadcast(Message);
}

float UOnyxDialogueSubsystem::GetMessageOpacity() const
{
	if (Message.IsEmpty())
	{
		return 0.f;
	}
	const float In = FMath::Clamp(MessageAge / MessageFade, 0.f, 1.f);
	const float Out = FMath::Clamp((MessageDuration - MessageAge) / MessageFade, 0.f, 1.f);
	return FMath::Min(In, Out);
}

void UOnyxDialogueSubsystem::StartDialogue(const TArray<FOnyxDialogueLine>& Lines, float LettersPerSecond)
{
	QueueDialogue(Lines, LettersPerSecond, nullptr);
}

void UOnyxDialogueSubsystem::QueueDialogue(const TArray<FOnyxDialogueLine>& Lines, float LettersPerSecond, TFunction<void()> OnFinished)
{
	FConversation Conversation;
	for (const FOnyxDialogueLine& Line : Lines)
	{
		if (!Line.Text.IsEmpty())
		{
			Conversation.Lines.Add(Line);
		}
	}
	if (Conversation.Lines.Num() == 0)
	{
		// Nothing to read: carry straight on.
		if (OnFinished)
		{
			OnFinished();
		}
		return;
	}
	Conversation.LettersPerSecond = FMath::Max(LettersPerSecond, 0.f);
	Conversation.OnFinished = MoveTemp(OnFinished);
	Conversations.Add(MoveTemp(Conversation));
	if (Conversations.Num() == 1)
	{
		LineIndex = 0;
		OpenTime = 0.f;
		BeginLine();
		OnDialogueStarted.Broadcast();
	}
}

void UOnyxDialogueSubsystem::BeginLine()
{
	ShownLetters = Conversations[0].LettersPerSecond > 0.f ? 0.f : float(GetLineLength());
	LettersSinceVoice = LettersPerVoice; // the first letter speaks
}

int32 UOnyxDialogueSubsystem::GetLineLength() const
{
	return IsTalking() ? Conversations[0].Lines[LineIndex].Text.ToString().Len() : 0;
}

void UOnyxDialogueSubsystem::Advance()
{
	if (!IsTalking() || OpenTime < OpenGrace)
	{
		return;
	}
	if (!IsLineComplete())
	{
		ShownLetters = float(GetLineLength());
		return;
	}
	if (LineIndex + 1 < Conversations[0].Lines.Num())
	{
		++LineIndex;
		BeginLine();
		return;
	}
	FinishConversation();
}

void UOnyxDialogueSubsystem::FinishConversation()
{
	TFunction<void()> OnFinished = MoveTemp(Conversations[0].OnFinished);
	Conversations.RemoveAt(0);
	LineIndex = 0;
	if (IsTalking())
	{
		OpenTime = 0.f;
		BeginLine();
	}
	else
	{
		OnDialogueEnded.Broadcast();
	}
	// Last: what runs next may start another conversation.
	if (OnFinished)
	{
		OnFinished();
	}
}

void UOnyxDialogueSubsystem::EndAllDialogue()
{
	while (IsTalking())
	{
		FinishConversation();
	}
}

FOnyxDialogueLine UOnyxDialogueSubsystem::GetLine() const
{
	return IsTalking() ? Conversations[0].Lines[LineIndex] : FOnyxDialogueLine();
}

FText UOnyxDialogueSubsystem::GetShownText() const
{
	if (!IsTalking())
	{
		return FText::GetEmpty();
	}
	const FText& Text = Conversations[0].Lines[LineIndex].Text;
	if (IsLineComplete())
	{
		return Text;
	}
	return FText::FromString(Text.ToString().Left(FMath::FloorToInt(ShownLetters)));
}

bool UOnyxDialogueSubsystem::IsLineComplete() const
{
	return !IsTalking() || ShownLetters >= float(GetLineLength());
}

bool UOnyxDialogueSubsystem::IsLastLine() const
{
	return IsTalking() && LineIndex == Conversations[0].Lines.Num() - 1;
}

void UOnyxDialogueSubsystem::Tick(float DeltaTime)
{
	if (!Message.IsEmpty())
	{
		MessageAge += DeltaTime;
		if (MessageAge >= MessageDuration)
		{
			Message = FText::GetEmpty();
		}
	}

	if (!IsTalking())
	{
		return;
	}
	OpenTime += DeltaTime;
	const FConversation& Conversation = Conversations[0];
	const int32 Length = GetLineLength();
	if (ShownLetters >= float(Length))
	{
		return;
	}
	const int32 Before = FMath::FloorToInt(ShownLetters);
	ShownLetters = FMath::Min(ShownLetters + Conversation.LettersPerSecond * DeltaTime, float(Length));
	const int32 After = FMath::FloorToInt(ShownLetters);

	// The voice speaks on letters, not on spaces or punctuation.
	const FOnyxDialogueLine& Line = Conversation.Lines[LineIndex];
	if (Line.Voice && After > Before)
	{
		const FString Text = Line.Text.ToString();
		for (int32 Index = Before; Index < After; ++Index)
		{
			if (FChar::IsAlnum(Text[Index]) && ++LettersSinceVoice >= LettersPerVoice)
			{
				LettersSinceVoice = 0;
				UOnyxAudioSubsystem::PlaySound2D(this, Line.Voice, EOnyxSoundCategory::Effects, 0.6f, FMath::FRandRange(0.94f, 1.06f));
				break;
			}
		}
	}
}

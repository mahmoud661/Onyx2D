// Copyright Onyx2D. All Rights Reserved.

#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "PaperFlipbook.h"

UOnyxClip::UOnyxClip()
{
	Lanes.Add(FOnyxClipLane{ TEXT("Lane 1"), GetLanePaletteColor(0) });
}

FLinearColor UOnyxClip::GetLanePaletteColor(int32 Index)
{
	static const FLinearColor Palette[] = {
		FLinearColor::FromSRGBColor(FColor(0x7F, 0x77, 0xDD)), FLinearColor::FromSRGBColor(FColor(0xEF, 0x9F, 0x27)),
		FLinearColor::FromSRGBColor(FColor(0x5D, 0xCA, 0xA5)), FLinearColor::FromSRGBColor(FColor(0xF0, 0x99, 0x7B)),
		FLinearColor::FromSRGBColor(FColor(0x37, 0x8A, 0xDD)), FLinearColor::FromSRGBColor(FColor(0x97, 0xC4, 0x59)),
		FLinearColor::FromSRGBColor(FColor(0xED, 0x93, 0xB1)), FLinearColor::FromSRGBColor(FColor(0xB4, 0xB2, 0xA9)) };
	return Palette[((Index % UE_ARRAY_COUNT(Palette)) + UE_ARRAY_COUNT(Palette)) % UE_ARRAY_COUNT(Palette)];
}

float UOnyxClip::GetLength() const
{
	return Flipbook ? Flipbook->GetTotalDuration() : 0.f;
}

int32 UOnyxClip::GetFrameUnits() const
{
	return Flipbook ? Flipbook->GetNumFrames() : 0;
}

float UOnyxClip::GetFramesPerSecond() const
{
	return Flipbook ? Flipbook->GetFramesPerSecond() : 10.f;
}

void UOnyxClip::SortEvents()
{
	Events.Remove(nullptr);
	Events.StableSort([](const UOnyxClipEvent& A, const UOnyxClipEvent& B) { return A.Time < B.Time; });
}

void UOnyxClip::GetEventsBetween(float From, float To, bool bWrapped, TArray<const UOnyxClipEvent*>& OutEvents) const
{
	for (const UOnyxClipEvent* Event : Events)
	{
		if (!Event || Event->IsWindow())
		{
			continue;
		}
		const bool bInRange = bWrapped ? (Event->Time > From || Event->Time <= To) : (Event->Time > From && Event->Time <= To);
		if (bInRange)
		{
			OutEvents.Add(Event);
		}
	}
}

void UOnyxClip::GetWindowsAt(float Time, TArray<const UOnyxClipWindow*>& OutWindows) const
{
	for (const UOnyxClipEvent* Event : Events)
	{
		if (const UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event); Window && Time >= Window->Time && Time < Window->GetEndTime())
		{
			OutWindows.Add(Window);
		}
	}
}

bool UOnyxClip::HasWindow(TSubclassOf<UOnyxClipWindow> WindowClass) const
{
	return Events.ContainsByPredicate([WindowClass](const UOnyxClipEvent* Event) { return Event && WindowClass && Event->IsA(WindowClass); });
}

FName UOnyxClip::MakeLaneName() const
{
	for (int32 Number = 1;; ++Number)
	{
		const FName Candidate(*FString::Printf(TEXT("Lane %d"), Number));
		if (!Lanes.ContainsByPredicate([&Candidate](const FOnyxClipLane& Lane) { return Lane.Name == Candidate; }))
		{
			return Candidate;
		}
	}
}

void UOnyxClip::FixLanes()
{
	if (Lanes.IsEmpty())
	{
		Lanes.Add(FOnyxClipLane{ TEXT("Lane 1"), GetLanePaletteColor(0) });
	}
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		if (Lanes[Index].Color.Equals(FLinearColor::White))
		{
			Lanes[Index].Color = GetLanePaletteColor(Index); // lanes made before lanes had colours
		}
	}
	for (UOnyxClipEvent* Event : Events)
	{
		if (Event)
		{
			Event->Lane = FMath::Clamp(Event->Lane, 0, Lanes.Num() - 1);
		}
	}
}

void UOnyxClip::PostLoad()
{
	Super::PostLoad();
	FixLanes();
}

#if WITH_EDITOR

int32 UOnyxClip::InsertLane(int32 Index)
{
	if (Lanes.Num() >= MaxLanes)
	{
		return INDEX_NONE;
	}
	Modify();
	Index = FMath::Clamp(Index, 0, Lanes.Num());
	for (UOnyxClipEvent* Event : Events)
	{
		if (Event && Event->Lane >= Index)
		{
			Event->Modify();
			++Event->Lane;
		}
	}
	Lanes.Insert(FOnyxClipLane{ MakeLaneName(), GetLanePaletteColor(Lanes.Num()) }, Index);
	return Index;
}

bool UOnyxClip::RemoveLane(int32 Index)
{
	if (Lanes.Num() <= 1 || !Lanes.IsValidIndex(Index))
	{
		return false;
	}
	Modify();
	for (int32 EventIndex = Events.Num() - 1; EventIndex >= 0; --EventIndex)
	{
		UOnyxClipEvent* Event = Events[EventIndex];
		if (!Event || Event->Lane == Index)
		{
			Events.RemoveAt(EventIndex);
		}
		else if (Event->Lane > Index)
		{
			Event->Modify();
			--Event->Lane;
		}
	}
	Lanes.RemoveAt(Index);
	return true;
}

UOnyxClipEvent* UOnyxClip::AddEvent(TSubclassOf<UOnyxClipEvent> Class, float Time, int32 Lane)
{
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	Modify();
	UOnyxClipEvent* Event = NewObject<UOnyxClipEvent>(this, Class, NAME_None, RF_Transactional);
	Event->Time = FMath::Clamp(Time, 0.f, FMath::Max(GetLength(), 0.f));
	Event->Lane = FMath::Clamp(Lane, 0, Lanes.Num() - 1);
	if (UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event))
	{
		// A new window covers a few frames, inside the clip.
		const float Length = GetLength();
		const float FrameTime = 1.f / FMath::Max(GetFramesPerSecond(), 1.f);
		Window->Length = FMath::Max(UOnyxClipWindow::ShortestLength, Length > 0.f ? FMath::Min(FrameTime * 3.f, Length - Event->Time) : FrameTime * 3.f);
	}
	Events.Add(Event);
	SortEvents();
	return Event;
}

void UOnyxClip::RemoveEvent(UOnyxClipEvent* Event)
{
	if (Event && Events.Contains(Event))
	{
		Modify();
		Events.Remove(Event);
	}
}

void UOnyxClip::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FixLanes();
	SortEvents();
}

#endif

// Copyright Onyx2D. All Rights Reserved.

#include "MovesetEditor/Timeline/OnyxClipTimelineModel.h"
#include "Animation/OnyxClip.h"
#include "Animation/OnyxClipEvent.h"
#include "Animation/OnyxClipWindow.h"
#include "Animation/OnyxMoveset.h"
#include "Editor.h"
#include "PaperFlipbook.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float MinPixelsPerUnit = 8.f;
	constexpr float MaxPixelsPerUnit = 240.f;
}

void FOnyxClipTimelineModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Clip);
	Collector.AddReferencedObjects(Clipboard);
}

// Clip -----------------------------------------------------------------------------------------------------------------

void FOnyxClipTimelineModel::SetClip(UOnyxClip* InClip)
{
	if (Clip == InClip)
	{
		return;
	}
	Clip = InClip;
	Time = 0.f;
	bPlaying = true;
	bLooping = !InClip || InClip->bLoop;
	Selected.Reset();
	FocusedEvent.Reset();
	SelectedFrame = INDEX_NONE;
	ScrollUnits = 0.f;
	ZoomToFit();
	bFitPending = true;
	BroadcastSelection();
}

UPaperFlipbook* FOnyxClipTimelineModel::GetFlipbook() const
{
	return Clip ? Clip->Flipbook.Get() : nullptr;
}

UOnyxMoveset* FOnyxClipTimelineModel::GetMoveset() const
{
	return Clip ? Clip->Moveset.LoadSynchronous() : nullptr;
}

float FOnyxClipTimelineModel::GetFps() const
{
	const UPaperFlipbook* Flipbook = GetFlipbook();
	return Flipbook ? FMath::Max(Flipbook->GetFramesPerSecond(), 0.01f) : 10.f;
}

int32 FOnyxClipTimelineModel::GetUnits() const
{
	const UPaperFlipbook* Flipbook = GetFlipbook();
	return Flipbook ? Flipbook->GetNumFrames() : 0;
}

float FOnyxClipTimelineModel::GetLength() const
{
	return GetUnits() / GetFps();
}

int32 FOnyxClipTimelineModel::GetKeyFrameStart(int32 KeyFrame) const
{
	const UPaperFlipbook* Flipbook = GetFlipbook();
	int32 Units = 0;
	for (int32 Index = 0; Flipbook && Index < KeyFrame && Index < Flipbook->GetNumKeyFrames(); ++Index)
	{
		Units += FMath::Max(Flipbook->GetKeyFrameChecked(Index).FrameRun, 1);
	}
	return Units;
}

int32 FOnyxClipTimelineModel::GetKeyFrameAt(float AtTime) const
{
	const UPaperFlipbook* Flipbook = GetFlipbook();
	if (!Flipbook || Flipbook->GetNumKeyFrames() == 0)
	{
		return INDEX_NONE;
	}
	const int32 Unit = GetUnitAt(AtTime);
	int32 Units = 0;
	for (int32 Index = 0; Index < Flipbook->GetNumKeyFrames(); ++Index)
	{
		Units += FMath::Max(Flipbook->GetKeyFrameChecked(Index).FrameRun, 1);
		if (Unit < Units)
		{
			return Index;
		}
	}
	return Flipbook->GetNumKeyFrames() - 1;
}

int32 FOnyxClipTimelineModel::GetUnitAt(float AtTime) const
{
	return FMath::Clamp(FMath::FloorToInt(AtTime * GetFps() + 0.001f), 0, FMath::Max(GetUnits() - 1, 0));
}

// Playback -------------------------------------------------------------------------------------------------------------

void FOnyxClipTimelineModel::SetTime(float NewTime)
{
	bPlaying = false;
	Time = FMath::Clamp(NewTime, 0.f, GetLength());
}

void FOnyxClipTimelineModel::TogglePlay()
{
	if (!bPlaying && GetLength() > 0.f && Time >= GetLength() - KINDA_SMALL_NUMBER)
	{
		Time = 0.f; // play again from the start
	}
	bPlaying = !bPlaying;
}

void FOnyxClipTimelineModel::StepFrames(int32 Delta)
{
	const int32 Units = GetUnits();
	if (Units <= 0)
	{
		return;
	}
	const int32 Unit = ((GetUnitAt(Time) + Delta) % Units + Units) % Units;
	SetTime(Unit / GetFps());
}

void FOnyxClipTimelineModel::JumpToEnd()
{
	SetTime(FMath::Max(GetUnits() - 1, 0) / GetFps());
}

void FOnyxClipTimelineModel::Tick(float DeltaTime)
{
	const float Length = GetLength();
	if (!bPlaying || !Clip || Length <= 0.f || bDragging)
	{
		return;
	}
	const float Previous = Time;
	Time += DeltaTime * FMath::Max(Clip->PlayRate, 0.01f);
	bool bWrapped = false;
	if (Time >= Length)
	{
		if (bLooping)
		{
			Time = FMath::Fmod(Time, Length);
			bWrapped = true;
		}
		else
		{
			Time = Length;
			bPlaying = false;
		}
	}
	if (bPreviewSounds && GEditor)
	{
		TArray<const UOnyxClipEvent*> Due;
		Clip->GetEventsBetween(Previous, Time, bWrapped, Due);
		for (const UOnyxClipEvent* Event : Due)
		{
			if (const UOnyxClipEvent_Sound* Sound = Cast<UOnyxClipEvent_Sound>(Event); Sound && Sound->Sound)
			{
				GEditor->PlayPreviewSound(Sound->Sound);
			}
		}
	}
}

// View -----------------------------------------------------------------------------------------------------------------

void FOnyxClipTimelineModel::SetViewWidth(float Width)
{
	const float NewWidth = FMath::Max(Width, 1.f);
	if (FMath::IsNearlyEqual(NewWidth, ViewWidth, 0.5f) && !bFitPending)
	{
		return;
	}
	ViewWidth = NewWidth;
	if (bFitPending && Width > 50.f)
	{
		bFitPending = false;
		ZoomToFit();
	}
	ClampScroll();
}

float FOnyxClipTimelineModel::GetScrollableUnits() const
{
	return FMath::Max(GetUnits() + 0.5f, ViewWidth / PixelsPerUnit);
}

void FOnyxClipTimelineModel::ClampScroll()
{
	ScrollUnits = FMath::Clamp(ScrollUnits, 0.f, FMath::Max(GetScrollableUnits() - ViewWidth / PixelsPerUnit, 0.f));
}

void FOnyxClipTimelineModel::Zoom(float Factor, float AnchorX)
{
	const float Anchor = XToUnit(AnchorX);
	PixelsPerUnit = FMath::Clamp(PixelsPerUnit * Factor, MinPixelsPerUnit, MaxPixelsPerUnit);
	ScrollUnits = Anchor - AnchorX / PixelsPerUnit;
	ClampScroll();
}

void FOnyxClipTimelineModel::ScrollTo(float Units)
{
	ScrollUnits = Units;
	ClampScroll();
}

void FOnyxClipTimelineModel::ZoomToFit()
{
	PixelsPerUnit = FMath::Clamp(ViewWidth / (FMath::Max(GetUnits(), 1) + 0.5f), MinPixelsPerUnit, 120.f);
	ScrollUnits = 0.f;
}

float FOnyxClipTimelineModel::Snap(float Seconds, bool bFlip) const
{
	const float Fps = GetFps();
	if (bSnapToFrames != bFlip)
	{
		return FMath::RoundToFloat(Seconds * Fps) / Fps;
	}
	return FMath::RoundToFloat(Seconds * 1000.f) / 1000.f; // any millisecond
}

// Selection ------------------------------------------------------------------------------------------------------------

void FOnyxClipTimelineModel::BroadcastSelection()
{
	OnSelectionChanged.Broadcast();
}

void FOnyxClipTimelineModel::SelectEvent(UOnyxClipEvent* Event, EOnyxSelect Mode)
{
	SelectEvents(Event ? TArray<UOnyxClipEvent*>{ Event } : TArray<UOnyxClipEvent*>(), Mode);
}

void FOnyxClipTimelineModel::SelectEvents(const TArray<UOnyxClipEvent*>& Events, EOnyxSelect Mode)
{
	if (Mode == EOnyxSelect::Replace)
	{
		Selected.Reset();
		FocusedEvent.Reset();
	}
	for (UOnyxClipEvent* Event : Events)
	{
		const bool bWasSelected = IsSelected(Event);
		if (Mode == EOnyxSelect::Toggle && bWasSelected)
		{
			Selected.Remove(Event);
			if (FocusedEvent == Event)
			{
				FocusedEvent.Reset();
			}
			continue;
		}
		if (!bWasSelected)
		{
			Selected.Add(Event);
		}
		FocusedEvent = Event;
	}
	if (!FocusedEvent.IsValid() && !Selected.IsEmpty())
	{
		FocusedEvent = Selected.Last();
	}
	if (!Selected.IsEmpty())
	{
		SelectedFrame = INDEX_NONE;
	}
	BroadcastSelection();
}

void FOnyxClipTimelineModel::ClearSelection()
{
	if (Selected.IsEmpty() && SelectedFrame == INDEX_NONE)
	{
		return;
	}
	Selected.Reset();
	FocusedEvent.Reset();
	SelectedFrame = INDEX_NONE;
	BroadcastSelection();
}

void FOnyxClipTimelineModel::SelectAllEvents()
{
	if (Clip)
	{
		TArray<UOnyxClipEvent*> All;
		for (UOnyxClipEvent* Event : Clip->Events)
		{
			if (Event)
			{
				All.Add(Event);
			}
		}
		SelectEvents(All, EOnyxSelect::Replace);
	}
}

bool FOnyxClipTimelineModel::IsSelected(const UOnyxClipEvent* Event) const
{
	return Event && Selected.Contains(Event);
}

TArray<UOnyxClipEvent*> FOnyxClipTimelineModel::GetSelectedEvents() const
{
	TArray<UOnyxClipEvent*> Result;
	for (const TWeakObjectPtr<UOnyxClipEvent>& Event : Selected)
	{
		if (UOnyxClipEvent* Valid = Event.Get(); Valid && Clip && Clip->Events.Contains(Valid))
		{
			Result.Add(Valid);
		}
	}
	return Result;
}

void FOnyxClipTimelineModel::SelectFrame(int32 KeyFrame)
{
	Selected.Reset();
	FocusedEvent.Reset();
	SelectedFrame = KeyFrame;
	BroadcastSelection();
}

// Edits ----------------------------------------------------------------------------------------------------------------

void FOnyxClipTimelineModel::Begin(const FText& Description)
{
	if (GEditor && !bInTransaction)
	{
		GEditor->BeginTransaction(Description);
		bInTransaction = true;
	}
	if (Clip)
	{
		Clip->Modify();
	}
}

void FOnyxClipTimelineModel::End()
{
	if (GEditor && bInTransaction)
	{
		GEditor->EndTransaction();
	}
	bInTransaction = false;
	if (Clip)
	{
		Clip->MarkPackageDirty();
		if (UPaperFlipbook* Flipbook = GetFlipbook())
		{
			Flipbook->MarkPackageDirty();
		}
	}
	OnEdited.Broadcast();
}

UOnyxClipEvent* FOnyxClipTimelineModel::AddEvent(UClass* Class, float AtTime, int32 Lane)
{
	if (!Clip || !Class)
	{
		return nullptr;
	}
	Begin(FText::Format(INVTEXT("Add {0}"), Class->GetDisplayNameText()));
	UOnyxClipEvent* Event = Clip->AddEvent(Class, Snap(AtTime, false), Lane);
	End();
	LastAddedClass = Class;
	SelectEvent(Event, EOnyxSelect::Replace);
	return Event;
}

UOnyxClipEvent* FOnyxClipTimelineModel::AddMessage(FName Message, float AtTime, int32 Lane)
{
	if (!Clip)
	{
		return nullptr;
	}
	UOnyxMoveset* Moveset = GetMoveset();
	Begin(INVTEXT("Add Message"));
	if (Message.IsNone())
	{
		Message = Moveset ? Moveset->MakeMessageName(TEXT("Message")) : FName(TEXT("Message"));
	}
	if (Moveset && !Moveset->Messages.Contains(Message))
	{
		Moveset->AddMessage(Message);
		Moveset->MarkPackageDirty();
	}
	UOnyxClipEvent_Message* Sender = Cast<UOnyxClipEvent_Message>(Clip->AddEvent(UOnyxClipEvent_Message::StaticClass(), Snap(AtTime, false), Lane));
	if (Sender)
	{
		Sender->Message = Message;
	}
	End();
	SelectEvent(Sender, EOnyxSelect::Replace);
	return Sender;
}

void FOnyxClipTimelineModel::DeleteSelection()
{
	const TArray<UOnyxClipEvent*> Events = GetSelectedEvents();
	if (!Clip || Events.IsEmpty())
	{
		return;
	}
	Begin(Events.Num() > 1 ? INVTEXT("Delete Events") : INVTEXT("Delete Event"));
	for (UOnyxClipEvent* Event : Events)
	{
		Clip->RemoveEvent(Event);
	}
	End();
	ClearSelection();
}

void FOnyxClipTimelineModel::DuplicateSelection()
{
	const TArray<UOnyxClipEvent*> Events = GetSelectedEvents();
	if (!Clip || Events.IsEmpty())
	{
		return;
	}
	Begin(INVTEXT("Duplicate Events"));
	TArray<UOnyxClipEvent*> Copies;
	for (UOnyxClipEvent* Event : Events)
	{
		UOnyxClipEvent* Copy = DuplicateObject<UOnyxClipEvent>(Event, Clip);
		Copy->SetFlags(RF_Transactional);
		Copy->Time = FMath::Min(Event->Time + 1.f / GetFps(), GetLength());
		Clip->Events.Add(Copy);
		Copies.Add(Copy);
	}
	Clip->SortEvents();
	End();
	SelectEvents(Copies, EOnyxSelect::Replace);
}

void FOnyxClipTimelineModel::CopySelection()
{
	const TArray<UOnyxClipEvent*> Events = GetSelectedEvents();
	if (Events.IsEmpty())
	{
		return;
	}
	Clipboard.Reset();
	ClipboardOrigin = TNumericLimits<float>::Max();
	ClipboardFirstLane = MAX_int32;
	for (UOnyxClipEvent* Event : Events)
	{
		Clipboard.Add(DuplicateObject<UOnyxClipEvent>(Event, GetTransientPackage()));
		ClipboardOrigin = FMath::Min(ClipboardOrigin, Event->Time);
		ClipboardFirstLane = FMath::Min(ClipboardFirstLane, Event->Lane);
	}
}

bool FOnyxClipTimelineModel::CanPaste() const
{
	return Clip && !Clipboard.IsEmpty();
}

void FOnyxClipTimelineModel::Paste(float AtTime, int32 AtLane)
{
	if (!CanPaste())
	{
		return;
	}
	Begin(INVTEXT("Paste Events"));
	TArray<UOnyxClipEvent*> Pasted;
	for (UOnyxClipEvent* Source : Clipboard)
	{
		UOnyxClipEvent* Copy = DuplicateObject<UOnyxClipEvent>(Source, Clip);
		Copy->SetFlags(RF_Transactional);
		Copy->Time = FMath::Clamp(Snap(AtTime, false) + Source->Time - ClipboardOrigin, 0.f, GetLength());
		Copy->Lane = FMath::Clamp(AtLane + Source->Lane - ClipboardFirstLane, 0, Clip->Lanes.Num() - 1);
		Clip->Events.Add(Copy);
		Pasted.Add(Copy);
	}
	Clip->SortEvents();
	End();
	SelectEvents(Pasted, EOnyxSelect::Replace);
}

void FOnyxClipTimelineModel::MoveEventsToLane(int32 Lane)
{
	const TArray<UOnyxClipEvent*> Events = GetSelectedEvents();
	if (!Clip || Events.IsEmpty() || !Clip->Lanes.IsValidIndex(Lane))
	{
		return;
	}
	Begin(INVTEXT("Move Events to Lane"));
	for (UOnyxClipEvent* Event : Events)
	{
		Event->Modify();
		Event->Lane = Lane;
	}
	End();
}

void FOnyxClipTimelineModel::SetEventTime(UOnyxClipEvent* Event, float Seconds)
{
	if (!Clip || !Event || !Clip->Events.Contains(Event))
	{
		return;
	}
	const UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event);
	const float Latest = FMath::Max(GetLength() - (Window ? Window->Length : 0.f), 0.f);
	Begin(INVTEXT("Set Event Time"));
	Event->Modify();
	Event->Time = FMath::Clamp(Seconds, 0.f, Latest);
	Clip->SortEvents();
	End();
}

void FOnyxClipTimelineModel::SetWindowLength(UOnyxClipWindow* Window, float Seconds)
{
	if (!Clip || !Window || !Clip->Events.Contains(Window))
	{
		return;
	}
	Begin(INVTEXT("Set Window Length"));
	Window->Modify();
	Window->Length = FMath::Clamp(Seconds, UOnyxClipWindow::ShortestLength, FMath::Max(GetLength() - Window->Time, UOnyxClipWindow::ShortestLength));
	End();
}

void FOnyxClipTimelineModel::BeginMoveSelection()
{
	const TArray<UOnyxClipEvent*> Events = GetSelectedEvents();
	if (!Clip || Events.IsEmpty())
	{
		return;
	}
	Begin(Events.Num() > 1 ? INVTEXT("Move Events") : INVTEXT("Move Event"));
	bDragging = true;
	DragStart.Reset();
	for (UOnyxClipEvent* Event : Events)
	{
		Event->Modify();
		DragStart.Add({ Event, FVector2f(Event->Time, Event->Lane) });
	}
}

void FOnyxClipTimelineModel::UpdateMoveSelection(float DeltaSeconds, int32 DeltaLanes, bool bFlipSnap)
{
	if (!bDragging || DragStart.IsEmpty() || !Clip)
	{
		return;
	}
	// The focused event snaps; the others keep their distance to it.
	const UOnyxClipEvent* Lead = FocusedEvent.Get();
	float LeadStart = DragStart[0].Value.X;
	float MinStart = TNumericLimits<float>::Max();
	float MaxEnd = 0.f;
	int32 MinLane = MAX_int32;
	int32 MaxLane = 0;
	for (const TPair<TWeakObjectPtr<UOnyxClipEvent>, FVector2f>& Start : DragStart)
	{
		const UOnyxClipEvent* Event = Start.Key.Get();
		if (!Event)
		{
			continue;
		}
		if (Event == Lead)
		{
			LeadStart = Start.Value.X;
		}
		const UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event);
		MinStart = FMath::Min(MinStart, Start.Value.X);
		MaxEnd = FMath::Max(MaxEnd, Start.Value.X + (Window ? Window->Length : 0.f));
		MinLane = FMath::Min(MinLane, int32(Start.Value.Y));
		MaxLane = FMath::Max(MaxLane, int32(Start.Value.Y));
	}
	float Delta = Snap(LeadStart + DeltaSeconds, bFlipSnap) - LeadStart;
	Delta = FMath::Clamp(Delta, -MinStart, FMath::Max(GetLength() - MaxEnd, -MinStart));
	DeltaLanes = FMath::Clamp(DeltaLanes, -MinLane, Clip->Lanes.Num() - 1 - MaxLane);
	for (const TPair<TWeakObjectPtr<UOnyxClipEvent>, FVector2f>& Start : DragStart)
	{
		if (UOnyxClipEvent* Event = Start.Key.Get())
		{
			Event->Time = Start.Value.X + Delta;
			Event->Lane = int32(Start.Value.Y) + DeltaLanes;
		}
	}
}

void FOnyxClipTimelineModel::BeginResizeWindow(UOnyxClipWindow* Window)
{
	if (!Clip || !Window)
	{
		return;
	}
	Begin(INVTEXT("Change Window Length"));
	Window->Modify();
	ResizingWindow = Window;
	bDragging = true;
}

void FOnyxClipTimelineModel::UpdateResizeWindow(float EndSeconds, bool bFlipSnap)
{
	if (UOnyxClipWindow* Window = ResizingWindow.Get())
	{
		const float MaxLength = FMath::Max(GetLength() - Window->Time, UOnyxClipWindow::ShortestLength);
		Window->Length = FMath::Clamp(Snap(EndSeconds, bFlipSnap) - Window->Time, UOnyxClipWindow::ShortestLength, MaxLength);
	}
}

void FOnyxClipTimelineModel::BeginFrameHoldDrag(int32 KeyFrame)
{
	UPaperFlipbook* Flipbook = GetFlipbook();
	if (!Flipbook || !Flipbook->IsValidKeyFrameIndex(KeyFrame))
	{
		return;
	}
	Begin(INVTEXT("Change Frame Hold"));
	Flipbook->Modify();
	HoldFrame = KeyFrame;
	bDragging = true;
}

void FOnyxClipTimelineModel::UpdateFrameHoldDrag(int32 Hold)
{
	UPaperFlipbook* Flipbook = GetFlipbook();
	if (Flipbook && Flipbook->IsValidKeyFrameIndex(HoldFrame) && Flipbook->GetKeyFrameChecked(HoldFrame).FrameRun != Hold)
	{
		FScopedFlipbookMutator EditLock(Flipbook);
		EditLock.KeyFrames[HoldFrame].FrameRun = FMath::Clamp(Hold, 1, 64);
	}
}

void FOnyxClipTimelineModel::EndDrag()
{
	if (!bDragging)
	{
		return;
	}
	bDragging = false;
	DragStart.Reset();
	ResizingWindow.Reset();
	HoldFrame = INDEX_NONE;
	if (Clip)
	{
		Clip->SortEvents();
	}
	End();
}

int32 FOnyxClipTimelineModel::GetEventCountOnLane(int32 Lane) const
{
	int32 Count = 0;
	if (Clip)
	{
		for (const UOnyxClipEvent* Event : Clip->Events)
		{
			Count += Event && Event->Lane == Lane ? 1 : 0;
		}
	}
	return Count;
}

void FOnyxClipTimelineModel::InsertLane(int32 Index)
{
	if (!Clip || Clip->Lanes.Num() >= UOnyxClip::MaxLanes)
	{
		return;
	}
	Begin(INVTEXT("Add Lane"));
	Clip->InsertLane(Index);
	End();
}

void FOnyxClipTimelineModel::RemoveLane(int32 Index)
{
	if (!Clip || Clip->Lanes.Num() <= 1)
	{
		return;
	}
	Begin(INVTEXT("Delete Lane"));
	Clip->RemoveLane(Index);
	End();
	ClearSelection();
}

void FOnyxClipTimelineModel::RenameLane(int32 Index, FName Name)
{
	if (!Clip || !Clip->Lanes.IsValidIndex(Index) || Name.IsNone() || Clip->Lanes[Index].Name == Name)
	{
		return;
	}
	Begin(INVTEXT("Rename Lane"));
	Clip->Lanes[Index].Name = Name;
	End();
}

void FOnyxClipTimelineModel::SetLaneColor(int32 Index, const FLinearColor& Color)
{
	if (!Clip || !Clip->Lanes.IsValidIndex(Index))
	{
		return;
	}
	Begin(INVTEXT("Lane Color"));
	Clip->Lanes[Index].Color = Color;
	End();
}

void FOnyxClipTimelineModel::MoveLane(int32 From, int32 To)
{
	if (!Clip || !Clip->Lanes.IsValidIndex(From) || !Clip->Lanes.IsValidIndex(To) || From == To)
	{
		return;
	}
	Begin(INVTEXT("Move Lane"));
	const FOnyxClipLane Moved = Clip->Lanes[From];
	Clip->Lanes.RemoveAt(From);
	Clip->Lanes.Insert(Moved, To);
	for (UOnyxClipEvent* Event : Clip->Events)
	{
		if (!Event)
		{
			continue;
		}
		int32 Lane = Event->Lane;
		if (Lane == From)
		{
			Lane = To;
		}
		else if (From < To && Lane > From && Lane <= To)
		{
			--Lane;
		}
		else if (From > To && Lane >= To && Lane < From)
		{
			++Lane;
		}
		if (Lane != Event->Lane)
		{
			Event->Modify();
			Event->Lane = Lane;
		}
	}
	End();
}

void FOnyxClipTimelineModel::SetFrameHold(int32 KeyFrame, int32 Hold)
{
	UPaperFlipbook* Flipbook = GetFlipbook();
	if (!Flipbook || !Flipbook->IsValidKeyFrameIndex(KeyFrame) || Flipbook->GetKeyFrameChecked(KeyFrame).FrameRun == Hold)
	{
		return;
	}
	Begin(INVTEXT("Change Frame Hold"));
	Flipbook->Modify();
	{
		FScopedFlipbookMutator EditLock(Flipbook);
		EditLock.KeyFrames[KeyFrame].FrameRun = FMath::Clamp(Hold, 1, 64);
	}
	End();
}

void FOnyxClipTimelineModel::DuplicateFrame(int32 KeyFrame)
{
	UPaperFlipbook* Flipbook = GetFlipbook();
	if (!Flipbook || !Flipbook->IsValidKeyFrameIndex(KeyFrame))
	{
		return;
	}
	Begin(INVTEXT("Duplicate Frame"));
	Flipbook->Modify();
	{
		FScopedFlipbookMutator EditLock(Flipbook);
		const FPaperFlipbookKeyFrame Copy = EditLock.KeyFrames[KeyFrame];
		EditLock.KeyFrames.Insert(Copy, KeyFrame + 1);
	}
	End();
	SelectFrame(KeyFrame + 1);
}

void FOnyxClipTimelineModel::DeleteFrame(int32 KeyFrame)
{
	UPaperFlipbook* Flipbook = GetFlipbook();
	if (!Flipbook || !Flipbook->IsValidKeyFrameIndex(KeyFrame) || Flipbook->GetNumKeyFrames() <= 1)
	{
		return;
	}
	Begin(INVTEXT("Delete Frame"));
	Flipbook->Modify();
	{
		FScopedFlipbookMutator EditLock(Flipbook);
		EditLock.KeyFrames.RemoveAt(KeyFrame);
	}
	End();
	SelectFrame(FMath::Min(KeyFrame, Flipbook->GetNumKeyFrames() - 1));
	Time = FMath::Clamp(Time, 0.f, GetLength());
}

void FOnyxClipTimelineModel::MoveFrame(int32 From, int32 To)
{
	UPaperFlipbook* Flipbook = GetFlipbook();
	if (!Flipbook || !Flipbook->IsValidKeyFrameIndex(From) || !Flipbook->IsValidKeyFrameIndex(To) || From == To)
	{
		return;
	}
	Begin(INVTEXT("Move Frame"));
	Flipbook->Modify();
	{
		FScopedFlipbookMutator EditLock(Flipbook);
		const FPaperFlipbookKeyFrame Moved = EditLock.KeyFrames[From];
		EditLock.KeyFrames.RemoveAt(From);
		EditLock.KeyFrames.Insert(Moved, To);
	}
	End();
	SelectFrame(To);
}

void FOnyxClipTimelineModel::SetFramesPerSecond(float Fps)
{
	UPaperFlipbook* Flipbook = GetFlipbook();
	Fps = FMath::Clamp(Fps, 0.5f, 120.f);
	if (!Flipbook || FMath::IsNearlyEqual(Flipbook->GetFramesPerSecond(), Fps))
	{
		return;
	}
	const float Ratio = Flipbook->GetFramesPerSecond() / Fps;
	Begin(INVTEXT("Change Frames per Second"));
	Flipbook->Modify();
	{
		FScopedFlipbookMutator EditLock(Flipbook);
		EditLock.FramesPerSecond = Fps;
	}
	// Events stay on their frames.
	for (UOnyxClipEvent* Event : Clip->Events)
	{
		if (Event)
		{
			Event->Modify();
			Event->Time *= Ratio;
			if (UOnyxClipWindow* Window = Cast<UOnyxClipWindow>(Event))
			{
				Window->Length = FMath::Max(Window->Length * Ratio, UOnyxClipWindow::ShortestLength);
			}
		}
	}
	Time *= Ratio;
	End();
}

void FOnyxClipTimelineModel::NotifyExternalChange()
{
	Selected.RemoveAll([this](const TWeakObjectPtr<UOnyxClipEvent>& Event) { return !Event.IsValid() || !Clip || !Clip->Events.Contains(Event.Get()); });
	if (FocusedEvent.IsValid() && !Selected.Contains(FocusedEvent))
	{
		FocusedEvent.Reset();
	}
	const UPaperFlipbook* Flipbook = GetFlipbook();
	if (!Flipbook || !Flipbook->IsValidKeyFrameIndex(SelectedFrame))
	{
		SelectedFrame = INDEX_NONE;
	}
	Time = FMath::Clamp(Time, 0.f, GetLength());
	OnEdited.Broadcast();
	BroadcastSelection();
}

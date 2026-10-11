// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxWorldSubsystem.h"
#include "Camera/OnyxCameraBounds.h"
#include "Components/PrimitiveComponent.h"

void UOnyxWorldSubsystem::RegisterOneWaySurface(UPrimitiveComponent* Surface)
{
	OneWaySurfaces.AddUnique(Surface);
}

void UOnyxWorldSubsystem::UnregisterOneWaySurface(UPrimitiveComponent* Surface)
{
	OneWaySurfaces.Remove(Surface);
}

bool UOnyxWorldSubsystem::IsOneWaySurface(const UPrimitiveComponent* Surface) const
{
	return Surface && OneWaySurfaces.ContainsByPredicate([Surface](const TWeakObjectPtr<UPrimitiveComponent>& Weak) { return Weak.Get() == Surface; });
}

void UOnyxWorldSubsystem::RegisterCameraBounds(AOnyxCameraBounds* Bounds)
{
	CameraBounds.AddUnique(Bounds);
}

void UOnyxWorldSubsystem::UnregisterCameraBounds(AOnyxCameraBounds* Bounds)
{
	CameraBounds.Remove(Bounds);
}

AOnyxCameraBounds* UOnyxWorldSubsystem::FindCameraBounds(const FVector& Location) const
{
	return FindCameraBounds(Location, [](const AOnyxCameraBounds&) { return true; });
}

AOnyxCameraBounds* UOnyxWorldSubsystem::FindCameraBounds(const FVector& Location, TFunctionRef<bool(const AOnyxCameraBounds&)> Filter) const
{
	AOnyxCameraBounds* Best = nullptr;
	for (const TWeakObjectPtr<AOnyxCameraBounds>& Weak : CameraBounds)
	{
		AOnyxCameraBounds* Bounds = Weak.Get();
		if (Bounds && Bounds->ContainsPoint(Location) && (!Best || Bounds->Priority > Best->Priority) && Filter(*Bounds))
		{
			Best = Bounds;
		}
	}
	return Best;
}

void UOnyxWorldSubsystem::SetLevelBounds(const UObject* Owner, const FBox& Bounds)
{
	LevelBounds.Add(Owner, Bounds);
}

void UOnyxWorldSubsystem::ClearLevelBounds(const UObject* Owner)
{
	LevelBounds.Remove(Owner);
}

FBox UOnyxWorldSubsystem::GetLevelBounds() const
{
	FBox Result(ForceInit);
	for (const TPair<TWeakObjectPtr<const UObject>, FBox>& Pair : LevelBounds)
	{
		if (Pair.Key.IsValid())
		{
			Result += Pair.Value;
		}
	}
	return Result;
}

// Copyright Onyx2D. All Rights Reserved.

#include "Core/OnyxDrawOrderComponent.h"
#include "Core/OnyxDrawOrderSubsystem.h"

UOnyxDrawOrderComponent::UOnyxDrawOrderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOnyxDrawOrderComponent::SetDrawOrder(EOnyxDrawLayer NewLayer, int32 NewOrder)
{
	Layer = NewLayer;
	Order = FMath::Clamp(NewOrder, -Onyx2D::DrawOrderSlots / 2, Onyx2D::DrawOrderSlots / 2 - 1);
	UOnyxDrawOrderSubsystem::RefreshActor(GetOwner());
}

void UOnyxDrawOrderComponent::OnRegister()
{
	Super::OnRegister();
	UOnyxDrawOrderSubsystem::RefreshActor(GetOwner());
}

#if WITH_EDITOR
void UOnyxDrawOrderComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UOnyxDrawOrderSubsystem::RefreshActor(GetOwner());
}
#endif

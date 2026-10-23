// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLightFlash.h"
#include "Engine/World.h"
#include "Lighting/OnyxLight2DComponent.h"

AOnyxLightFlash::AOnyxLightFlash()
{
	PrimaryActorTick.bCanEverTick = true;
	Light2D = CreateDefaultSubobject<UOnyxLight2DComponent>(TEXT("Light2D"));
	RootComponent = Light2D;
}

AOnyxLightFlash* AOnyxLightFlash::Spawn(UObject* WorldContext, FVector Location, FLinearColor Color, float Intensity, float Radius, float InDuration, bool bCastShadows,
	bool bOwnGlow)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AOnyxLightFlash* Flash = World->SpawnActor<AOnyxLightFlash>(AOnyxLightFlash::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Flash)
	{
		Flash->Light2D->Color = Color;
		Flash->Light2D->Intensity = Intensity;
		Flash->Light2D->Radius = Radius;
		Flash->Light2D->bCastShadows = bCastShadows;
		Flash->Light2D->bOwnGlow = bOwnGlow;
		Flash->StartIntensity = Intensity;
		Flash->Duration = FMath::Max(InDuration, 0.02f);
	}
	return Flash;
}

void AOnyxLightFlash::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	const float Alpha = 1.f - FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	Light2D->Intensity = StartIntensity * Alpha * Alpha;
	if (Elapsed >= Duration)
	{
		Destroy();
	}
}

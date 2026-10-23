// Copyright Onyx2D. All Rights Reserved.

#include "World/OnyxMirrorFloor.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace OnyxMirror
{
	const TCHAR* DefaultMaterial = TEXT("/Onyx2D/Materials/M_OnyxMirror.M_OnyxMirror");
	const FName RectParam(TEXT("MirrorArea"));
	const FName StrengthParam(TEXT("Strength"));
	const FName FalloffParam(TEXT("Falloff"));
	const FName TintParam(TEXT("Tint"));

	TAutoConsoleVariable<int32> CVarMirrorFloors(TEXT("onyx.MirrorFloors"), 1, TEXT("Onyx Mirror Floors draw their reflections (1) or not (0)."));
}

AOnyxMirrorFloor::AOnyxMirrorFloor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the camera has moved

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Surface"));
	SetRootComponent(Root);
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetupAttachment(Root);
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetHiddenInGame(true);
	Area->ShapeColor = FColor(170, 210, 255);
	Area->SetCanEverAffectNavigation(false);
	Reflection = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Reflection"));
	Reflection->SetupAttachment(Root);
	Reflection->bUnbound = true; // a screen pass: where it shows is worked out on the screen
	Reflection->bEnabled = false;
	UpdateArea();
}

void AOnyxMirrorFloor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateArea();
}

void AOnyxMirrorFloor::UpdateArea()
{
	if (Area)
	{
		Area->SetBoxExtent(FVector(Width * 0.5f, 8.f, Depth * 0.5f), false);
		Area->SetRelativeLocation(FVector(0.f, 0.f, -Depth * 0.5f));
	}
}

void AOnyxMirrorFloor::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Base = Material.IsNull() ? LoadObject<UMaterialInterface>(nullptr, OnyxMirror::DefaultMaterial) : Material.LoadSynchronous();
	if (Base)
	{
		Instance = UMaterialInstanceDynamic::Create(Base, this);
		Reflection->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Instance));
	}
}

void AOnyxMirrorFloor::SetShown(bool bShown)
{
	if (Reflection->bEnabled != bShown)
	{
		Reflection->bEnabled = bShown;
	}
}

void AOnyxMirrorFloor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	int32 ViewWidth = 0;
	int32 ViewHeight = 0;
	if (!Instance || !Player || OnyxMirror::CVarMirrorFloors.GetValueOnGameThread() == 0)
	{
		SetShown(false);
		return;
	}
	Player->GetViewportSize(ViewWidth, ViewHeight);
	if (ViewWidth <= 0 || ViewHeight <= 0)
	{
		SetShown(false);
		return;
	}
	// The floor on the screen: its ends at the surface, the surface line, and how far down the reflection goes.
	const FVector At = GetActorLocation();
	const float Surface = At.Z;
	FVector2D Left = FVector2D::ZeroVector;
	FVector2D Right = FVector2D::ZeroVector;
	FVector2D Bottom = FVector2D::ZeroVector;
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(Player, FVector(At.X - Width * 0.5f, At.Y, Surface), Left, false)
		&& UGameplayStatics::ProjectWorldToScreen(Player, FVector(At.X + Width * 0.5f, At.Y, Surface), Right, false)
		&& UGameplayStatics::ProjectWorldToScreen(Player, FVector(At.X, At.Y, Surface - Depth), Bottom, false);
	if (!bProjected)
	{
		SetShown(false);
		return;
	}
	const FLinearColor Rect(Left.X / ViewWidth, Right.X / ViewWidth, Left.Y / ViewHeight, (Bottom.Y - Left.Y) / ViewHeight);
	// Out of view: left or right of the screen, or its whole stretch below or above it.
	if (Rect.G < 0.f || Rect.R > 1.f || Rect.B > 1.f || Rect.B + Rect.A < 0.f || Rect.A <= 0.f)
	{
		SetShown(false);
		return;
	}
	Instance->SetVectorParameterValue(OnyxMirror::RectParam, Rect);
	Instance->SetScalarParameterValue(OnyxMirror::StrengthParam, Strength);
	Instance->SetScalarParameterValue(OnyxMirror::FalloffParam, Falloff);
	Instance->SetVectorParameterValue(OnyxMirror::TintParam, Tint);
	SetShown(true);
}

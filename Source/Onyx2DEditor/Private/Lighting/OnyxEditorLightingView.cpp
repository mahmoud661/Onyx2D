// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxEditorLightingView.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Lighting/OnyxLightingSubsystem.h"

void FOnyxEditorLightingView::Start()
{
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FOnyxEditorLightingView::Tick));
	}
}

void FOnyxEditorLightingView::Stop()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

bool FOnyxEditorLightingView::Tick(float DeltaTime)
{
	const FLevelEditorViewportClient* Client = GCurrentLevelEditingViewportClient;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UOnyxLightingSubsystem* Lighting = World ? World->GetSubsystem<UOnyxLightingSubsystem>() : nullptr;
	if (!Client || !Client->Viewport || !Lighting || Client->GetWorld() != World)
	{
		return true;
	}

	FOnyxLightView View;
	View.ViewportSize = Client->Viewport->GetSizeXY();
	View.POV.Location = Client->GetViewLocation();
	if (Client->IsOrtho())
	{
		View.POV.ProjectionMode = ECameraProjectionMode::Orthographic;
		View.POV.OrthoWidth = Client->GetOrthoUnitsPerPixel(Client->Viewport) * View.ViewportSize.X;
		// Ortho views look down a fixed axis: Front (XZ) looks along -Y, at the side of the sprites that faces the camera.
		switch (Client->GetViewportType())
		{
		case LVT_OrthoXZ:         View.POV.Rotation = FRotator(0.f, -90.f, 0.f); break;
		case LVT_OrthoNegativeXZ: View.POV.Rotation = FRotator(0.f, 90.f, 0.f); break;
		default:                  View.POV.Rotation = Client->GetViewRotation(); break;
		}
	}
	else
	{
		View.POV.Rotation = Client->GetViewRotation();
		View.POV.FOV = Client->ViewFOV;
	}
	Lighting->SetEditorView(View);
	return true;
}

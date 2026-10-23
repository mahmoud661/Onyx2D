// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLightVisualizer.h"
#include "GameFramework/Actor.h"
#include "Lighting/OnyxLight2D.h"
#include "Lighting/OnyxLight2DComponent.h"
#include "SceneManagement.h"

namespace OnyxLightGizmo
{
	/** Plane (X right, Y up) -> world, just in front of the sprites so it reads over them. */
	FVector World(const FVector2D& Plane)
	{
		return FVector(Plane.X, -2.f, Plane.Y);
	}

	void Line(FPrimitiveDrawInterface* PDI, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness = 0.f)
	{
		PDI->DrawLine(World(A), World(B), Color, SDPG_Foreground, Thickness);
	}

	/** An arc around Centre from angle From to To (radians), or a whole circle. */
	void Arc(FPrimitiveDrawInterface* PDI, const FVector2D& Centre, float Radius, float From, float To, const FLinearColor& Color, float Thickness = 0.f)
	{
		const int32 Segments = FMath::Clamp(FMath::CeilToInt(FMath::Abs(To - From) / UE_TWO_PI * 64.f), 4, 64);
		FVector2D Last = Centre + Radius * FVector2D(FMath::Cos(From), FMath::Sin(From));
		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = FMath::Lerp(From, To, float(Index) / Segments);
			const FVector2D Next = Centre + Radius * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
			Line(PDI, Last, Next, Color, Thickness);
			Last = Next;
		}
	}

	/** The outline of a rectangle (half extent along Direction and across it) grown by Border, its corners rounded. */
	void RoundedBox(FPrimitiveDrawInterface* PDI, const FVector2D& Centre, const FVector2D& Direction, const FVector2D& Half, float Border,
		const FLinearColor& Color, float Thickness = 0.f)
	{
		const FVector2D Across(-Direction.Y, Direction.X);
		const float Base = FMath::Atan2(Direction.Y, Direction.X);
		auto At = [&](float X, float Y) { return Centre + Direction * X + Across * Y; };
		// Straight sides, then a quarter circle at each corner.
		Line(PDI, At(-Half.X, Half.Y + Border), At(Half.X, Half.Y + Border), Color, Thickness);
		Line(PDI, At(-Half.X, -Half.Y - Border), At(Half.X, -Half.Y - Border), Color, Thickness);
		Line(PDI, At(Half.X + Border, -Half.Y), At(Half.X + Border, Half.Y), Color, Thickness);
		Line(PDI, At(-Half.X - Border, -Half.Y), At(-Half.X - Border, Half.Y), Color, Thickness);
		if (Border > 0.f)
		{
			Arc(PDI, At(Half.X, Half.Y), Border, Base, Base + UE_HALF_PI, Color, Thickness);
			Arc(PDI, At(-Half.X, Half.Y), Border, Base + UE_HALF_PI, Base + UE_PI, Color, Thickness);
			Arc(PDI, At(-Half.X, -Half.Y), Border, Base + UE_PI, Base + 1.5f * UE_PI, Color, Thickness);
			Arc(PDI, At(Half.X, -Half.Y), Border, Base + 1.5f * UE_PI, Base + UE_TWO_PI, Color, Thickness);
		}
	}
}

void FOnyxLightVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	using namespace OnyxLightGizmo;
	const UOnyxLight2DComponent* Light = Cast<UOnyxLight2DComponent>(Component);
	if (!Light)
	{
		return;
	}
	const FVector2D Centre = Light->GetPlanePosition();
	const FVector2D Direction = Light->GetPlaneDirection();
	const float Radius = Light->Radius;
	const FLinearColor Tint = FLinearColor::LerpUsingHSV(Light->Color, FLinearColor::White, 0.35f);
	const FLinearColor Faint = Tint.CopyWithNewOpacity(0.55f) * 0.7f;
	const float Facing = FMath::Atan2(Direction.Y, Direction.X);

	switch (Light->Shape)
	{
	case EOnyxLightShape::Line:
	case EOnyxLightShape::Area:
	{
		// The shape it shines from, and how far past it the light reaches.
		const FVector2D Half = Light->GetShapeHalfExtent();
		RoundedBox(PDI, Centre, Direction, Half, 0.f, Tint, 2.f);
		RoundedBox(PDI, Centre, Direction, Half, Radius, Faint);
		break;
	}
	default:
	{
		const float HalfAngle = FMath::DegreesToRadians(FMath::Clamp(Light->ConeAngle * 0.5f, 1.f, 180.f));
		if (HalfAngle >= UE_PI - 1e-3f)
		{
			Arc(PDI, Centre, Radius, 0.f, UE_TWO_PI, Tint);
			Arc(PDI, Centre, Radius * 0.5f, 0.f, UE_TWO_PI, Faint);
		}
		else
		{
			Arc(PDI, Centre, Radius, Facing - HalfAngle, Facing + HalfAngle, Tint);
			Line(PDI, Centre, Centre + Radius * FVector2D(FMath::Cos(Facing - HalfAngle), FMath::Sin(Facing - HalfAngle)), Tint);
			Line(PDI, Centre, Centre + Radius * FVector2D(FMath::Cos(Facing + HalfAngle), FMath::Sin(Facing + HalfAngle)), Tint);
		}
		break;
	}
	}

	// Its Light Animation's motion: the sweep's limits, the swing's path.
	const UOnyxLightFlickerComponent* Animation = Light->GetOwner() ? Light->GetOwner()->FindComponentByClass<UOnyxLightFlickerComponent>() : nullptr;
	if (Animation && Animation->SweepAngle > 0.f)
	{
		const float Sweep = FMath::DegreesToRadians(Animation->SweepAngle);
		const FLinearColor Sweeping(1.f, 0.85f, 0.3f, 0.8f);
		for (const float Side : { -1.f, 1.f })
		{
			const float Angle = Facing + Side * Sweep;
			Line(PDI, Centre, Centre + Radius * 0.6f * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)), Sweeping);
		}
		Arc(PDI, Centre, Radius * 0.6f, Facing - Sweep, Facing + Sweep, Sweeping);
	}
	if (Animation && Animation->SwayDistance > 0.f)
	{
		// The point it hangs from, and the arc it swings along.
		const float Most = FMath::Asin(FMath::Clamp(Animation->SwayDistance / Animation->SwayLength, 0.f, 1.f));
		const FVector2D Pivot = Centre + FVector2D(0.f, Animation->SwayLength);
		const FLinearColor Swinging(0.5f, 1.f, 0.6f, 0.8f);
		Arc(PDI, Pivot, Animation->SwayLength, -UE_HALF_PI - Most, -UE_HALF_PI + Most, Swinging);
		Line(PDI, Pivot, Centre, Swinging);
	}
}

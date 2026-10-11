// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLightBufferRenderer.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Lighting/OnyxLight2DComponent.h"

namespace
{
	constexpr int32 FalloffSize = 128;

	/** Plane (X right, Y up) -> world. The gameplay plane is Y = 0, which is where lights are evaluated. */
	FVector ToWorld(const FVector2D& Plane)
	{
		return FVector(Plane.X, 0.f, Plane.Y);
	}

	struct FProjector
	{
		FMatrix ViewProjection;
		FVector2D TargetSize;

		/** World point -> render target pixel; false when behind the camera. */
		bool Project(const FVector& World, FVector2D& OutPixel) const
		{
			const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(World, 1.f));
			if (Clip.W <= UE_KINDA_SMALL_NUMBER)
			{
				return false;
			}
			const double X = Clip.X / Clip.W;
			const double Y = Clip.Y / Clip.W;
			OutPixel = FVector2D((X * 0.5 + 0.5) * TargetSize.X, (0.5 - Y * 0.5) * TargetSize.Y);
			return true;
		}
	};

	/** Fraction of the soak depth that stays fully lit before fading (tile surfaces read as lit, not as dark holes). */
	constexpr float SoakFullPart = 0.5f;

	float Luminance(const FLinearColor& Color)
	{
		return 0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B;
	}

	/** 1 inside the cone, fading to 0 over ConeSoftness degrees at its edges. */
	float ConeFade(const FVector2D& Offset, const FVector2D& Direction, float HalfAngle)
	{
		if (HalfAngle >= 179.9f || Offset.IsNearlyZero())
		{
			return 1.f;
		}
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(Offset.GetSafeNormal(), Direction), -1.f, 1.f)));
		const float Soft = FMath::Min(FOnyxLightBufferRenderer::ConeSoftness, HalfAngle);
		return FMath::Clamp((HalfAngle - Angle) / Soft, 0.f, 1.f);
	}
}

UTexture* FOnyxLightBufferRenderer::CreateFalloffTexture(UObject* Outer)
{
	UTexture2D* Texture = UTexture2D::CreateTransient(FalloffSize, FalloffSize, PF_B8G8R8A8, TEXT("OnyxLightFalloff"));
	if (!Texture)
	{
		return nullptr;
	}
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	const float Half = (FalloffSize - 1) * 0.5f;
	for (int32 Y = 0; Y < FalloffSize; ++Y)
	{
		for (int32 X = 0; X < FalloffSize; ++X)
		{
			const float Distance = FMath::Min(FVector2D((X - Half) / Half, (Y - Half) / Half).Size(), 1.f);
			// Smooth falloff with a hot core, reaching exactly 0 at the radius (no visible disc edge).
			const float Value = FMath::Square(1.f - Distance * Distance) * (0.75f + 0.25f * (1.f - Distance));
			const uint8 Byte = uint8(FMath::RoundToInt(FMath::Clamp(Value, 0.f, 1.f) * 255.f));
			Pixels[Y * FalloffSize + X] = FColor(Byte, Byte, Byte, Byte);
		}
	}
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void FOnyxLightBufferRenderer::Render(UWorld* World, const FOnyxLightView& View, UTextureRenderTarget2D* LightTarget, UTextureRenderTarget2D* PositionTarget,
	UTexture* Falloff, TConstArrayView<const UOnyxLight2DComponent*> Lights, TConstArrayView<FOnyxOccluderSegment> Occluders, float LightStrength)
{
	if (!World || !View.IsValid() || !LightTarget || !PositionTarget || !Falloff || !Falloff->GetResource())
	{
		return;
	}
	FMinimalViewInfo POV = View.POV;
	POV.AspectRatio = float(View.ViewportSize.X) / float(View.ViewportSize.Y);
	FMatrix ViewMatrix, ProjectionMatrix, ViewProjection;
	UGameplayStatics::GetViewProjectionMatrix(POV, ViewMatrix, ProjectionMatrix, ViewProjection);
	const FProjector Projector{ ViewProjection, FVector2D(LightTarget->SizeX, LightTarget->SizeY) };
	const FBox2D Screen(FVector2D::ZeroVector, Projector.TargetSize);

	TArray<FCanvasUVTri> LightTris;
	TArray<FCanvasUVTri> PositionTris;
	TArray<FVector2D> Polygon;
	TArray<FVector2D> Soak;
	TArray<FVector2D> Pixels;
	TArray<FVector2D> SoakPixels;
	TArray<float> Fades;

	for (const UOnyxLight2DComponent* Light : Lights)
	{
		const float Brightness = Light ? Light->GetEffectiveIntensity() * LightStrength : 0.f;
		if (Brightness <= KINDA_SMALL_NUMBER || !Light->IsVisible())
		{
			continue;
		}
		const FVector2D Origin = Light->GetPlanePosition();
		const float Radius = Light->Radius;

		// Skip lights whose circle is entirely off screen.
		FVector2D CenterPixel, EdgePixel;
		if (!Projector.Project(ToWorld(Origin), CenterPixel) || !Projector.Project(ToWorld(Origin + FVector2D(Radius, 0.f)), EdgePixel))
		{
			continue;
		}
		const float PixelRadius = FVector2D::Distance(CenterPixel, EdgePixel);
		if (!FBox2D(CenterPixel - FVector2D(PixelRadius), CenterPixel + FVector2D(PixelRadius)).Intersect(Screen))
		{
			continue;
		}

		const FVector2D Direction = Light->GetPlaneDirection();
		const float HalfAngle = Light->ConeAngle * 0.5f;
		FOnyxVisibility2D::Compute(Origin, Radius, Direction, HalfAngle,
			Light->bCastShadows ? Occluders : TConstArrayView<FOnyxOccluderSegment>(), Polygon, Soak);
		if (Polygon.Num() < 2)
		{
			continue;
		}

		Pixels.Reset();
		SoakPixels.Reset();
		Fades.Reset();
		bool bProjected = true;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			FVector2D Pixel, SoakPixel;
			bProjected &= Projector.Project(ToWorld(Polygon[Index]), Pixel);
			bProjected &= Projector.Project(ToWorld(Soak[Index]), SoakPixel);
			Pixels.Add(Pixel);
			SoakPixels.Add(SoakPixel);
			Fades.Add(ConeFade(Polygon[Index] - Origin, Direction, HalfAngle));
		}
		if (!bProjected)
		{
			continue;
		}

		const FLinearColor LightColor = Light->Color * Brightness;
		const float Weight = Luminance(LightColor);
		// Light screen position (+1 keeps it positive) and height, both in screen-height units for the composite.
		const FVector2D LightUV = CenterPixel / Projector.TargetSize;
		const float HeightUV = PixelRadius * Light->Height / Projector.TargetSize.Y;
		const FLinearColor PositionColor(Weight * (LightUV.X + 1.f), Weight * (LightUV.Y + 1.f), Weight * HeightUV, 1.f);

		auto UVOf = [&](const FVector2D& Point) { return FVector2D(0.5f, 0.5f) + (Point - Origin) / (2.f * Radius) * FVector2D(1.f, -1.f); };
		const bool bClosed = HalfAngle >= 179.9f;
		const int32 Count = Polygon.Num();
		const int32 Edges = bClosed ? Count : Count - 1;
		for (int32 Index = 0; Index < Edges; ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			FCanvasUVTri Tri;
			Tri.V0_Pos = CenterPixel;
			Tri.V1_Pos = Pixels[Index];
			Tri.V2_Pos = Pixels[Next];
			Tri.V0_UV = FVector2D(0.5f, 0.5f);
			Tri.V1_UV = UVOf(Polygon[Index]);
			Tri.V2_UV = UVOf(Polygon[Next]);

			Tri.V0_Color = LightColor;
			Tri.V1_Color = LightColor * Fades[Index];
			Tri.V2_Color = LightColor * Fades[Next];
			LightTris.Add(Tri);

			Tri.V0_Color = PositionColor;
			Tri.V1_Color = PositionColor * Fades[Index];
			Tri.V2_Color = PositionColor * Fades[Next];
			PositionTris.Add(Tri);

			// Soak band: from the hit points (lit) into the shape, fading to black, so tile surfaces glow softly.
			const bool bSoaked = !Polygon[Index].Equals(Soak[Index], 0.5f) || !Polygon[Next].Equals(Soak[Next], 0.5f);
			if (!bSoaked)
			{
				continue;
			}
			// Two steps: the surface layer stays fully lit, then the light fades out deeper in the shape.
			const FVector2D MidA = FMath::Lerp(Polygon[Index], Soak[Index], SoakFullPart);
			const FVector2D MidB = FMath::Lerp(Polygon[Next], Soak[Next], SoakFullPart);
			FVector2D MidPixelA, MidPixelB;
			Projector.Project(ToWorld(MidA), MidPixelA);
			Projector.Project(ToWorld(MidB), MidPixelB);
			auto AddQuad = [&](const FVector2D (&Points)[4], const FVector2D (&PlanePoints)[4], const float (&QuadFade)[4])
			{
				for (const FIntVector& Corners : { FIntVector(0, 1, 2), FIntVector(0, 2, 3) })
				{
					FCanvasUVTri Band;
					Band.V0_Pos = Points[Corners.X]; Band.V1_Pos = Points[Corners.Y]; Band.V2_Pos = Points[Corners.Z];
					Band.V0_UV = UVOf(PlanePoints[Corners.X]); Band.V1_UV = UVOf(PlanePoints[Corners.Y]); Band.V2_UV = UVOf(PlanePoints[Corners.Z]);
					Band.V0_Color = LightColor * QuadFade[Corners.X];
					Band.V1_Color = LightColor * QuadFade[Corners.Y];
					Band.V2_Color = LightColor * QuadFade[Corners.Z];
					LightTris.Add(Band);
					Band.V0_Color = PositionColor * QuadFade[Corners.X];
					Band.V1_Color = PositionColor * QuadFade[Corners.Y];
					Band.V2_Color = PositionColor * QuadFade[Corners.Z];
					PositionTris.Add(Band);
				}
			};
			AddQuad({ Pixels[Index], MidPixelA, MidPixelB, Pixels[Next] }, { Polygon[Index], MidA, MidB, Polygon[Next] },
				{ Fades[Index], Fades[Index], Fades[Next], Fades[Next] });
			AddQuad({ MidPixelA, SoakPixels[Index], SoakPixels[Next], MidPixelB }, { MidA, Soak[Index], Soak[Next], MidB },
				{ Fades[Index], 0.f, 0.f, Fades[Next] });
		}
	}

	const FTexture* FalloffResource = Falloff->GetResource();
	auto Draw = [&](UTextureRenderTarget2D* Target, const TArray<FCanvasUVTri>& Tris)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(World, Target, FLinearColor::Black);
		if (Tris.IsEmpty())
		{
			return;
		}
		UCanvas* Canvas = nullptr;
		FVector2D Size;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Target, Canvas, Size, Context);
		if (Canvas)
		{
			FCanvasTriangleItem Item(Tris, FalloffResource);
			Item.BlendMode = SE_BLEND_Additive;
			Canvas->DrawItem(Item);
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
	};
	Draw(LightTarget, LightTris);
	Draw(PositionTarget, PositionTris);
}

// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxLightBufferRenderer.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Lighting/OnyxLight2DComponent.h"

namespace OnyxLightBuffer
{
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

	float SmoothStep01(float From, float To, float Value)
	{
		const float T = FMath::Clamp((Value - From) / FMath::Max(To - From, 1e-4f), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** True when segment A-B crosses segment C-D (touching A, where the ray starts, doesn't count). */
	bool Crosses(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		const FVector2D AB = B - A;
		const FVector2D CD = D - C;
		const double Denominator = FVector2D::CrossProduct(AB, CD);
		if (FMath::Abs(Denominator) < 1e-6)
		{
			return false;
		}
		const FVector2D AC = C - A;
		const double T = FVector2D::CrossProduct(AC, CD) / Denominator;
		const double U = FVector2D::CrossProduct(AC, AB) / Denominator;
		return T > 1e-4 && T < 1.0 - 1e-4 && U >= 0.0 && U <= 1.0;
	}

	/** The falloff atlas cell a light's UVs (0..1 over its reach) land in. */
	FVector2D AtlasUV(const FVector2D& UV, int32 Cell)
	{
		return FVector2D((Cell + FMath::Clamp(UV.X, 0.f, 1.f)) / FOnyxLightBufferRenderer::FalloffCells, UV.Y);
	}

	/** Triangles for each texture they are drawn with (the falloff atlas, each pattern), per buffer. */
	struct FTriangles
	{
		TMap<const FTexture*, TArray<FCanvasUVTri>> Light;
		TMap<const FTexture*, TArray<FCanvasUVTri>> Position;
		TMap<const FTexture*, TArray<FCanvasUVTri>> OwnGlow;
		TMap<const FTexture*, TArray<FCanvasUVTri>> Halo;
	};

	uint32 HashFloat(uint32 Hash, double Value, double Step)
	{
		return HashCombineFast(Hash, GetTypeHash(int64(FMath::RoundToDouble(Value / Step))));
	}
}

float FOnyxLightBufferRenderer::EvaluateFalloff(EOnyxLightFalloff Falloff, float Distance)
{
	using namespace OnyxLightBuffer;
	const float D = FMath::Clamp(Distance, 0.f, 1.f);
	switch (Falloff)
	{
	case EOnyxLightFalloff::Soft:
	{
		// A glow: bright at the heart, gone well before the edge, no rim anywhere.
		const float Tail = FMath::Exp(-4.f);
		return (FMath::Exp(-4.f * D * D) - Tail) / (1.f - Tail);
	}
	case EOnyxLightFalloff::Sharp:
		// A hot core inside a pool with a crisp edge.
		return (0.35f + 0.65f * FMath::Exp(-9.f * D * D)) * (1.f - SmoothStep01(0.86f, 1.f, D));
	case EOnyxLightFalloff::Even:
		// Flat, then a short soft edge.
		return 1.f - SmoothStep01(0.7f, 1.f, D);
	case EOnyxLightFalloff::Smooth:
	default:
		// A warm core easing out to exactly 0 at the reach (no visible disc edge).
		return FMath::Square(1.f - D * D) * (0.75f + 0.25f * (1.f - D));
	}
}

UTexture* FOnyxLightBufferRenderer::CreateFalloffTexture(UObject* Outer)
{
	const int32 Width = FalloffCells * FalloffCellSize;
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, FalloffCellSize, PF_B8G8R8A8, TEXT("OnyxLightFalloff"));
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
	const float Half = (FalloffCellSize - 1) * 0.5f;
	for (int32 Cell = 0; Cell < FalloffCells; ++Cell)
	{
		const EOnyxLightFalloff Falloff = static_cast<EOnyxLightFalloff>(Cell);
		for (int32 Y = 0; Y < FalloffCellSize; ++Y)
		{
			for (int32 X = 0; X < FalloffCellSize; ++X)
			{
				const float Distance = FVector2D((X - Half) / Half, (Y - Half) / Half).Size();
				const float Value = Distance >= 1.f ? 0.f : EvaluateFalloff(Falloff, Distance);
				const uint8 Byte = uint8(FMath::RoundToInt(FMath::Clamp(Value, 0.f, 1.f) * 255.f));
				Pixels[Y * Width + Cell * FalloffCellSize + X] = FColor(Byte, Byte, Byte, Byte);
			}
		}
	}
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void FOnyxLightBufferRenderer::Render(UWorld* World, const FOnyxLightView& View, UTextureRenderTarget2D* LightTarget, UTextureRenderTarget2D* PositionTarget,
	UTextureRenderTarget2D* OwnGlowTarget, UTexture* Falloff, TConstArrayView<const UOnyxLight2DComponent*> Lights,
	TConstArrayView<FOnyxOccluderSegment> Occluders, float LightStrength, FOnyxLightShadowCaches* Caches, uint32 OccluderVersion,
	FOnyxLightBufferStats* OutStats, TConstArrayView<FOnyxPixelLightQuad> PixelLights, UTextureRenderTarget2D* HaloTarget)
{
	using namespace OnyxLightBuffer;
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
	const FTexture* FalloffResource = Falloff->GetResource();

	FTriangles Triangles;
	FOnyxLightBufferStats Stats;
	TArray<FVector2D> LocalPolygon;
	TArray<FVector2D> LocalSoak;
	TArray<float> LocalVisibility;
	TArray<FVector2D> Pixels;
	TArray<FVector2D> SoakPixels;
	TArray<float> Fades;
	TArray<FOnyxOccluderSegment> Near;

	for (const UOnyxLight2DComponent* Light : Lights)
	{
		const float Brightness = Light ? Light->GetEffectiveIntensity() * LightStrength : 0.f;
		if (Brightness <= KINDA_SMALL_NUMBER || !Light->IsVisible())
		{
			continue;
		}
		const FVector2D Origin = Light->GetPlanePosition();
		const float Radius = FMath::Max(Light->Radius, 1.f);
		const float Reach = Light->GetReach();

		// Skip lights entirely off screen.
		FVector2D CenterPixel, EdgePixel, RadiusPixel;
		if (!Projector.Project(ToWorld(Origin), CenterPixel) || !Projector.Project(ToWorld(Origin + FVector2D(Reach, 0.f)), EdgePixel)
			|| !Projector.Project(ToWorld(Origin + FVector2D(Radius, 0.f)), RadiusPixel))
		{
			continue;
		}
		const float PixelReach = FVector2D::Distance(CenterPixel, EdgePixel);
		if (!FBox2D(CenterPixel - FVector2D(PixelReach), CenterPixel + FVector2D(PixelReach)).Intersect(Screen))
		{
			continue;
		}
		++Stats.Drawn;

		const FVector2D Direction = Light->GetPlaneDirection();
		const FVector2D Across(-Direction.Y, Direction.X);
		const bool bShadows = Light->bCastShadows && !Occluders.IsEmpty();
		const FLinearColor LightColor = Light->Color * Brightness;
		const float Weight = Luminance(LightColor);
		// Light screen position (+1 keeps it positive) and height, both in screen-height units for the composite.
		const FVector2D LightUV = CenterPixel / Projector.TargetSize;
		const float HeightUV = FVector2D::Distance(CenterPixel, RadiusPixel) * Light->Height / Projector.TargetSize.Y;
		const FLinearColor PositionColor(Weight * (LightUV.X + 1.f), Weight * (LightUV.Y + 1.f), Weight * HeightUV, 1.f);
		const int32 Cell = FMath::Clamp(int32(Light->Falloff), 0, FalloffCells - 1);
		const FTexture* PatternResource = Light->Pattern && (Light->Shape == EOnyxLightShape::Point || Light->Shape == EOnyxLightShape::Spot)
			? Light->Pattern->GetResource() : nullptr;
		const FTexture* Texture = PatternResource ? PatternResource : FalloffResource;
		TArray<FCanvasUVTri>& LightTris = Triangles.Light.FindOrAdd(Texture);
		TArray<FCanvasUVTri>& PositionTris = Triangles.Position.FindOrAdd(Texture);
		const int32 FirstTri = LightTris.Num();

		// What its shadow depends on: kept from an earlier frame while none of it changes.
		const FVector2D HalfExtent = Light->GetShapeHalfExtent();
		const bool bGrid = Light->Shape == EOnyxLightShape::Line || Light->Shape == EOnyxLightShape::Area;
		const float HalfAngle = bGrid ? 180.f : FMath::Clamp(Light->ConeAngle * 0.5f, 1.f, 180.f);
		uint32 Key = GetTypeHash(int32(Light->Shape));
		Key = HashFloat(Key, Origin.X, 0.5);
		Key = HashFloat(Key, Origin.Y, 0.5);
		Key = HashFloat(Key, Radius, 0.5);
		Key = HashFloat(Key, HalfAngle, 0.05);
		Key = HashFloat(Key, HalfExtent.X, 0.5);
		Key = HashFloat(Key, HalfExtent.Y, 0.5);
		if (HalfAngle < 179.9f || bGrid)
		{
			Key = HashFloat(Key, Direction.X, 0.001);
			Key = HashFloat(Key, Direction.Y, 0.001);
		}
		Key = HashCombineFast(Key, bShadows ? OccluderVersion + 1 : 0);
		FOnyxLightShadowCache* Cache = Caches ? &Caches->FindOrAdd(TObjectKey<UOnyxLight2DComponent>(Light)) : nullptr;
		const bool bReuse = Cache && Cache->bValid && Cache->Key == Key;
		if (bReuse)
		{
			++Stats.ShadowsReused;
		}
		else
		{
			++Stats.ShadowsComputed;
		}

		if (!bGrid)
		{
			// --- A point or a spot: a fan clipped to what it can see, soaking a little into the walls it lights. ---
			TArray<FVector2D>& Polygon = Cache ? Cache->Polygon : LocalPolygon;
			TArray<FVector2D>& Soak = Cache ? Cache->Soak : LocalSoak;
			if (!bReuse)
			{
				FOnyxVisibility2D::Compute(Origin, Radius, Direction, HalfAngle, bShadows ? Occluders : TConstArrayView<FOnyxOccluderSegment>(), Polygon, Soak);
				if (Cache)
				{
					Cache->Key = Key;
					Cache->bValid = true;
				}
			}
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
			// The falloff's cell in the atlas, or the pattern turned with the light (it covers the whole reach).
			const float Cos = Direction.X;
			const float Sin = Direction.Y;
			auto UVOf = [&](const FVector2D& Point)
			{
				const FVector2D Offset = (Point - Origin) / (2.f * Radius);
				if (PatternResource)
				{
					const FVector2D Local(Offset.X * Cos + Offset.Y * Sin, -Offset.X * Sin + Offset.Y * Cos);
					return FVector2D(0.5f + Local.X, 0.5f - Local.Y);
				}
				return AtlasUV(FVector2D(0.5f + Offset.X, 0.5f - Offset.Y), Cell);
			};
			const FVector2D CentreUV = UVOf(Origin);
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
				Tri.V0_UV = CentreUV;
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
		else
		{
			// --- A line or an area: a grid, each point lit by its distance to the shape, dark when a wall stands between it
			// and the nearest point of the light. Fine enough that the falloff reads smooth and shadows soften by a cell. ---
			const FVector2D Span = HalfExtent + FVector2D(Radius);
			float Step = FMath::Clamp(Radius / 5.f, 12.f, 64.f);
			int32 ColumnsCount = FMath::CeilToInt(2.f * Span.X / Step) + 1;
			int32 RowsCount = FMath::CeilToInt(2.f * Span.Y / Step) + 1;
			while (ColumnsCount * RowsCount > 3000)
			{
				Step *= 1.25f;
				ColumnsCount = FMath::CeilToInt(2.f * Span.X / Step) + 1;
				RowsCount = FMath::CeilToInt(2.f * Span.Y / Step) + 1;
			}
			const float StepX = 2.f * Span.X / FMath::Max(ColumnsCount - 1, 1);
			const float StepY = 2.f * Span.Y / FMath::Max(RowsCount - 1, 1);
			const int32 Points = ColumnsCount * RowsCount;
			auto PlaneAt = [&](int32 Column, int32 Row, FVector2D& OutLocal)
			{
				OutLocal = FVector2D(-Span.X + Column * StepX, -Span.Y + Row * StepY);
				return Origin + Direction * OutLocal.X + Across * OutLocal.Y;
			};
			TArray<float>& Visibility = Cache ? Cache->Visibility : LocalVisibility;
			if (!bReuse || Visibility.Num() != Points)
			{
				Visibility.SetNumUninitialized(Points);
				Near.Reset();
				if (bShadows)
				{
					const FBox2D Bounds(Origin - FVector2D(Reach), Origin + FVector2D(Reach));
					for (const FOnyxOccluderSegment& Segment : Occluders)
					{
						const FBox2D SegmentBox(FVector2D::Min(Segment.A, Segment.B), FVector2D::Max(Segment.A, Segment.B));
						if (Bounds.Intersect(SegmentBox))
						{
							Near.Add(Segment);
						}
					}
				}
				for (int32 Row = 0; Row < RowsCount; ++Row)
				{
					for (int32 Column = 0; Column < ColumnsCount; ++Column)
					{
						FVector2D Local;
						const FVector2D At = PlaneAt(Column, Row, Local);
						const FVector2D Nearest = Origin + Direction * FMath::Clamp(Local.X, -HalfExtent.X, HalfExtent.X)
							+ Across * FMath::Clamp(Local.Y, -HalfExtent.Y, HalfExtent.Y);
						bool bLit = true;
						for (const FOnyxOccluderSegment& Segment : Near)
						{
							if (Crosses(Nearest, At, Segment.A, Segment.B))
							{
								bLit = false;
								break;
							}
						}
						Visibility[Row * ColumnsCount + Column] = bLit ? 1.f : 0.f;
					}
				}
				if (Cache)
				{
					Cache->Key = Key;
					Cache->bValid = true;
				}
			}
			// Each point's brightness from its distance to the shape; the texture adds nothing (its cell's full middle).
			TArray<float> Value;
			Value.SetNumUninitialized(Points);
			Pixels.SetNumUninitialized(Points);
			bool bProjected = true;
			for (int32 Row = 0; Row < RowsCount; ++Row)
			{
				for (int32 Column = 0; Column < ColumnsCount; ++Column)
				{
					FVector2D Local;
					const FVector2D At = PlaneAt(Column, Row, Local);
					const float OutX = FMath::Max(FMath::Abs(Local.X) - HalfExtent.X, 0.f);
					const float OutY = FMath::Max(FMath::Abs(Local.Y) - HalfExtent.Y, 0.f);
					const int32 Index = Row * ColumnsCount + Column;
					Value[Index] = EvaluateFalloff(Light->Falloff, FMath::Sqrt(OutX * OutX + OutY * OutY) / Radius) * Visibility[Index];
					bProjected &= Projector.Project(ToWorld(At), Pixels[Index]);
				}
			}
			if (!bProjected)
			{
				continue;
			}
			const FVector2D Full = AtlasUV(FVector2D(0.5f, 0.5f), Cell);
			auto AddTri = [&](int32 A, int32 B, int32 C)
			{
				if (Value[A] + Value[B] + Value[C] <= 1e-3f)
				{
					return;
				}
				FCanvasUVTri Tri;
				Tri.V0_Pos = Pixels[A]; Tri.V1_Pos = Pixels[B]; Tri.V2_Pos = Pixels[C];
				Tri.V0_UV = Full; Tri.V1_UV = Full; Tri.V2_UV = Full;
				Tri.V0_Color = LightColor * Value[A]; Tri.V1_Color = LightColor * Value[B]; Tri.V2_Color = LightColor * Value[C];
				LightTris.Add(Tri);
				Tri.V0_Color = PositionColor * Value[A]; Tri.V1_Color = PositionColor * Value[B]; Tri.V2_Color = PositionColor * Value[C];
				PositionTris.Add(Tri);
			};
			for (int32 Row = 0; Row + 1 < RowsCount; ++Row)
			{
				for (int32 Column = 0; Column + 1 < ColumnsCount; ++Column)
				{
					const int32 A = Row * ColumnsCount + Column;
					AddTri(A, A + 1, A + ColumnsCount + 1);
					AddTri(A, A + ColumnsCount + 1, A + ColumnsCount);
				}
			}
		}
		// Own glows go into the light buffer like any light, and again on their own, to be taken off guarded sprites (Light Guard).
		if (Light->bOwnGlow && OwnGlowTarget)
		{
			Triangles.OwnGlow.FindOrAdd(Texture).Append(LightTris.GetData() + FirstTri, LightTris.Num() - FirstTri);
		}
	}

	// Pixel lights: each a picture of its light, laid where its sprite is.
	for (const FOnyxPixelLightQuad& Quad : PixelLights)
	{
		if (!Quad.Texture)
		{
			continue;
		}
		FVector2D Pixels4[4];
		bool bProjected = true;
		FBox2D Bounds(ForceInit);
		FVector2D Centre = FVector2D::ZeroVector;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			bProjected &= Projector.Project(ToWorld(Quad.Corners[Index]), Pixels4[Index]);
			Bounds += Pixels4[Index];
			Centre += Quad.Corners[Index] * 0.25f;
		}
		if (!bProjected || !Bounds.Intersect(Screen))
		{
			continue;
		}
		++Stats.PixelLightsDrawn;
		const FLinearColor LightColor = Quad.Color * LightStrength; // black when only its halo shows
		FVector2D CentrePixel;
		Projector.Project(ToWorld(Centre), CentrePixel);
		const FVector2D LightUV = CentrePixel / Projector.TargetSize;
		const float HeightUV = 0.5f * Bounds.GetSize().Y * 0.25f / Projector.TargetSize.Y;
		const float Weight = Luminance(LightColor);
		const FLinearColor PositionColor(Weight * (LightUV.X + 1.f), Weight * (LightUV.Y + 1.f), Weight * HeightUV, 1.f);
		TArray<FCanvasUVTri>& LightTris = Triangles.Light.FindOrAdd(Quad.Texture);
		TArray<FCanvasUVTri>& PositionTris = Triangles.Position.FindOrAdd(Quad.Texture);
		for (const FIntVector& Corners : { FIntVector(0, 1, 2), FIntVector(0, 2, 3) })
		{
			FCanvasUVTri Tri;
			Tri.V0_Pos = Pixels4[Corners.X]; Tri.V1_Pos = Pixels4[Corners.Y]; Tri.V2_Pos = Pixels4[Corners.Z];
			Tri.V0_UV = Quad.UVs[Corners.X]; Tri.V1_UV = Quad.UVs[Corners.Y]; Tri.V2_UV = Quad.UVs[Corners.Z];
			Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = LightColor;
			LightTris.Add(Tri);
			if (Quad.bOwnGlow && OwnGlowTarget)
			{
				Triangles.OwnGlow.FindOrAdd(Quad.Texture).Add(Tri);
			}
			if (HaloTarget && Quad.Halo.GetMax() > KINDA_SMALL_NUMBER)
			{
				FCanvasUVTri Glow = Tri;
				Glow.V0_Color = Glow.V1_Color = Glow.V2_Color = Quad.Halo;
				Triangles.Halo.FindOrAdd(Quad.HaloTexture ? Quad.HaloTexture : Quad.Texture).Add(Glow);
			}
			Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = PositionColor;
			PositionTris.Add(Tri);
		}
	}

	auto Draw = [&](UTextureRenderTarget2D* Target, const TMap<const FTexture*, TArray<FCanvasUVTri>>& Groups)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(World, Target, FLinearColor::Black);
		bool bAny = false;
		for (const TPair<const FTexture*, TArray<FCanvasUVTri>>& Group : Groups)
		{
			bAny |= !Group.Value.IsEmpty();
		}
		if (!bAny)
		{
			return;
		}
		UCanvas* Canvas = nullptr;
		FVector2D Size;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Target, Canvas, Size, Context);
		if (Canvas)
		{
			for (const TPair<const FTexture*, TArray<FCanvasUVTri>>& Group : Groups)
			{
				if (!Group.Value.IsEmpty())
				{
					FCanvasTriangleItem Item(Group.Value, Group.Key);
					Item.BlendMode = SE_BLEND_Additive;
					Canvas->DrawItem(Item);
				}
			}
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
	};
	Draw(LightTarget, Triangles.Light);
	Draw(PositionTarget, Triangles.Position);
	if (OwnGlowTarget)
	{
		Draw(OwnGlowTarget, Triangles.OwnGlow);
	}
	if (HaloTarget)
	{
		Draw(HaloTarget, Triangles.Halo);
	}
	if (OutStats)
	{
		*OutStats = Stats;
	}
}

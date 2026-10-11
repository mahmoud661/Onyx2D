// Copyright Onyx2D. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A wall edge that blocks 2D light, in plane coordinates (X right, Y up). */
struct FOnyxOccluderSegment
{
	FVector2D A;
	FVector2D B;
	/** How far light soaks into the shape behind this edge, so solid tiles get a lit surface layer. */
	float Penetration = 0.f;
};

/**
 * 2D shadows by visibility polygon: the area a light reaches, clipped by occluder edges.
 * Rays go to every edge end (and just past it) plus a ring at the light radius, which gives exact,
 * hard-edged shadows from any number of rectangles or convex outlines. Pure maths, no engine state.
 */
class ONYX2DRUNTIME_API FOnyxVisibility2D
{
public:
	/**
	 * @param Origin       Light position.
	 * @param Radius       Light reach.
	 * @param Direction    Centre of the light cone (unit vector); ignored when HalfAngle >= 180.
	 * @param HalfAngle    Half of the cone, in degrees (180 = point light).
	 * @param Segments     Occluder edges; only those near the light matter.
	 * @param OutPolygon   Where each ray hits (fully lit), in angle order. Closed loop for point lights, an arc
	 *                     for cones (draw it as a triangle fan from Origin).
	 * @param OutSoak      Where the light that soaked into the hit shape fades out (one per OutPolygon point;
	 *                     equal to it when the ray hit nothing). Draw the band between both, fading to black.
	 */
	static void Compute(const FVector2D& Origin, float Radius, const FVector2D& Direction, float HalfAngle,
		TConstArrayView<FOnyxOccluderSegment> Segments, TArray<FVector2D>& OutPolygon, TArray<FVector2D>& OutSoak);

	/** Rays around the edge of the light so the outline stays round where nothing blocks it. */
	static constexpr int32 RingRays = 48;
};

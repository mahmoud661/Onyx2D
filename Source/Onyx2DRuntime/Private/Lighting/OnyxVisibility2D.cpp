// Copyright Onyx2D. All Rights Reserved.

#include "Lighting/OnyxVisibility2D.h"

namespace
{
	constexpr double AngleEpsilon = 0.0005; // radians: rays just either side of an edge end
	constexpr double Pi = UE_DOUBLE_PI;

	/** Distance along the ray to the segment, or a negative value when it misses. */
	double RaySegment(const FVector2D& Origin, const FVector2D& Dir, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D Edge = B - A;
		const double Denominator = Dir.X * Edge.Y - Dir.Y * Edge.X;
		if (FMath::Abs(Denominator) < UE_DOUBLE_SMALL_NUMBER)
		{
			return -1.0; // parallel
		}
		const FVector2D ToA = A - Origin;
		const double T = (ToA.X * Edge.Y - ToA.Y * Edge.X) / Denominator; // along the ray
		const double U = (ToA.X * Dir.Y - ToA.Y * Dir.X) / Denominator;   // along the edge
		return (T > 0.0 && U >= 0.0 && U <= 1.0) ? T : -1.0;
	}

	double WrapAngle(double Angle)
	{
		while (Angle <= -Pi) { Angle += 2.0 * Pi; }
		while (Angle > Pi) { Angle -= 2.0 * Pi; }
		return Angle;
	}
}

void FOnyxVisibility2D::Compute(const FVector2D& Origin, float Radius, const FVector2D& Direction, float HalfAngle,
	TConstArrayView<FOnyxOccluderSegment> Segments, TArray<FVector2D>& OutPolygon, TArray<FVector2D>& OutSoak)
{
	OutPolygon.Reset();
	OutSoak.Reset();
	if (Radius <= 0.f)
	{
		return;
	}
	const bool bCone = HalfAngle < 179.9f;
	const double ConeCenter = FMath::Atan2(Direction.Y, Direction.X);
	const double ConeHalf = FMath::DegreesToRadians(FMath::Clamp(HalfAngle, 1.f, 180.f));

	// Only edges that can touch the light circle.
	TArray<const FOnyxOccluderSegment*, TInlineAllocator<128>> Near;
	const FBox2D Reach(Origin - FVector2D(Radius), Origin + FVector2D(Radius));
	for (const FOnyxOccluderSegment& Segment : Segments)
	{
		FBox2D Box(ForceInit);
		Box += Segment.A;
		Box += Segment.B;
		if (Box.Intersect(Reach))
		{
			Near.Add(&Segment);
		}
	}

	// Angles relative to the cone centre (or absolute for point lights), in (-Pi, Pi].
	TArray<double, TInlineAllocator<512>> Angles;
	auto AddAngle = [&](double Absolute)
	{
		const double Relative = bCone ? WrapAngle(Absolute - ConeCenter) : WrapAngle(Absolute);
		if (!bCone || FMath::Abs(Relative) <= ConeHalf)
		{
			Angles.Add(Relative);
		}
	};
	for (int32 Index = 0; Index < RingRays; ++Index)
	{
		AddAngle(bCone ? ConeCenter - ConeHalf + 2.0 * ConeHalf * Index / (RingRays - 1) : -Pi + 2.0 * Pi * Index / RingRays);
	}
	if (bCone)
	{
		Angles.Add(-ConeHalf);
		Angles.Add(ConeHalf);
	}
	for (const FOnyxOccluderSegment* Segment : Near)
	{
		for (const FVector2D& End : { Segment->A, Segment->B })
		{
			const FVector2D ToEnd = End - Origin;
			if (ToEnd.SizeSquared() > FMath::Square(Radius * 1.5))
			{
				continue;
			}
			const double Angle = FMath::Atan2(ToEnd.Y, ToEnd.X);
			AddAngle(Angle - AngleEpsilon);
			AddAngle(Angle);
			AddAngle(Angle + AngleEpsilon);
		}
	}
	Angles.Sort();

	OutPolygon.Reserve(Angles.Num());
	OutSoak.Reserve(Angles.Num());
	for (const double Relative : Angles)
	{
		const double Absolute = bCone ? Relative + ConeCenter : Relative;
		const FVector2D Dir(FMath::Cos(Absolute), FMath::Sin(Absolute));
		double Nearest = Radius;
		float Soak = 0.f;
		for (const FOnyxOccluderSegment* Segment : Near)
		{
			const double T = RaySegment(Origin, Dir, Segment->A, Segment->B);
			if (T >= 0.0 && T < Nearest)
			{
				Nearest = T;
				Soak = Segment->Penetration;
			}
		}
		// Light soaks a little into what it hits (tile surfaces get lit, not just the air in front).
		OutPolygon.Add(Origin + Dir * Nearest);
		OutSoak.Add(Origin + Dir * FMath::Min(Nearest + Soak, double(Radius)));
	}
}

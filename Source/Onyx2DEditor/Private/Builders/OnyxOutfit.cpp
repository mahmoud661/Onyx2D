// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxOutfit.h"

FOnyxOutfit::FOnyxOutfit()
{
	PartColors = {
		{ FColor(0x5F, 0xCD, 0xE4), EOnyxOutfitPart::Head }, { FColor(0x64, 0xB9, 0xE3), EOnyxOutfitPart::HeadShade },
		{ FColor(0xDF, 0x71, 0x26), EOnyxOutfitPart::Torso }, { FColor(0xC9, 0x66, 0x24), EOnyxOutfitPart::TorsoShade },
		{ FColor(0xC4, 0x64, 0x23), EOnyxOutfitPart::TorsoShade }, { FColor(0xB3, 0x5B, 0x20), EOnyxOutfitPart::TorsoDark },
		{ FColor(0xAC, 0x32, 0x32), EOnyxOutfitPart::RightArmUpper }, { FColor(0x9C, 0x30, 0x4D), EOnyxOutfitPart::RightArmLower },
		{ FColor(0x76, 0x42, 0x8A), EOnyxOutfitPart::LeftArmUpper }, { FColor(0x67, 0x40, 0x85), EOnyxOutfitPart::LeftArmLower },
		{ FColor(0x5A, 0x93, 0x57), EOnyxOutfitPart::RightLegDark }, { FColor(0x6F, 0xA3, 0x33), EOnyxOutfitPart::RightLegDark },
		{ FColor(0x7F, 0xBD, 0x39), EOnyxOutfitPart::RightLegLight },
		{ FColor(0xC0, 0x73, 0xC7), EOnyxOutfitPart::LeftLegDark }, { FColor(0xD7, 0x7B, 0xBA), EOnyxOutfitPart::LeftLegLight },
		{ FColor(0xFF, 0xFF, 0xFF), EOnyxOutfitPart::Steel },
	};
}


FOnyxPixelCanvas FOnyxOutfit::DressStrip(const FOnyxPixelCanvas& Strip, EOnyxDirection Direction) const
{
	const int32 Frame = Strip.Size.Y;
	FOnyxPixelCanvas Result(Strip.Size.X, Strip.Size.Y);
	for (int32 Index = 0; Frame > 0 && Index < Strip.Size.X / Frame; ++Index)
	{
		FOnyxPixelCanvas One(Frame, Frame);
		for (int32 Y = 0; Y < Frame; ++Y)
		{
			for (int32 X = 0; X < Frame; ++X)
			{
				One.Set(X, Y, Strip.Get(Index * Frame + X, Y));
			}
		}
		const FOnyxPixelCanvas Dressed = Dress(One, Direction);
		for (int32 Y = 0; Y < Frame; ++Y)
		{
			for (int32 X = 0; X < Frame; ++X)
			{
				Result.Set(Index * Frame + X, Y, Dressed.Get(X, Y));
			}
		}
	}
	return Result;
}


EOnyxOutfitPart FOnyxOutfit::Classify(const FColor& Color) const
{
	if (Color.A == 0)
	{
		return EOnyxOutfitPart::None;
	}
	for (const FPartColor& Entry : PartColors)
	{
		if (FMath::Abs(int32(Entry.Color.R) - Color.R) + FMath::Abs(int32(Entry.Color.G) - Color.G) + FMath::Abs(int32(Entry.Color.B) - Color.B) <= ColorTolerance)
		{
			return Entry.Part;
		}
	}
	return EOnyxOutfitPart::None;
}


namespace OnyxOutfitHelpers
{
	inline bool IsArm(EOnyxOutfitPart Part) { return Part >= EOnyxOutfitPart::RightArmUpper && Part <= EOnyxOutfitPart::LeftArmLower; }
	inline bool IsLeg(EOnyxOutfitPart Part) { return Part >= EOnyxOutfitPart::RightLegDark && Part <= EOnyxOutfitPart::LeftLegLight; }
	inline bool IsTorso(EOnyxOutfitPart Part) { return Part >= EOnyxOutfitPart::Torso && Part <= EOnyxOutfitPart::TorsoDark; }
	inline bool IsHead(EOnyxOutfitPart Part) { return Part == EOnyxOutfitPart::Head || Part == EOnyxOutfitPart::HeadShade; }

	/** Steps from Seeds through the pixels In marks (8 ways round), -1 where it never gets. */
	inline TArray<int32> Distances(const TArray<uint8>& In, const TArray<int32>& Seeds, int32 Width, int32 Height)
	{
		TArray<int32> Distance;
		for (int32 Index = 0; Index < Width * Height; ++Index)
		{
			Distance.Add(-1);
		}
		TArray<int32> Queue;
		for (const int32 Seed : Seeds)
		{
			if (Distance[Seed] < 0)
			{
				Distance[Seed] = 0;
				Queue.Add(Seed);
			}
		}
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 At = Queue[Head], X = At % Width, Y = At / Width;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const int32 NX = X + DX, NY = Y + DY;
					if ((DX || DY) && NX >= 0 && NY >= 0 && NX < Width && NY < Height && In[NY * Width + NX] && Distance[NY * Width + NX] < 0)
					{
						Distance[NY * Width + NX] = Distance[At] + 1;
						Queue.Add(NY * Width + NX);
					}
				}
			}
		}
		return Distance;
	}
}

FOnyxPixelCanvas FOnyxOutfit::DrawHead(const TArray<uint8>& Mask, const TArray<uint8>& Shade, int32 Width, int32 Height, int32 Margin, EOnyxDirection Direction) const
{
	const int32 CW = Width + Margin * 2, CH = Height + Margin * 2;
	FOnyxPixelCanvas Canvas(CW, CH);
	TArray<uint8> HairAt;
	for (int32 Index = 0; Index < CW * CH; ++Index)
	{
		HairAt.Add(0);
	}
	auto In = [&](int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < Width && Y < Height && Mask[Y * Width + X]; };
	const bool bLong = HairStyle == EOnyxHairStyle::Long, bBald = HairStyle == EOnyxHairStyle::Bald;
	// The face looks right (its back on the left) except straight down or up.
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (!In(X, Y))
			{
				continue;
			}
			const float U = (X + 0.5f) / Width, V = (Y + 0.5f) / Height;
			const float Fringe = (X % 3 == 1) ? 0.1f : 0.f; // a ragged fringe
			bool bHair = false;
			switch (Direction)
			{
			case EOnyxDirection::Up: bHair = true; break;
			case EOnyxDirection::UpSide: bHair = !(U > 0.78f && V > 0.42f && V < 0.88f); break;
			case EOnyxDirection::Side: bHair = V < 0.36f + Fringe || (U < 0.5f && V < (bLong ? 1.f : 0.86f)) || (U < 0.6f && V < 0.55f); break;
			case EOnyxDirection::DownSide: bHair = V < 0.38f + Fringe || (U < 0.26f && V < (bLong ? 1.f : 0.8f)); break;
			default: bHair = V < 0.4f + Fringe || ((U < 0.13f || U > 0.87f) && V < (bLong ? 1.f : 0.72f)); break;
			}
			if (bBald)
			{
				bHair = bHair && (Direction == EOnyxDirection::Up || Direction == EOnyxDirection::UpSide) ? V > 0.55f : false;
			}
			const bool bShaded = Shade[Y * Width + X] != 0;
			FColor Color;
			if (bHair)
			{
				Color = bShaded ? HairShade : (V < 0.2f && U > 0.3f && U < 0.62f) ? HairLight : Hair;
				HairAt[(Y + Margin) * CW + X + Margin] = 1;
			}
			else
			{
				Color = bShaded ? SkinShade : Skin;
				if (bBeard && V > 0.74f && Direction != EOnyxDirection::Up && Direction != EOnyxDirection::UpSide)
				{
					Color = bShaded ? HairShade : Hair;
				}
			}
			Canvas.Set(X + Margin, Y + Margin, Color);
		}
	}
	// Hair has volume: a pixel more over the top and down the sides of the head where it is hair; long hair falls lower.
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (!In(X, Y) || !HairAt[(Y + Margin) * CW + X + Margin])
			{
				continue;
			}
			const float V = (Y + 0.5f) / Height;
			if (!In(X, Y - 1))
			{
				Canvas.Set(X + Margin, Y + Margin - 1, Hair);
			}
			if (V < 0.6f && !In(X - 1, Y))
			{
				Canvas.Set(X + Margin - 1, Y + Margin, HairShade);
			}
			if (V < 0.6f && !In(X + 1, Y))
			{
				Canvas.Set(X + Margin + 1, Y + Margin, Direction == EOnyxDirection::Side || Direction == EOnyxDirection::DownSide ? Hair : HairShade);
			}
			if (bLong && !In(X, Y + 1) && (Direction == EOnyxDirection::Up || Direction == EOnyxDirection::UpSide || ((Direction == EOnyxDirection::Side
				|| Direction == EOnyxDirection::DownSide) && (X + 0.5f) / Width < 0.45f) || (X + 0.5f) / Width < 0.2f || (X + 0.5f) / Width > 0.8f))
			{
				for (int32 Fall = 1; Fall <= 3 && Y + Margin + Fall < CH; ++Fall)
				{
					Canvas.Set(X + Margin, Y + Margin + Fall, Fall == 3 ? HairShade : Hair);
				}
			}
		}
	}
	// Eyes: two looking down, one looking to the side; a mouth straight on.
	auto Eye = [&](float U)
	{
		const int32 X = FMath::Clamp(FMath::RoundToInt(U * Width - 0.5f), 0, Width - 1);
		const int32 Y = FMath::RoundToInt(Height * 0.56f);
		for (int32 Row = Y; Row <= Y + 1; ++Row)
		{
			if (In(X, Row) && !HairAt[(Row + Margin) * CW + X + Margin])
			{
				Canvas.Set(X + Margin, Row + Margin, Eyes);
			}
		}
	};
	switch (Direction)
	{
	case EOnyxDirection::Down: Eye(0.32f); Eye(0.7f); break;
	case EOnyxDirection::DownSide: Eye(0.52f); Eye(0.84f); break;
	case EOnyxDirection::Side: Eye(0.8f); break;
	default: break;
	}
	if (Direction == EOnyxDirection::Down && !bBeard)
	{
		const int32 X = Width / 2, Y = FMath::RoundToInt(Height * 0.8f);
		if (In(X, Y)) Canvas.Set(X + Margin, Y + Margin, SkinShade);
	}
	// Hats over the hair.
	if (Hat != EOnyxHat::None)
	{
		int32 Top = Height;
		for (int32 Y = 0; Y < Height && Top == Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				if (In(X, Y)) { Top = Y; break; }
			}
		}
		const int32 Brim = Top + FMath::Max(2, Height * 3 / 10);
		if (Hat == EOnyxHat::Straw)
		{
			for (int32 Y = Top - 2; Y < Brim; ++Y)
			{
				for (int32 X = Width / 6; X < Width - Width / 6; ++X)
				{
					Canvas.Set(X + Margin, Y + Margin, Y == Brim - 1 ? HatShade : HatColor);
				}
			}
			for (int32 X = -2; X < Width + 2; ++X)
			{
				Canvas.Set(X + Margin, Brim + Margin, HatColor);
				Canvas.Set(X + Margin, Brim + 1 + Margin, HatShade);
			}
		}
		else
		{
			for (int32 Y = Top - 1; Y < Brim; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					if (In(X, FMath::Max(Y, Top)) || Y < Top)
					{
						if (X >= Width / 8 && X < Width - Width / 8) Canvas.Set(X + Margin, Y + Margin, Y == Brim - 1 ? HatShade : HatColor);
					}
				}
			}
			// The peak points the way it looks.
			const bool bRight = Direction == EOnyxDirection::Side || Direction == EOnyxDirection::DownSide;
			if (Direction != EOnyxDirection::Up && Direction != EOnyxDirection::UpSide)
			{
				for (int32 X = bRight ? Width / 2 : Width / 5; X < (bRight ? Width + 2 : Width - Width / 5); ++X)
				{
					Canvas.Set(X + Margin, Brim + Margin, HatShade);
				}
			}
		}
	}
	return Canvas;
}

FOnyxPixelCanvas FOnyxOutfit::Dress(const FOnyxPixelCanvas& Source, EOnyxDirection Direction) const
{
	using namespace OnyxOutfitHelpers;
	const int32 Width = Source.Size.X, Height = Source.Size.Y, Count = Width * Height;
	FOnyxPixelCanvas Out = Source;
	TArray<EOnyxOutfitPart> Parts;
	TArray<uint8> Body; // pixels of the figure (for the outline)
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Parts.Add(Classify(Source.Get(Index % Width, Index / Width)));
		Body.Add(0);
	}
	auto PartAt = [&](int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < Width && Y < Height ? Parts[Y * Width + X] : EOnyxOutfitPart::None; };
	auto Paint = [&](int32 X, int32 Y, const FColor& Color) { if (X >= 0 && Y >= 0 && X < Width && Y < Height) { Out.Set(X, Y, Color); Body[Y * Width + X] = 1; } };
	// Far limbs (the left ones, from the side and the diagonals) a shade darker.
	const bool bSideways = Direction == EOnyxDirection::Side || Direction == EOnyxDirection::DownSide || Direction == EOnyxDirection::UpSide;
	const bool bFront = Direction == EOnyxDirection::Down || Direction == EOnyxDirection::DownSide || Direction == EOnyxDirection::Side;

	// --- The torso: a shirt or an open jacket, a collar under the head, a belt at its foot --------------------------------
	float TorsoX = 0.f, TorsoY = 0.f;
	int32 TorsoCount = 0, TorsoMinX = Width, TorsoMaxX = -1;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (IsTorso(Parts[Index]))
		{
			TorsoX += Index % Width;
			TorsoY += Index / Width;
			TorsoMinX = FMath::Min(TorsoMinX, Index % Width);
			TorsoMaxX = FMath::Max(TorsoMaxX, Index % Width);
			++TorsoCount;
		}
	}
	if (TorsoCount > 0)
	{
		TorsoX /= TorsoCount;
		TorsoY /= TorsoCount;
		const float Middle = Direction == EOnyxDirection::DownSide ? TorsoX + (TorsoMaxX - TorsoMinX) * 0.18f : TorsoX;
		for (int32 X = 0; X < Width; ++X)
		{
			int32 Top = -1, Bottom = -1;
			for (int32 Y = 0; Y < Height; ++Y)
			{
				if (IsTorso(PartAt(X, Y)))
				{
					Top = Top < 0 ? Y : Top;
					Bottom = Y;
				}
			}
			for (int32 Y = Top; Top >= 0 && Y <= Bottom; ++Y)
			{
				const EOnyxOutfitPart Part = PartAt(X, Y);
				if (!IsTorso(Part))
				{
					continue;
				}
				FColor Color = Part == EOnyxOutfitPart::Torso ? Shirt : Part == EOnyxOutfitPart::TorsoShade ? ShirtShade : ShirtDark;
				const bool bCentre = FMath::Abs(X + 0.f - Middle) < ((TorsoMaxX - TorsoMinX) >= 8 ? 1.f : 0.6f);
				if (bOpenJacket && bFront && Direction != EOnyxDirection::Side && bCentre)
				{
					Color = ShirtInner;
				}
				if (Y == Top && IsHead(PartAt(X, Y - 1)))
				{
					Color = bFront ? ShirtInner : ShirtShade; // the collar
				}
				if (Y == Bottom && Bottom - Top >= 4)
				{
					Color = bFront && bCentre && Direction != EOnyxDirection::Side ? Buckle : Belt;
				}
				Paint(X, Y, Color);
			}
		}
	}

	// --- Arms: sleeves from the shoulder, a hand at the far end --------------------------------------------------------
	TArray<uint8> Hands;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Hands.Add(0);
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const EOnyxOutfitPart Upper = Side == 0 ? EOnyxOutfitPart::RightArmUpper : EOnyxOutfitPart::LeftArmUpper;
		const EOnyxOutfitPart Lower = Side == 0 ? EOnyxOutfitPart::RightArmLower : EOnyxOutfitPart::LeftArmLower;
		TArray<uint8> Arm;
		TArray<int32> Shoulder;
		int32 Nearest = -1;
		float NearestDistance = 1e9f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const bool bArm = Parts[Index] == Upper || Parts[Index] == Lower;
			Arm.Add(bArm ? 1 : 0);
			if (!bArm)
			{
				continue;
			}
			const int32 X = Index % Width, Y = Index / Width;
			if (IsTorso(PartAt(X - 1, Y)) || IsTorso(PartAt(X + 1, Y)) || IsTorso(PartAt(X, Y - 1)) || IsTorso(PartAt(X, Y + 1)))
			{
				Shoulder.Add(Index);
			}
			const float D = FMath::Square(X - TorsoX) + FMath::Square(Y - TorsoY);
			if (D < NearestDistance)
			{
				NearestDistance = D;
				Nearest = Index;
			}
		}
		if (Nearest < 0)
		{
			continue;
		}
		if (Shoulder.IsEmpty())
		{
			Shoulder.Add(Nearest);
		}
		const TArray<int32> Distance = Distances(Arm, Shoulder, Width, Height);
		int32 Far = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Far = FMath::Max(Far, Distance[Index]);
		}
		const bool bFar = bSideways && Side == 1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Arm[Index])
			{
				continue;
			}
			FColor Color;
			if (Far >= 2 && Distance[Index] >= Far - 1)
			{
				Color = bFar ? SkinShade : Skin;
				Hands[Index] = 1;
			}
			else
			{
				Color = (Parts[Index] == Upper && !bFar) ? Sleeve : SleeveShade;
			}
			Paint(Index % Width, Index / Width, Color);
		}
	}

	// --- Legs: trousers from the hip, a boot at the far end -----------------------------------------------------------
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const EOnyxOutfitPart Dark = Side == 0 ? EOnyxOutfitPart::RightLegDark : EOnyxOutfitPart::LeftLegDark;
		const EOnyxOutfitPart Light = Side == 0 ? EOnyxOutfitPart::RightLegLight : EOnyxOutfitPart::LeftLegLight;
		TArray<uint8> Leg;
		TArray<int32> Hip;
		int32 Highest = -1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const bool bLeg = Parts[Index] == Dark || Parts[Index] == Light;
			Leg.Add(bLeg ? 1 : 0);
			if (!bLeg)
			{
				continue;
			}
			const int32 X = Index % Width, Y = Index / Width;
			if (IsTorso(PartAt(X - 1, Y)) || IsTorso(PartAt(X + 1, Y)) || IsTorso(PartAt(X, Y - 1)) || IsTorso(PartAt(X, Y + 1)))
			{
				Hip.Add(Index);
			}
			if (Highest < 0 || Y < Highest / Width)
			{
				Highest = Index;
			}
		}
		if (Highest < 0)
		{
			continue;
		}
		if (Hip.IsEmpty())
		{
			Hip.Add(Highest);
		}
		const TArray<int32> Distance = Distances(Leg, Hip, Width, Height);
		int32 Far = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Far = FMath::Max(Far, Distance[Index]);
		}
		const bool bFar = bSideways && Side == 1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Leg[Index])
			{
				continue;
			}
			const int32 Boot = Far >= 5 ? Far - 2 : Far >= 3 ? Far - 1 : Far + 1;
			FColor Color;
			if (Distance[Index] >= Boot)
			{
				Color = (bFar || Distance[Index] == Far) ? BootsShade : Boots;
			}
			else
			{
				Color = (Parts[Index] == Light && !bFar) ? Trousers : TrousersShade;
			}
			Paint(Index % Width, Index / Width, Color);
		}
	}

	// --- The sword: steel held in a hand (a grip where it meets it); white away from the hands is the slash ------------
	{
		TArray<uint8> Steel;
		TArray<int32> Held;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const bool bSteel = Parts[Index] == EOnyxOutfitPart::Steel;
			Steel.Add(bSteel ? 1 : 0);
			if (!bSteel)
			{
				continue;
			}
			const int32 X = Index % Width, Y = Index / Width;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const int32 NX = X + DX, NY = Y + DY;
					if (NX >= 0 && NY >= 0 && NX < Width && NY < Height && Hands[NY * Width + NX])
					{
						Held.AddUnique(Index);
					}
				}
			}
		}
		if (!Held.IsEmpty())
		{
			const TArray<int32> Distance = Distances(Steel, Held, Width, Height);
			int32 Reached = 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Reached += Distance[Index] >= 0 ? 1 : 0;
			}
			const int32 Reach = Reached > 40 ? 7 : 1 << 20; // a blade against a slash: only its own length
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (Distance[Index] < 0 || Distance[Index] > Reach)
				{
					continue;
				}
				const int32 X = Index % Width, Y = Index / Width;
				const bool bEdge = !(Y + 1 < Height && Steel[(Y + 1) * Width + X]) || !(X + 1 < Width && Steel[Y * Width + X + 1]);
				Paint(X, Y, Distance[Index] == 0 ? Grip : bEdge ? BladeEdge : Blade);
			}
		}
	}

	// --- The head: hair and a face drawn for the way it looks, turned with the head when the body tumbles -------------
	{
		int32 MinX = Width, MinY = Height, MaxX = -1, MaxY = -1;
		float HeadX = 0.f, HeadY = 0.f;
		int32 HeadCount = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (IsHead(Parts[Index]))
			{
				const int32 X = Index % Width, Y = Index / Width;
				MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X);
				MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y);
				HeadX += X; HeadY += Y; ++HeadCount;
			}
		}
		if (HeadCount > 0)
		{
			HeadX /= HeadCount;
			HeadY /= HeadCount;
			// Which way is up for the head: away from the torso (quarter turns clockwise from upright).
			int32 Turns = 0;
			if (TorsoCount > 0)
			{
				const float DX = HeadX - TorsoX, DY = HeadY - TorsoY;
				Turns = FMath::Abs(DY) >= FMath::Abs(DX) ? (DY <= 0.f ? 0 : 2) : (DX > 0.f ? 1 : 3);
			}
			const int32 BoxW = MaxX - MinX + 1, BoxH = MaxY - MinY + 1;
			// Upright box: a quarter turn swaps its sides.
			const int32 UW = (Turns % 2) ? BoxH : BoxW, UH = (Turns % 2) ? BoxW : BoxH;
			// From an upright pixel (U, V) to the frame's box.
			auto ToBox = [&](int32 U, int32 V, int32 W, int32 H, int32& OutX, int32& OutY)
			{
				switch (Turns)
				{
				case 1: OutX = H - 1 - V; OutY = U; break;            // up points right
				case 2: OutX = W - 1 - U; OutY = H - 1 - V; break;    // upside down
				case 3: OutX = V; OutY = W - 1 - U; break;            // up points left
				default: OutX = U; OutY = V; break;
				}
			};
			TArray<uint8> Mask, ShadeMask;
			for (int32 V = 0; V < UH; ++V)
			{
				for (int32 U = 0; U < UW; ++U)
				{
					int32 BX = 0, BY = 0;
					ToBox(U, V, UW, UH, BX, BY);
					const EOnyxOutfitPart Part = PartAt(MinX + BX, MinY + BY);
					Mask.Add(IsHead(Part) ? 1 : 0);
					ShadeMask.Add(Part == EOnyxOutfitPart::HeadShade ? 1 : 0);
				}
			}
			// A tumbling head is seen from the side; upright it looks the frame's way.
			const EOnyxDirection Looking = Turns == 0 ? Direction : (Direction == EOnyxDirection::Up || Direction == EOnyxDirection::UpSide ? EOnyxDirection::Up : EOnyxDirection::Side);
			constexpr int32 Margin = 3;
			const FOnyxPixelCanvas Drawn = DrawHead(Mask, ShadeMask, UW, UH, Margin, Looking);
			for (int32 V = 0; V < Drawn.Size.Y; ++V)
			{
				for (int32 U = 0; U < Drawn.Size.X; ++U)
				{
					const FColor Color = Drawn.Get(U, V);
					if (Color.A == 0)
					{
						continue;
					}
					int32 BX = 0, BY = 0;
					ToBox(U, V, Drawn.Size.X, Drawn.Size.Y, BX, BY);
					Paint(MinX - Margin + BX, MinY - Margin + BY, Color);
				}
			}
		}
	}

	// --- A dark outline round the figure (over the ground's shadow, never over a slash) ---------------------------------
	TArray<uint8> Edge;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Edge.Add(0);
	}
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (Body[Y * Width + X])
			{
				continue;
			}
			const FColor Here = Out.Get(X, Y);
			const bool bOpen = Here.A == 0 || Classify(Here) == EOnyxOutfitPart::None;
			if (!bOpen || (Here.A > 0 && Here.R > 200 && Here.G > 200 && Here.B > 200))
			{
				continue; // a slash
			}
			const bool bNext = (X > 0 && Body[Y * Width + X - 1]) || (X + 1 < Width && Body[Y * Width + X + 1])
				|| (Y > 0 && Body[(Y - 1) * Width + X]) || (Y + 1 < Height && Body[(Y + 1) * Width + X]);
			Edge[Y * Width + X] = bNext ? 1 : 0;
		}
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Edge[Index])
		{
			Out.Set(Index % Width, Index / Width, OutlineColor);
		}
	}
	return Out;
}

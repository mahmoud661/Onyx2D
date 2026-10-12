// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxMaterialBuilder.h"
#include "Builders/OnyxAssetUtils.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MaterialEditingLibrary.h"
#include "MaterialExpressionSpriteTextureSampler.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionViewSize.h"
#include "Materials/MaterialParameterCollection.h"

const TCHAR* FOnyxMaterialBuilder::Folder = TEXT("/Onyx2D/Materials");

namespace
{
	const TCHAR* MaterialNames[] = { TEXT("M_OnyxSolid"), TEXT("M_OnyxBackground"), TEXT("M_OnyxGlow"), TEXT("M_OnyxLitSprite"),
		TEXT("M_OnyxLightHalo"), TEXT("PP_OnyxLight2D"), TEXT("M_OnyxWater"), TEXT("M_OnyxWaterDrop") };
	const TCHAR* CollectionName = TEXT("MPC_OnyxLighting");
	const TCHAR* PlaceholderName = TEXT("RT_OnyxLightPlaceholder");

	/**
	 * 2D lighting composite: lights every lit sprite from the 2D light buffer, the global light and the 2D sun,
	 * with normal-map relief. Unlit pixels (sky, glows) have no base colour in the G-buffer and pass through.
	 */
	const TCHAR* CompositeHLSL = TEXT(R"HLSL(
float aspect = Aspect.x; // view width / height
// Soft shadows: Gaussian-weighted taps over the light buffers (they are half resolution and smooth already).
float3 light = 0;
float3 position = 0;
float total = 0;
for (int y = -2; y <= 2; y++)
{
    for (int x = -2; x <= 2; x++)
    {
        float w = exp(-(x * x + y * y) / 4.0);
        float2 offset = float2(x / aspect, y) * Softness.x;
        light += Texture2DSample(LightTex, LightTexSampler, UV + offset).rgb * w;
        position += Texture2DSample(PositionTex, PositionTexSampler, UV + offset).rgb * w;
        total += w;
    }
}
light /= total;
position /= total;

float3 n = normalize(float3(Normal.x, Normal.z, Normal.y)); // screen space: right, up, towards the camera
float3 sunDir = normalize(SunDirection.xyz);
float sunTerm = lerp(1.0, min(saturate(dot(n, sunDir)) / max(sunDir.z, 0.05), 2.5), NormalStrength);

float weight = dot(light, float3(0.2126, 0.7152, 0.0722));
float lightTerm = 1.0;
if (weight > 1e-4)
{
    float3 pos = position / weight;                // weighted average light: screen UV (+1) and height
    float2 toLight = pos.xy - 1.0 - UV;
    toLight.x *= aspect;
    toLight.y = -toLight.y;                        // screen V runs down, light maths runs up
    float3 l = normalize(float3(toLight, max(pos.z, 0.005)));
    lightTerm = lerp(1.0, min(saturate(dot(n, l)) / max(l.z, 0.05), 2.5), NormalStrength);
}
float3 lit = Ambient.rgb + SunColor.rgb * sunTerm + light * lightTerm;
return Scene.rgb + Base.rgb * lit;
)HLSL");

	/**
	 * Onyx Water: UV.x = world X in metres, UV.y = cm under the surface (negative just above it). Smooth gradient with depth,
	 * drifting caustics and light shafts near the top, rising bubbles, and a crisp surface line with a glow under it.
	 * The surface edge is anti-aliased in the shader (fwidth), so it stays smooth without TAA.
	 */
	const TCHAR* WaterHLSL = TEXT(R"HLSL(
float d = UV.y;
float aa = max(fwidth(d), 0.0001) * 1.5;
float inside = saturate(d / aa + 0.5);
float depth01 = saturate(d / max(Depth, 1.0));
float3 col = lerp(Shallow.rgb, Deep.rgb, sqrt(depth01));
float alpha = lerp(Shallow.a, Deep.a, depth01);
float t = Time;

// Caustics: one soft, slowly warping net of light about a metre across, only near the top.
float2 p = float2(UV.x * 1.1, d * 0.011);
p += 0.25 * float2(sin(p.y * 4.0 + t * 0.8), sin(p.x * 3.3 - t * 0.6));
float wa = sin(p.x * 6.2831 + t * 0.5);
float wb = sin(p.y * 5.6548 - t * 0.35 + wa * 0.8);
float caustic = pow(saturate(1.0 - abs(wa + wb) * 0.5), 6.0);
col += Surface.rgb * caustic * Caustics * 0.55 * exp(-max(d, 0.0) / 70.0);

// Light shafts falling from the surface.
float shaft = pow(saturate(0.5 + 0.5 * sin(UV.x * 1.3 + t * 0.12) * sin(UV.x * 0.47 - t * 0.05)), 5.0);
col += Surface.rgb * shaft * 0.16 * Caustics * exp(-max(d, 0.0) / 150.0);

// A few bubbles rising: thin bright rings with a highlight, between the bottom and just under the surface.
float2 bp = float2(UV.x * 1.6, d * 0.022 + t * 0.18);
float2 cell = floor(bp);
float h = frac(sin(dot(cell, float2(127.1, 311.7))) * 43758.5453);
float2 o = float2(frac(h * 3.7), frac(h * 17.13)) * 0.5 + 0.25;
float r = 0.07 + 0.08 * frac(h * 7.7);
float dist = length((frac(bp) - o) * float2(1.0, 0.8));
float w = max(length(fwidth(bp)), 0.002) * 0.7; // from the continuous coordinates: no seams at cell edges
float ring = saturate(1.0 - abs(dist - r) / w) + 0.6 * saturate(1.0 - length(frac(bp) - o + r * 0.45) / (r * 0.35));
float bub = saturate(ring) * step(0.9, h) * saturate((d - 15.0) / 30.0) * saturate((Depth - d) / 30.0) * Bubbles;
col = lerp(col, Surface.rgb, bub * 0.6);
alpha = max(alpha, bub * 0.55);

float lineMask = saturate(1.0 - abs(d - Thickness * 0.5) / (Thickness * 0.5 + aa));
float glow = exp(-max(d, 0.0) / 12.0) * 0.4;
col = lerp(col, Surface.rgb, saturate(lineMask + glow * (1.0 - lineMask)) * Surface.a);
alpha = max(alpha, lineMask * Surface.a);

alpha *= inside;
return float4(col * Tint.rgb, alpha);
)HLSL");

	/** Splash droplets: round, soft-edged dots in their vertex colour. */
	const TCHAR* DropHLSL = TEXT(R"HLSL(
float r = length(UV - 0.5) * 2.0;
float edge = max(fwidth(r), 0.001);
float a = saturate((1.0 - r) / edge);
return float4(ColorRGB * Tint.rgb, ColorA * a);
)HLSL");

	/** Small graph-building helper over UMaterialEditingLibrary. */
	struct FGraph
	{
		UMaterial* Material;

		template <class T>
		T* Add(int32 X, int32 Y)
		{
			return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass(), X, Y));
		}

		UMaterialExpressionScalarParameter* Scalar(FName Name, float Value, int32 X, int32 Y)
		{
			UMaterialExpressionScalarParameter* Node = Add<UMaterialExpressionScalarParameter>(X, Y);
			Node->ParameterName = Name;
			Node->DefaultValue = Value;
			return Node;
		}

		UMaterialExpressionVectorParameter* Vector(FName Name, const FLinearColor& Value, int32 X, int32 Y)
		{
			UMaterialExpressionVectorParameter* Node = Add<UMaterialExpressionVectorParameter>(X, Y);
			Node->ParameterName = Name;
			Node->DefaultValue = Value;
			return Node;
		}

		void Link(UMaterialExpression* From, const FString& Output, UMaterialExpression* To, const FString& Input) const
		{
			UMaterialEditingLibrary::ConnectMaterialExpressions(From, Output, To, Input);
		}

		/** Some channels of From. Every channel is set: a new mask doesn't start with all of them off (R is on). */
		UMaterialExpressionComponentMask* Mask(UMaterialExpression* From, bool bR, bool bG, bool bB, bool bA, int32 X, int32 Y)
		{
			UMaterialExpressionComponentMask* Node = Add<UMaterialExpressionComponentMask>(X, Y);
			Node->R = bR;
			Node->G = bG;
			Node->B = bB;
			Node->A = bA;
			Link(From, FString(), Node, FString());
			return Node;
		}

		void Out(UMaterialExpression* From, const FString& Output, EMaterialProperty Property) const
		{
			UMaterialEditingLibrary::ConnectMaterialProperty(From, Output, Property);
		}
	};

	/** A fresh (or emptied, when rebuilding) material, or null when it exists and isn't to be rebuilt. */
	UMaterial* BeginMaterial(const TCHAR* Name, bool bRebuild)
	{
		const FString Path = FString(FOnyxMaterialBuilder::Folder) / Name;
		UMaterial* Existing = LoadObject<UMaterial>(nullptr, *(Path + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Existing && !bRebuild)
		{
			return nullptr;
		}
		UMaterial* Material = Existing ? Existing : FOnyxAssetUtils::CreateOrReuse<UMaterial>(FOnyxMaterialBuilder::Folder, Name);
		if (Material)
		{
			Material->Modify();
			UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
		}
		return Material;
	}

	void FinishMaterial(UMaterial* Material, TArray<FString>& Built)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
		Material->MarkPackageDirty();
		FOnyxAssetUtils::SavePackages({ Material->GetPackage() });
		Built.Add(Material->GetName());
	}

	template <class T>
	T* LoadPluginAsset(const TCHAR* Name)
	{
		const FString Path = FString(FOnyxMaterialBuilder::Folder) / Name;
		return LoadObject<T>(nullptr, *(Path + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UMaterialParameterCollection* EnsureCollection(bool bRebuild, TArray<FString>& Built)
	{
		UMaterialParameterCollection* Collection = LoadPluginAsset<UMaterialParameterCollection>(CollectionName);
		if (Collection && !bRebuild)
		{
			return Collection;
		}
		Collection = Collection ? Collection : FOnyxAssetUtils::CreateOrReuse<UMaterialParameterCollection>(FOnyxMaterialBuilder::Folder, CollectionName);
		Collection->Modify();
		Collection->VectorParameters.Reset();
		FCollectionVectorParameter Tint;
		Tint.ParameterName = TEXT("BackgroundTint"); // the parallax tint of the current lighting mood
		Tint.DefaultValue = FLinearColor::White;
		Collection->VectorParameters.Add(Tint);
		Collection->PostEditChange();
		Collection->MarkPackageDirty();
		FOnyxAssetUtils::SavePackages({ Collection->GetPackage() });
		Built.Add(CollectionName);
		return Collection;
	}

	UTextureRenderTarget2D* EnsurePlaceholder(bool bRebuild, TArray<FString>& Built)
	{
		UTextureRenderTarget2D* Placeholder = LoadPluginAsset<UTextureRenderTarget2D>(PlaceholderName);
		if (Placeholder && !bRebuild)
		{
			return Placeholder;
		}
		// A linear-colour sampler needs a linear texture as its default: a tiny float target, black.
		Placeholder = Placeholder ? Placeholder : FOnyxAssetUtils::CreateOrReuse<UTextureRenderTarget2D>(FOnyxMaterialBuilder::Folder, PlaceholderName);
		Placeholder->Modify();
		Placeholder->RenderTargetFormat = RTF_RGBA16f;
		Placeholder->ClearColor = FLinearColor::Black;
		Placeholder->InitAutoFormat(4, 4);
		Placeholder->PostEditChange();
		Placeholder->MarkPackageDirty();
		FOnyxAssetUtils::SavePackages({ Placeholder->GetPackage() });
		Built.Add(PlaceholderName);
		return Placeholder;
	}

	void BuildSolid(bool bRebuild, TArray<FString>& Built)
	{
		// Lit solid colour: placeholders, level builder blocks, hazards.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxSolid"), bRebuild);
		if (!Material)
		{
			return;
		}
		FGraph Graph{ Material };
		Graph.Out(Graph.Vector(TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f), -400, -100), FString(), MP_BaseColor);
		Graph.Out(Graph.Scalar(TEXT("Roughness"), 0.85f, -400, 150), FString(), MP_Roughness);
		Material->bUsedWithInstancedStaticMeshes = true;
		FinishMaterial(Material, Built);
	}

	void BuildBackground(bool bRebuild, UMaterialParameterCollection* Collection, TArray<FString>& Built)
	{
		// Unlit, masked, tiling texture for parallax layers, tinted by the lighting mood.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxBackground"), bRebuild);
		if (!Material)
		{
			return;
		}
		Material->SetShadingModel(MSM_Unlit);
		Material->BlendMode = BLEND_Masked;
		Material->TwoSided = true;
		FGraph Graph{ Material };
		UMaterialExpressionTextureCoordinate* UV = Graph.Add<UMaterialExpressionTextureCoordinate>(-1100, 0);
		UMaterialExpressionAppendVector* Tiling = Graph.Add<UMaterialExpressionAppendVector>(-1100, 150);
		Graph.Link(Graph.Scalar(TEXT("TilingX"), 1.f, -1350, 120), FString(), Tiling, TEXT("A"));
		Graph.Link(Graph.Scalar(TEXT("TilingY"), 1.f, -1350, 220), FString(), Tiling, TEXT("B"));
		UMaterialExpressionMultiply* Scaled = Graph.Add<UMaterialExpressionMultiply>(-850, 50);
		Graph.Link(UV, FString(), Scaled, TEXT("A"));
		Graph.Link(Tiling, FString(), Scaled, TEXT("B"));

		UMaterialExpressionTextureSampleParameter2D* Texture = Graph.Add<UMaterialExpressionTextureSampleParameter2D>(-600, 0);
		Texture->ParameterName = TEXT("Texture");
		Texture->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		Texture->SamplerType = SAMPLERTYPE_Color;
		Graph.Link(Scaled, FString(), Texture, TEXT("UVs"));

		UMaterialExpressionMultiply* Color = Graph.Add<UMaterialExpressionMultiply>(-300, 0);
		Graph.Link(Texture, TEXT("RGB"), Color, TEXT("A"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor::White, -600, 250), FString(), Color, TEXT("B"));

		UMaterialExpressionCollectionParameter* Mood = Graph.Add<UMaterialExpressionCollectionParameter>(-300, 250);
		Mood->Collection = Collection;
		Mood->ParameterName = TEXT("BackgroundTint");
		if (Collection && Collection->VectorParameters.Num() > 0)
		{
			Mood->ParameterId = Collection->VectorParameters[0].Id;
		}
		UMaterialExpressionComponentMask* MoodRGB = Graph.Mask(Mood, true, true, true, false, -150, 250);
		UMaterialExpressionMultiply* Tinted = Graph.Add<UMaterialExpressionMultiply>(-100, 50);
		Graph.Link(Color, FString(), Tinted, TEXT("A"));
		Graph.Link(MoodRGB, FString(), Tinted, TEXT("B"));
		Graph.Out(Tinted, FString(), MP_EmissiveColor);
		Graph.Out(Texture, TEXT("A"), MP_OpacityMask);
		FinishMaterial(Material, Built);
	}

	void BuildGlow(bool bRebuild, TArray<FString>& Built)
	{
		// Unlit glowing colour: pickups and effects that must read in the dark.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxGlow"), bRebuild);
		if (!Material)
		{
			return;
		}
		Material->SetShadingModel(MSM_Unlit);
		FGraph Graph{ Material };
		UMaterialExpressionMultiply* Glow = Graph.Add<UMaterialExpressionMultiply>(-250, 0);
		Graph.Link(Graph.Vector(TEXT("Color"), FLinearColor(1.f, 0.75f, 0.15f), -500, -50), FString(), Glow, TEXT("A"));
		Graph.Link(Graph.Scalar(TEXT("Intensity"), 1.5f, -500, 150), FString(), Glow, TEXT("B"));
		Graph.Out(Glow, FString(), MP_EmissiveColor);
		FinishMaterial(Material, Built);
	}

	/** True when a plugin material exists but predates a parameter (rebuild it). */
	bool IsOutdated(const TCHAR* Name, FName Parameter)
	{
		const UMaterial* Existing = LoadPluginAsset<UMaterial>(Name);
		float Value = 0.f;
		return Existing && !Existing->GetScalarParameterDefaultValue(FHashedMaterialParameterInfo(Parameter), Value);
	}

	/** Unlit translucent, drawn after the 2D lighting composite (which runs before depth of field) so it is not lit twice. */
	void SetupWaterSurface(UMaterial* Material)
	{
		Material->SetShadingModel(MSM_Unlit);
		Material->BlendMode = BLEND_Translucent;
		Material->TwoSided = true;
		Material->TranslucencyPass = MTP_AfterDOF;
	}

	UMaterialExpressionCustom* AddCustom(FGraph& Graph, const TCHAR* Code, const TCHAR* Description, std::initializer_list<const TCHAR*> Inputs)
	{
		UMaterialExpressionCustom* Custom = Graph.Add<UMaterialExpressionCustom>(-500, 0);
		Custom->Code = Code;
		Custom->OutputType = CMOT_Float4;
		Custom->Description = Description;
		Custom->Inputs.Reset();
		for (const TCHAR* InputName : Inputs)
		{
			Custom->Inputs.AddDefaulted_GetRef().InputName = InputName;
		}
		Custom->PostEditChange();
		return Custom;
	}

	void OutputColorAndOpacity(FGraph& Graph, UMaterialExpressionCustom* Custom)
	{
		Graph.Out(Graph.Mask(Custom, true, true, true, false, -250, -50), FString(), MP_EmissiveColor);
		Graph.Out(Graph.Mask(Custom, false, false, false, true, -250, 100), FString(), MP_Opacity);
	}

	void BuildWater(bool bRebuild, TArray<FString>& Built)
	{
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxWater"), bRebuild || IsOutdated(TEXT("M_OnyxWater"), TEXT("WaterVersion5")));
		if (!Material)
		{
			return;
		}
		SetupWaterSurface(Material);
		FGraph Graph{ Material };
		UMaterialExpressionCustom* Custom = AddCustom(Graph, WaterHLSL, TEXT("Onyx Water"),
			{ TEXT("UV"), TEXT("Time"), TEXT("Shallow"), TEXT("Deep"), TEXT("Surface"), TEXT("Depth"), TEXT("Thickness"), TEXT("Caustics"), TEXT("Bubbles"), TEXT("Tint"), TEXT("Version") });
		Graph.Link(Graph.Add<UMaterialExpressionTextureCoordinate>(-900, -400), FString(), Custom, TEXT("UV"));
		Graph.Link(Graph.Add<UMaterialExpressionTime>(-900, -300), FString(), Custom, TEXT("Time"));
		Graph.Link(Graph.Vector(TEXT("ShallowColor"), FLinearColor(0.12f, 0.6f, 0.86f, 0.5f), -900, -200), TEXT("RGBA"), Custom, TEXT("Shallow"));
		Graph.Link(Graph.Vector(TEXT("DeepColor"), FLinearColor(0.02f, 0.14f, 0.4f, 0.88f), -900, -50), TEXT("RGBA"), Custom, TEXT("Deep"));
		Graph.Link(Graph.Vector(TEXT("SurfaceColor"), FLinearColor(0.85f, 0.97f, 1.f, 1.f), -900, 100), TEXT("RGBA"), Custom, TEXT("Surface"));
		Graph.Link(Graph.Scalar(TEXT("Depth"), 300.f, -900, 250), FString(), Custom, TEXT("Depth"));
		Graph.Link(Graph.Scalar(TEXT("Thickness"), 3.f, -900, 350), FString(), Custom, TEXT("Thickness"));
		Graph.Link(Graph.Scalar(TEXT("Caustics"), 0.6f, -900, 450), FString(), Custom, TEXT("Caustics"));
		Graph.Link(Graph.Scalar(TEXT("Bubbles"), 1.f, -900, 550), FString(), Custom, TEXT("Bubbles"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor::White, -900, 650), FString(), Custom, TEXT("Tint"));
		Graph.Link(Graph.Scalar(TEXT("WaterVersion5"), 1.f, -900, 800), FString(), Custom, TEXT("Version")); // marks this version (older ones are rebuilt)
		OutputColorAndOpacity(Graph, Custom);
		FinishMaterial(Material, Built);
	}

	void BuildWaterDrop(bool bRebuild, TArray<FString>& Built)
	{
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxWaterDrop"), bRebuild);
		if (!Material)
		{
			return;
		}
		SetupWaterSurface(Material);
		FGraph Graph{ Material };
		UMaterialExpressionCustom* Custom = AddCustom(Graph, DropHLSL, TEXT("Onyx Water Drop"), { TEXT("UV"), TEXT("ColorRGB"), TEXT("ColorA"), TEXT("Tint") });
		Graph.Link(Graph.Add<UMaterialExpressionTextureCoordinate>(-900, -200), FString(), Custom, TEXT("UV"));
		UMaterialExpressionVertexColor* Vertex = Graph.Add<UMaterialExpressionVertexColor>(-900, -50);
		Graph.Link(Vertex, FString(), Custom, TEXT("ColorRGB"));
		Graph.Link(Vertex, TEXT("A"), Custom, TEXT("ColorA"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor::White, -900, 150), FString(), Custom, TEXT("Tint"));
		OutputColorAndOpacity(Graph, Custom);
		FinishMaterial(Material, Built);
	}

	void BuildLitSprite(bool bRebuild, TArray<FString>& Built)
	{
		// Lit sprite with normal map. Paper2D swaps "SpriteTexture" for the sprite's sheet and
		// "SpriteAdditionalTexture_0" for its Additional Textures slot 0 (the normal map).
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxLitSprite"), bRebuild);
		if (!Material)
		{
			return;
		}
		Material->BlendMode = BLEND_Masked;
		Material->TwoSided = true; // sprites turn around (yaw 180) when facing left
		Material->OpacityMaskClipValue = 0.5f;
		FGraph Graph{ Material };

		UMaterialExpressionSpriteTextureSampler* Base = Graph.Add<UMaterialExpressionSpriteTextureSampler>(-900, -200);
		Base->ParameterName = TEXT("SpriteTexture");
		Base->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		Base->SamplerType = SAMPLERTYPE_Color;

		UMaterialExpressionVertexColor* Vertex = Graph.Add<UMaterialExpressionVertexColor>(-900, 50); // sprite tint
		UMaterialExpressionMultiply* Color = Graph.Add<UMaterialExpressionMultiply>(-550, -200);
		Graph.Link(Base, TEXT("RGB"), Color, TEXT("A"));
		Graph.Link(Vertex, FString(), Color, TEXT("B"));
		Graph.Out(Color, FString(), MP_BaseColor);

		UMaterialExpressionMultiply* Alpha = Graph.Add<UMaterialExpressionMultiply>(-550, 0);
		Graph.Link(Base, TEXT("A"), Alpha, TEXT("A"));
		Graph.Link(Vertex, TEXT("A"), Alpha, TEXT("B"));
		Graph.Out(Alpha, FString(), MP_OpacityMask);

		UMaterialExpressionSpriteTextureSampler* Normal = Graph.Add<UMaterialExpressionSpriteTextureSampler>(-900, 300);
		Normal->bSampleAdditionalTextures = true;
		Normal->AdditionalSlotIndex = 0;
		Normal->ParameterName = FName(TEXT("SpriteAdditionalTexture"), 1); // Paper2D's name for slot 0
		Normal->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
		Normal->SamplerType = SAMPLERTYPE_Normal;

		UMaterialExpressionConstant3Vector* Flat = Graph.Add<UMaterialExpressionConstant3Vector>(-900, 600);
		Flat->Constant = FLinearColor(0.f, 0.f, 1.f);
		UMaterialExpressionLinearInterpolate* Strength = Graph.Add<UMaterialExpressionLinearInterpolate>(-550, 350);
		Graph.Link(Flat, FString(), Strength, TEXT("A"));
		Graph.Link(Normal, TEXT("RGB"), Strength, TEXT("B"));
		Graph.Link(Graph.Scalar(TEXT("NormalStrength"), 1.f, -900, 750), FString(), Strength, TEXT("Alpha"));
		Graph.Out(Strength, FString(), MP_Normal);
		Graph.Out(Graph.Scalar(TEXT("Roughness"), 0.8f, -550, 550), FString(), MP_Roughness);
		Graph.Out(Graph.Scalar(TEXT("Specular"), 0.3f, -550, 650), FString(), MP_Specular);
		FinishMaterial(Material, Built);
	}

	void BuildLightHalo(bool bRebuild, TArray<FString>& Built)
	{
		// Additive radial glow around 2D lights: reads as light in the air.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxLightHalo"), bRebuild);
		if (!Material)
		{
			return;
		}
		Material->SetShadingModel(MSM_Unlit);
		Material->BlendMode = BLEND_Additive;
		Material->TwoSided = true;
		FGraph Graph{ Material };
		UMaterialExpressionTextureCoordinate* UV = Graph.Add<UMaterialExpressionTextureCoordinate>(-1300, 0);
		UMaterialExpressionConstant2Vector* Center = Graph.Add<UMaterialExpressionConstant2Vector>(-1300, 150);
		Center->R = 0.5f;
		Center->G = 0.5f;
		UMaterialExpressionSubtract* Offset = Graph.Add<UMaterialExpressionSubtract>(-1100, 50);
		Graph.Link(UV, FString(), Offset, TEXT("A"));
		Graph.Link(Center, FString(), Offset, TEXT("B"));
		UMaterialExpressionLength* Length = Graph.Add<UMaterialExpressionLength>(-950, 50);
		Graph.Link(Offset, FString(), Length, FString());
		UMaterialExpressionMultiply* Doubled = Graph.Add<UMaterialExpressionMultiply>(-800, 50);
		Doubled->ConstB = 2.f;
		Graph.Link(Length, FString(), Doubled, TEXT("A"));
		UMaterialExpressionOneMinus* Inverted = Graph.Add<UMaterialExpressionOneMinus>(-650, 50);
		Graph.Link(Doubled, FString(), Inverted, FString());
		UMaterialExpressionSaturate* Clamped = Graph.Add<UMaterialExpressionSaturate>(-500, 50);
		Graph.Link(Inverted, FString(), Clamped, FString());
		UMaterialExpressionPower* Falloff = Graph.Add<UMaterialExpressionPower>(-350, 50);
		Graph.Link(Clamped, FString(), Falloff, TEXT("Base"));
		Graph.Link(Graph.Scalar(TEXT("Falloff"), 2.5f, -500, 200), FString(), Falloff, TEXT("Exp"));
		UMaterialExpressionMultiply* Tinted = Graph.Add<UMaterialExpressionMultiply>(-150, 100);
		Graph.Link(Falloff, FString(), Tinted, TEXT("A"));
		Graph.Link(Graph.Vector(TEXT("Color"), FLinearColor(1.f, 0.7f, 0.4f), -350, 250), FString(), Tinted, TEXT("B"));
		UMaterialExpressionMultiply* Glow = Graph.Add<UMaterialExpressionMultiply>(0, 100);
		Graph.Link(Tinted, FString(), Glow, TEXT("A"));
		Graph.Link(Graph.Scalar(TEXT("Intensity"), 0.6f, -150, 300), FString(), Glow, TEXT("B"));
		Graph.Out(Glow, FString(), MP_EmissiveColor);
		FinishMaterial(Material, Built);
	}

	void BuildComposite(bool bRebuild, UTextureRenderTarget2D* Placeholder, TArray<FString>& Built)
	{
		UMaterial* Material = BeginMaterial(TEXT("PP_OnyxLight2D"), bRebuild);
		if (!Material)
		{
			return;
		}
		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = BL_SceneColorBeforeDOF;
		FGraph Graph{ Material };

		auto SceneTexture = [&Graph](ESceneTextureId Id, int32 X, int32 Y)
		{
			UMaterialExpressionSceneTexture* Node = Graph.Add<UMaterialExpressionSceneTexture>(X, Y);
			Node->SceneTextureId = Id;
			return Node;
		};
		auto Buffer = [&Graph, Placeholder](FName Name, int32 X, int32 Y)
		{
			UMaterialExpressionTextureObjectParameter* Node = Graph.Add<UMaterialExpressionTextureObjectParameter>(X, Y);
			Node->ParameterName = Name;
			Node->Texture = Placeholder;
			Node->SamplerType = SAMPLERTYPE_LinearColor;
			return Node;
		};

		UMaterialExpressionScreenPosition* Screen = Graph.Add<UMaterialExpressionScreenPosition>(-1400, 0);
		UMaterialExpressionViewSize* ViewSize = Graph.Add<UMaterialExpressionViewSize>(-1400, 900);
		UMaterialExpressionComponentMask* MaskX = Graph.Mask(ViewSize, true, false, false, false, -1250, 850);
		UMaterialExpressionComponentMask* MaskY = Graph.Mask(ViewSize, false, true, false, false, -1250, 950);
		UMaterialExpressionDivide* Aspect = Graph.Add<UMaterialExpressionDivide>(-1100, 900);
		Graph.Link(MaskX, FString(), Aspect, TEXT("A"));
		Graph.Link(MaskY, FString(), Aspect, TEXT("B"));

		UMaterialExpressionCustom* Custom = Graph.Add<UMaterialExpressionCustom>(-500, 200);
		Custom->Code = CompositeHLSL;
		Custom->OutputType = CMOT_Float3;
		Custom->Description = TEXT("Onyx 2D Lighting");
		Custom->Inputs.Reset();
		for (const TCHAR* InputName : { TEXT("Scene"), TEXT("Base"), TEXT("Normal"), TEXT("UV"), TEXT("LightTex"), TEXT("PositionTex"),
			TEXT("Ambient"), TEXT("SunColor"), TEXT("SunDirection"), TEXT("NormalStrength"), TEXT("Aspect"), TEXT("Softness") })
		{
			FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
			Input.InputName = InputName;
		}
		Custom->PostEditChange(); // rebuilds the input pins from Inputs

		Graph.Link(SceneTexture(PPI_PostProcessInput0, -1000, -300), TEXT("Color"), Custom, TEXT("Scene"));
		Graph.Link(SceneTexture(PPI_BaseColor, -1000, -150), TEXT("Color"), Custom, TEXT("Base"));
		Graph.Link(SceneTexture(PPI_WorldNormal, -1000, 0), TEXT("Color"), Custom, TEXT("Normal"));
		Graph.Link(Screen, TEXT("ViewportUV"), Custom, TEXT("UV"));
		Graph.Link(Buffer(TEXT("LightBuffer"), -1000, 150), FString(), Custom, TEXT("LightTex"));
		Graph.Link(Buffer(TEXT("PositionBuffer"), -1000, 400), FString(), Custom, TEXT("PositionTex"));
		Graph.Link(Graph.Scalar(TEXT("Softness"), 0.002f, -800, 1200), FString(), Custom, TEXT("Softness"));
		Graph.Link(Graph.Vector(TEXT("Ambient"), FLinearColor(0.75f, 0.75f, 0.75f), -800, 600), FString(), Custom, TEXT("Ambient"));
		Graph.Link(Graph.Vector(TEXT("SunColor"), FLinearColor(0.3f, 0.3f, 0.3f), -800, 750), FString(), Custom, TEXT("SunColor"));
		Graph.Link(Graph.Vector(TEXT("SunDirection"), FLinearColor(-0.5f, 0.86f, 0.6f, 0.f), -800, 900), FString(), Custom, TEXT("SunDirection"));
		Graph.Link(Graph.Scalar(TEXT("NormalStrength"), 0.8f, -800, 1050), FString(), Custom, TEXT("NormalStrength"));
		Graph.Link(Aspect, FString(), Custom, TEXT("Aspect"));
		Graph.Out(Custom, FString(), MP_EmissiveColor);
		FinishMaterial(Material, Built);
	}
}

bool FOnyxMaterialBuilder::AreMaterialsPresent()
{
	for (const TCHAR* Name : MaterialNames)
	{
		if (!LoadPluginAsset<UMaterial>(Name))
		{
			return false;
		}
	}
	return LoadPluginAsset<UMaterialParameterCollection>(CollectionName) && LoadPluginAsset<UTextureRenderTarget2D>(PlaceholderName);
}

TArray<FString> FOnyxMaterialBuilder::EnsureMaterials(bool bRebuild)
{
	TArray<FString> Built;
	UMaterialParameterCollection* Collection = EnsureCollection(bRebuild, Built);
	UTextureRenderTarget2D* Placeholder = EnsurePlaceholder(bRebuild, Built);
	BuildSolid(bRebuild, Built);
	BuildBackground(bRebuild, Collection, Built);
	BuildGlow(bRebuild, Built);
	BuildLitSprite(bRebuild, Built);
	BuildWater(bRebuild, Built);
	BuildWaterDrop(bRebuild, Built);
	BuildLightHalo(bRebuild, Built);
	BuildComposite(bRebuild, Placeholder, Built);
	return Built;
}

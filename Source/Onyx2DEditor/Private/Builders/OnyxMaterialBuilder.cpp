// Copyright Onyx2D. All Rights Reserved.

#include "Builders/OnyxMaterialBuilder.h"
#include "Builders/OnyxAssetUtils.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MaterialEditingLibrary.h"
#include "MaterialExpressionSpriteTextureSampler.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Core/Onyx2DTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionIsOrthographic.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMin.h"
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
#include "MaterialShared.h"
#include "RHIShaderPlatform.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnyxMaterials, Log, All);

const TCHAR* FOnyxMaterialBuilder::Folder = TEXT("/Onyx2D/Materials");

namespace
{
	const TCHAR* MaterialNames[] = { TEXT("M_OnyxSolid"), TEXT("M_OnyxBackground"), TEXT("M_OnyxGlow"), TEXT("M_OnyxLitSprite"),
		TEXT("M_OnyxLightHalo"), TEXT("PP_OnyxLight2D"), TEXT("M_OnyxWater"), TEXT("M_OnyxWaterDrop"), TEXT("M_OnyxRain"), TEXT("M_OnyxFog"),
		TEXT("M_OnyxAfterimage"), TEXT("M_OnyxEffectSprite"), TEXT("M_OnyxMirror"), TEXT("M_OnyxFogVolume") };
	const TCHAR* CollectionName = TEXT("MPC_OnyxLighting");
	const TCHAR* PlaceholderName = TEXT("RT_OnyxLightPlaceholder");

	/**
	 * 2D lighting composite: lights every lit sprite from the 2D light buffer, the global light and the 2D sun,
	 * with normal-map relief. Unlit pixels (sky, glows) have no base colour in the G-buffer and pass through.
	 * Bright light rolls every lit sprite off towards full brightness instead of clipping it to white. Guarded sprites (Light
	 * Guard, in the G-buffer's metallic, unused in 2D) also have their own glows taken back off and take other light a little
	 * less: paint stays paint under a lantern.
	 */
	const TCHAR* CompositeHLSL = TEXT(R"HLSL(
float aspect = Aspect.x; // view width / height
// Soft shadows: Gaussian-weighted taps over the light buffers (they are half resolution and smooth already).
float3 light = 0;
float3 position = 0;
float3 own = 0;
float total = 0;
for (int y = -2; y <= 2; y++)
{
    for (int x = -2; x <= 2; x++)
    {
        float w = exp(-(x * x + y * y) / 4.0);
        float2 offset = float2(x / aspect, y) * Softness.x;
        light += Texture2DSample(LightTex, LightTexSampler, UV + offset).rgb * w;
        position += Texture2DSample(PositionTex, PositionTexSampler, UV + offset).rgb * w;
        own += Texture2DSample(OwnGlowTex, OwnGlowTexSampler, UV + offset).rgb * w;
        total += w;
    }
}
light /= total;
position /= total;
own /= total;
// Glow in the air round Pixel Lights: on top of everything as it is, lighting nothing.
float3 halo = Texture2DSample(HaloTex, HaloTexSampler, UV).rgb * HaloStrength;

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
float guard = saturate(Guard) * Guarding;
light = max(light - own * guard, 0.0) * (1.0 - 0.35 * guard); // a character isn't washed out by the light it carries
float3 lit = Ambient.rgb + SunColor.rgb * sunTerm + light * lightTerm;
float3 color = Base.rgb * lit;
// Strong light rolls painted colour off towards full brightness (hue kept) instead of clipping it to white: a lantern on
// teal stone makes bright teal stone, not a white disc. Guarded sprites roll off earlier still.
float peak = max(color.r, max(color.g, color.b));
float knee = lerp(HighlightKnee, Knee, guard);
if (peak > knee)
{
    float rolled = knee + (1.0 - knee) * (1.0 - exp(-(peak - knee) / (1.0 - knee)));
    color *= rolled / peak;
}
return Scene.rgb + color + halo;
)HLSL");

	/**
	 * Onyx Fog (the actor): drifting fog over a box of the level. UV runs 0 to 1 across the box and from its top (0) to its
	 * bottom (1); the clouds come from world positions, so the fog stays put in the world. Big slow banks and finer wisps
	 * bent by them; soft from every side, thicker towards the bottom (Ground), pushed back round up to four places (ClearN:
	 * world X, world Z, radius, softness); coloured by the level's mood and lit where the 2D lights shine into it (the light
	 * buffer, read at the pixel's place on screen). Drawn after the lighting composite, so it covers what is behind it.
	 */
	const TCHAR* FogVolumeHLSL = TEXT(R"HLSL(
struct FogNoise
{
    // An integer hash of the cell (pcg2d): exact, so two cells always agree on the corner they share. The usual
    // frac(sin(dot(c, k)) * 43758.5) is not: the GPU rounds the big sine a little differently for each cell, and the
    // noise breaks into straight seams along the cell borders (hard lines and boxes in the fog).
    float Hash(float2 c)
    {
        uint2 v = uint2(int2(c) + 65536);
        v = v * 1664525u + 1013904223u;
        v.x += v.y * 1664525u;
        v.y += v.x * 1664525u;
        v ^= v >> 16u;
        v.x += v.y * 1664525u;
        v.y += v.x * 1664525u;
        v ^= v >> 16u;
        return float(v.x) * (1.0 / 4294967296.0);
    }
    float Value(float2 q)
    {
        float2 c = floor(q);
        float2 f = frac(q);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(Hash(c), Hash(c + float2(1.0, 0.0)), f.x), lerp(Hash(c + float2(0.0, 1.0)), Hash(c + float2(1.0, 1.0)), f.x), f.y);
    }
    float Fbm(float2 q)
    {
        float total = 0.0;
        float amp = 0.55;
        for (int i = 0; i < 4; i++)
        {
            total += amp * Value(q);
            q = q * 2.03 + float2(5.2, 1.3);
            amp *= 0.5;
        }
        return total;
    }
};
FogNoise N;
float size = max(CloudSize, 1.0);
float2 p = float2(World.x, World.z) / size;
float2 drift = Drift.xy / size * Time;
// Big slow banks, stretched sideways; finer wisps drifting a little faster, bent by the banks.
float banks = N.Fbm(p * float2(0.5, 1.0) - drift * 0.6);
float wisps = N.Fbm(p * float2(1.4, 2.2) - drift + float2(banks * 1.2, 0.0));
float cloud = lerp(banks, banks * 0.4 + wisps * 0.75, Detail);
// Soft from every side of the box, and never along a straight line: the clouds push its outline in and out, so the box's
// shape is never seen. Thicker towards its bottom as Ground says.
float wob = (banks - 0.5) * 1.6 + (wisps - 0.5) * 0.6;
float2 e = max(Edge.xy, 0.0001);
// A layer lying on the ground lies right on it: the more Ground says, the less its bottom fades (its box's bottom is the floor).
float eb = max(e.y * (1.0 - 0.8 * Ground), 0.0001);
float2 inX = float2(UV.x, 1.0 - UV.x) + wob * e.x * 0.7;
float2 inY = float2(UV.y + wob * e.y * 0.7, 1.0 - UV.y + wob * eb * 0.7);
float edgeX = smoothstep(0.0, e.x, inX.x) * smoothstep(0.0, e.x, inX.y);
float edgeY = smoothstep(0.0, e.y, inY.x) * smoothstep(0.0, eb, inY.y);
edgeX *= edgeX;
edgeY *= edgeY;
float ground = lerp(1.0, saturate(UV.y * 1.3), Ground);
// How much of the box is fog: Density lowers the threshold the clouds must pass; thicker and thinner, never cut off hard.
float d = saturate((cloud - (1.0 - Density) * 0.75) * 2.2 + Density * 0.25);
d *= edgeX * edgeY * ground * Amount;
// Pushed back round the player and anything clearing it.
float4 clears[4] = { Clear0, Clear1, Clear2, Clear3 };
for (int k = 0; k < 4; k++)
{
    float r = clears[k].z;
    if (r > 0.5)
    {
        float dist = length(float2(World.x, World.z) - clears[k].xy);
        d *= smoothstep(r * (1.0 - clears[k].w), r, dist);
    }
}
// Its own colour in the level's mood, plus the lights shining into it (a little brighter where it is thicker); a lamp's
// glow also fills thin fog round it, lighting nothing behind. Fog spreads light all round, so the light is gathered over a
// disc about a third of a cloud across in the world (whatever the zoom): a shadow's hard edge never shows in it as a line.
// The taps turn with the clouds, so no ring of them shows either.
float2 uvPerCm = float2(abs(ddx(ScreenUV.x)) / max(abs(ddx(World.x)), 0.0001), abs(ddy(ScreenUV.y)) / max(abs(ddy(World.z)), 0.0001));
float2 reach = uvPerCm * clamp(size * 0.35, 70.0, 220.0);
float turn = banks * 6.2832;
float2 axis = float2(cos(turn), sin(turn));
float3 light = Texture2DSample(LightTex, LightTexSampler, ScreenUV).rgb * 2.0;
[unroll] for (int t = 0; t < 6; t++)
{
    float a = t * 1.0472;
    float2 inner = float2(axis.x * cos(a) - axis.y * sin(a), axis.x * sin(a) + axis.y * cos(a));
    float2 outer = float2(inner.x * 0.866 - inner.y * 0.5, inner.x * 0.5 + inner.y * 0.866); // between the inner taps
    light += Texture2DSample(LightTex, LightTexSampler, ScreenUV + inner * reach * 0.45).rgb * 1.5;
    light += Texture2DSample(LightTex, LightTexSampler, ScreenUV + outer * reach).rgb;
}
light /= 17.0;
light /= 1.0 + dot(light, float3(0.2126, 0.7152, 0.0722)) * 0.8; // a lamp glows in it, it does not blow out to white
float lum = dot(light, float3(0.2126, 0.7152, 0.0722));
// The mood dims it, but only partly: fog is pale even in a dark level, the way mist shows under a night sky.
float3 rgb = Tint.rgb * lerp(Mood.rgb, 1.0, 0.25) * OwnLight + lerp(light, light * Tint.rgb, 0.5) * Scatter * (0.55 + 0.45 * cloud);
float alpha = saturate(d * Tint.a + lum * Scatter * 0.12 * d);
return float4(rgb, alpha);
)HLSL");

	/**
	 * Onyx Water: UV.x = world X in metres, UV.y = cm under the surface (negative just above it). Smooth gradient with depth,
	 * drifting caustics and light shafts near the top, rising bubbles, and a crisp surface line with a glow under it.
	 * The surface edge is anti-aliased in the shader (fwidth), so it stays smooth without TAA.
	 */
	const TCHAR* WaterHLSL = TEXT(R"HLSL(
float t = Time;
if (Above.y > 0.5)
{
    // Seen from above (a Top Down level): UV = world X and Z in metres, Above.x = cm in from the shore (below 0 outside).
    // Shallow along the shore, deeper towards the middle, caustics all over, glints coming and going, a wavering line of
    // foam where it meets the land.
    float s = Above.x;
    float saa = max(fwidth(s), 0.0001) * 1.5;
    float deep01 = saturate(s / max(Depth, 1.0));
    float3 c = lerp(Shallow.rgb, Deep.rgb, sqrt(deep01));
    float a = lerp(Shallow.a, Deep.a, deep01);
    float2 q = UV * 1.1;
    q += 0.25 * float2(sin(q.y * 4.0 + t * 0.8), sin(q.x * 3.3 - t * 0.6));
    float qa = sin(q.x * 6.2831 + t * 0.5);
    float qb = sin(q.y * 5.6548 - t * 0.35 + qa * 0.8);
    c += Surface.rgb * pow(saturate(1.0 - abs(qa + qb) * 0.5), 6.0) * Caustics * 0.45 * (1.0 - 0.55 * deep01);
    float2 gc = floor(UV * 9.0);
    float gh = frac(sin(dot(gc, float2(41.3, 289.1))) * 43758.5453);
    float twinkle = saturate(sin(t * (1.5 + gh * 2.0) + gh * 40.0) * 4.0 - 3.0);
    float glint = step(0.93, gh) * twinkle * saturate(1.0 - length(frac(UV * 9.0) - 0.5) * 4.0) * saturate(s / 20.0);
    c = lerp(c, Surface.rgb, glint * 0.8);
    float wob = sin(UV.x * 9.0 + t * 1.3) * sin(UV.y * 7.0 - t * 0.9);
    float foam = saturate(1.0 - abs(s - Thickness * (1.0 + 0.5 * wob)) / (Thickness + saa));
    c = lerp(c, Surface.rgb, foam * Surface.a);
    a = max(a, foam * Surface.a);
    return float4(c * Tint.rgb, a * saturate(s / saa + 0.5));
}
float d = UV.y;
float aa = max(fwidth(d), 0.0001) * 1.5;
float inside = saturate(d / aa + 0.5);
float depth01 = saturate(d / max(Depth, 1.0));
float3 col = lerp(Shallow.rgb, Deep.rgb, sqrt(depth01));
float alpha = lerp(Shallow.a, Deep.a, depth01);

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

	/**
	 * Water particles in their vertex colour, all anti-aliased. Kind picks the shape: 0 a soft round splash droplet, 1 a
	 * bubble (thin ring, faint inside, highlight at the top left), 2 a pulse ring, 3 a lumpy patch of foam, 4 a streak of
	 * churned water (UV.y runs from its head to its tail), 5 a soft cloud of silt. Seed varies the foam's lumps, and for a
	 * ring is where the water's surface cuts across it (nothing above).
	 */
	const TCHAR* DropHLSL = TEXT(R"HLSL(
float r = length(UV - 0.5) * 2.0;
float edge = max(fwidth(r), 0.001);
float disc = saturate((1.0 - r) / edge);
float a = disc;
if (Kind < 0.5)
{
    // A droplet: a hard-edged dot like the pixel art round it (a soft one reads as a puff of smoke).
    a = r < 0.85 ? 1.0 : 0.0;
}
else if (Kind > 0.5 && Kind < 1.5)
{
    float rim = saturate(1.0 - abs(r - 0.8) / max(0.18, edge * 1.5));
    float highlight = saturate(1.0 - length(UV - float2(0.36, 0.34)) / 0.16);
    a = max(max(rim, disc * 0.15), highlight) * disc;
}
else if (Kind > 1.5 && Kind < 2.5)
{
    // Only below the surface (Seed: where it cuts across), softly at the cut.
    a = saturate(1.0 - abs(r - 0.86) / max(0.07, edge * 1.5)) * disc * saturate((UV.y - Seed) / max(fwidth(UV.y) * 2.0, 0.0001));
}
else if (Kind > 2.5 && Kind < 3.5)
{
    // Foam: crisp flecks lying along the surface in a thin band (far wider than tall), thinning out towards its ends, with
    // hard edges in steps like pixel art round it. A soft round cloud here reads as smoke, not water.
    float2 g = floor(UV * 8.0);
    float2 c = (g + 0.5) / 8.0 - 0.5;
    float lens = abs(c.y) * 4.0 + abs(c.x) * 1.1;
    float n = frac(sin(dot(g + Seed * 13.7, float2(12.9898, 78.233))) * 43758.5453);
    a = (lens < 0.95 && n > 0.3 + lens * 0.4) ? 1.0 : 0.0;
}
else if (Kind > 3.5 && Kind < 4.5)
{
    float across = 1.0 - abs(UV.x * 2.0 - 1.0);
    float along = 1.0 - UV.y;
    a = smoothstep(0.0, 0.7, across) * pow(saturate(sin(along * 3.14159)), 0.6);
}
else if (Kind > 4.5)
{
    a = pow(saturate(1.0 - r), 1.6);
}
return float4(ColorRGB * Tint.rgb, ColorA * a);
)HLSL");

	/**
	 * Onyx Rain: UV1.x picks the shape. 0 = a streak (UV.y runs from its head, 0, back along its tail, 1), 1 = the ring a
	 * drop leaves on the ground (a flat quad), 2 = a droplet. Vertex colours are stored divided by Brightness, so drops in
	 * lamplight can be brighter than white and bloom.
	 */
	const TCHAR* RainHLSL = TEXT(R"HLSL(
float a = 0.0;
if (Kind < 0.5)
{
    float across = 1.0 - abs(UV.x * 2.0 - 1.0);
    a = saturate(across * 1.6) * pow(saturate(1.0 - UV.y), 1.3);
}
else if (Kind < 1.5)
{
    float r = length(UV * 2.0 - 1.0);
    float edge = max(fwidth(r), 0.02);
    a = saturate(1.0 - abs(r - 0.78) / max(0.24, edge * 1.5)) * saturate((1.0 - r) / edge + 0.5);
}
else
{
    float r = length(UV * 2.0 - 1.0);
    a = saturate((1.0 - r) / max(fwidth(r), 0.05));
}
return float4(ColorRGB * Brightness * Tint.rgb, ColorA * a * Tint.a);
)HLSL");

	/**
	 * Onyx Fog, for a parallax layer: soft clouds of mist drifting sideways. UV.x runs along the layer (TilingX = one repeat
	 * per layer height), UV.y from its top (0) to its bottom (1); the mist thickens towards the bottom and fades out at both
	 * edges, so the band has no visible border. Tint's alpha is how thick it gets; the lighting mood's background tint applies.
	 */
	const TCHAR* FogHLSL = TEXT(R"HLSL(
// Banks of mist far wider than they are tall (cells about 5 x 1): taller than wide, a thin layer of mist turned into
// vertical curtains. Each cell's value is an exact integer hash (pcg2d), so neighbouring cells agree on their shared
// corners: the usual frac(sin(...) * 43758.5) rounds differently per cell on the GPU and breaks into straight seams.
uint2 v;
float2 q = float2(UV.x * TilingX * 0.45, UV.y * 2.2) * Scale + float2(Time * Speed, 0.0);
float total = 0.0;
float amp = 0.55;
for (int i = 0; i < 4; i++)
{
    float2 c = floor(q);
    float2 f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float4 h;
    for (int k = 0; k < 4; k++)
    {
        v = uint2(int2(c) + int2(k & 1, k >> 1) + 65536);
        v = v * 1664525u + 1013904223u;
        v.x += v.y * 1664525u;
        v.y += v.x * 1664525u;
        v ^= v >> 16u;
        v.x += v.y * 1664525u;
        v.y += v.x * 1664525u;
        v ^= v >> 16u;
        h[k] = float(v.x) * (1.0 / 4294967296.0);
    }
    float h00 = h.x;
    float h10 = h.y;
    float h01 = h.z;
    float h11 = h.w;
    total += amp * lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
    q = q * 2.07 + float2(Time * Speed * 0.7, 3.1);
    amp *= 0.5;
}
float band = smoothstep(0.0, 0.5, UV.y) * smoothstep(1.0, 0.82, UV.y);
float mist = saturate(Density * 0.45 + (total - 0.5) * 1.15) * band; // gentle contrast: thicker and thinner, never cut off
return float4(Tint.rgb * Mood.rgb, Tint.a * mist);
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

		/**
		 * The draw order (Onyx Draw Order subsystem, custom primitive data) as a world position offset straight towards
		 * the camera: the sprite gets its own depth, and moving along the view ray keeps it exactly where it is on screen.
		 * bFlatten first lays a mesh flat on the plane through its origin (placeholder cubes are a few cm deep, which would
		 * otherwise reach past the draw order of the sprites around them); from the side camera it looks the same.
		 */
		void DrawOrderOffset(int32 X, int32 Y, bool bFlatten = false)
		{
			UMaterialExpressionScalarParameter* Order = Scalar(TEXT("DrawOrder"), 0.f, X, Y);
			Order->bUseCustomPrimitiveData = true;
			Order->PrimitiveDataIndex = uint8(Onyx2D::DrawOrderDataIndex);
			// Towards the camera, in the vertex shader: along the view ray in perspective (the vertex stays on its ray, so
			// on the same screen pixel), along the view axis in orthographic (every ray is parallel to it).
			UMaterialExpressionSubtract* Ray = Add<UMaterialExpressionSubtract>(X - 300, Y - 260);
			Link(Add<UMaterialExpressionCameraPositionWS>(X - 600, Y - 300), FString(), Ray, TEXT("A"));
			Link(Add<UMaterialExpressionWorldPosition>(X - 600, Y - 200), FString(), Ray, TEXT("B"));
			UMaterialExpressionNormalize* RayDirection = Add<UMaterialExpressionNormalize>(X - 100, Y - 260);
			Link(Ray, FString(), RayDirection, FString());
			UMaterialExpressionConstant3Vector* ViewBack = Add<UMaterialExpressionConstant3Vector>(X - 600, Y - 100);
			ViewBack->Constant = FLinearColor(0.f, 0.f, -1.f);
			UMaterialExpressionTransform* AxisToCamera = Add<UMaterialExpressionTransform>(X - 300, Y - 100);
			AxisToCamera->TransformSourceType = TRANSFORMSOURCE_View;
			AxisToCamera->TransformType = TRANSFORM_World;
			Link(ViewBack, FString(), AxisToCamera, FString());
			UMaterialExpressionLinearInterpolate* ToCamera = Add<UMaterialExpressionLinearInterpolate>(X + 100, Y - 180);
			Link(RayDirection, FString(), ToCamera, TEXT("A"));
			Link(AxisToCamera, FString(), ToCamera, TEXT("B"));
			Link(Add<UMaterialExpressionIsOrthographic>(X - 300, Y + 20), FString(), ToCamera, TEXT("Alpha"));
			UMaterialExpressionMultiply* Offset = Add<UMaterialExpressionMultiply>(X + 300, Y + 60);
			Link(ToCamera, FString(), Offset, TEXT("A"));
			Link(Order, FString(), Offset, TEXT("B"));
			if (!bFlatten)
			{
				Out(Offset, FString(), MP_WorldPositionOffset);
				return;
			}
			// (0, ObjectY - VertexY, 0): every vertex onto the plane through the object's origin.
			UMaterialExpressionSubtract* ToPlane = Add<UMaterialExpressionSubtract>(X + 300, Y + 260);
			Link(Add<UMaterialExpressionObjectPositionWS>(X, Y + 240), FString(), ToPlane, TEXT("A"));
			Link(Add<UMaterialExpressionWorldPosition>(X, Y + 340), FString(), ToPlane, TEXT("B"));
			UMaterialExpressionConstant3Vector* DepthAxis = Add<UMaterialExpressionConstant3Vector>(X, Y + 440);
			DepthAxis->Constant = FLinearColor(0.f, 1.f, 0.f);
			UMaterialExpressionMultiply* Flatten = Add<UMaterialExpressionMultiply>(X + 500, Y + 300);
			Link(ToPlane, FString(), Flatten, TEXT("A"));
			Link(DepthAxis, FString(), Flatten, TEXT("B"));
			UMaterialExpressionAdd* Total = Add<UMaterialExpressionAdd>(X + 700, Y + 160);
			Link(Flatten, FString(), Total, TEXT("A"));
			Link(Offset, FString(), Total, TEXT("B"));
			Out(Total, FString(), MP_WorldPositionOffset);
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
		// A material that doesn't compile is drawn with Unreal's default one, without a word in game: say so here, where
		// it's built.
		if (FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform))
		{
			Resource->FinishCompilation();
			for (const FString& Error : Resource->GetCompileErrors())
			{
				UE_LOG(LogOnyxMaterials, Error, TEXT("%s does not compile: %s"), *Material->GetName(), *Error);
			}
		}
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

	bool IsOutdated(const TCHAR* Name, FName Parameter);

	/** The parameter that marks materials built with the draw order. */
	const FName DrawOrderMarker(TEXT("DrawOrder"));

	void BuildSolid(bool bRebuild, TArray<FString>& Built)
	{
		// Lit solid colour: placeholders, level builder blocks, hazards.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxSolid"), bRebuild || IsOutdated(TEXT("M_OnyxSolid"), DrawOrderMarker));
		if (!Material)
		{
			return;
		}
		FGraph Graph{ Material };
		Graph.Out(Graph.Vector(TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f), -400, -100), FString(), MP_BaseColor);
		Graph.Out(Graph.Scalar(TEXT("Roughness"), 0.85f, -400, 150), FString(), MP_Roughness);
		Graph.DrawOrderOffset(-1200, 300, true);
		Material->bUsedWithInstancedStaticMeshes = true;
		FinishMaterial(Material, Built);
	}

	void BuildBackground(bool bRebuild, UMaterialParameterCollection* Collection, TArray<FString>& Built)
	{
		// Unlit, masked, tiling texture for parallax layers, tinted by the lighting mood.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxBackground"), bRebuild || IsOutdated(TEXT("M_OnyxBackground"), TEXT("BottomRow")));
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

		// Repeats sideways; downwards it stops at the picture's last row (BottomRow), which fills a layer extended below.
		UMaterialExpressionMin* ClampV = Graph.Add<UMaterialExpressionMin>(-700, 120);
		Graph.Link(Graph.Mask(Scaled, false, true, false, false, -800, 120), FString(), ClampV, TEXT("A"));
		Graph.Link(Graph.Scalar(TEXT("BottomRow"), 0.999f, -900, 220), FString(), ClampV, TEXT("B"));
		UMaterialExpressionAppendVector* Wrapped = Graph.Add<UMaterialExpressionAppendVector>(-650, 40);
		Graph.Link(Graph.Mask(Scaled, true, false, false, false, -800, 40), FString(), Wrapped, TEXT("A"));
		Graph.Link(ClampV, FString(), Wrapped, TEXT("B"));

		UMaterialExpressionTextureSampleParameter2D* Texture = Graph.Add<UMaterialExpressionTextureSampleParameter2D>(-600, 0);
		Texture->ParameterName = TEXT("Texture");
		Texture->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		Texture->SamplerType = SAMPLERTYPE_Color;
		Graph.Link(Wrapped, FString(), Texture, TEXT("UVs"));

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
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxGlow"), bRebuild || IsOutdated(TEXT("M_OnyxGlow"), DrawOrderMarker));
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
		Graph.DrawOrderOffset(-1200, 300, true);
		FinishMaterial(Material, Built);
	}

	/** True when a plugin material exists but predates a parameter (rebuild it). */
	/** True when the material exists but lacks the scalar parameter that marks the current version of its graph. */
	bool IsOutdated(const TCHAR* Name, FName Parameter)
	{
		const UMaterial* Existing = LoadPluginAsset<UMaterial>(Name);
		if (!Existing)
		{
			return false;
		}
		for (const UMaterialExpression* Expression : Existing->GetExpressions())
		{
			if (const UMaterialExpressionParameter* Named = Cast<UMaterialExpressionParameter>(Expression); Named && Named->ParameterName == Parameter)
			{
				return false;
			}
		}
		if (!Existing->GetExpressions().IsEmpty())
		{
			return true; // the graph is loaded and the marker isn't in it
		}
		// A commandlet can load the plugin's materials before editor-only data is on, so the graph isn't in memory:
		// look for the marker in the saved package's names instead.
		FString File;
		TArray<uint8> Bytes;
		if (!FPackageName::TryConvertLongPackageNameToFilename(Existing->GetPackage()->GetName(), File, FPackageName::GetAssetPackageExtension())
			|| !FFileHelper::LoadFileToArray(Bytes, *File))
		{
			return true;
		}
		const FTCHARToUTF8 Marker(*Parameter.ToString());
		const int32 Length = Marker.Length();
		for (int32 Index = 0; Index + Length <= Bytes.Num(); ++Index)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Index, Marker.Get(), Length) == 0)
			{
				return false;
			}
		}
		return true;
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
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxWater"), bRebuild || IsOutdated(TEXT("M_OnyxWater"), TEXT("WaterVersion6")));
		if (!Material)
		{
			return;
		}
		SetupWaterSurface(Material);
		FGraph Graph{ Material };
		UMaterialExpressionCustom* Custom = AddCustom(Graph, WaterHLSL, TEXT("Onyx Water"),
			{ TEXT("UV"), TEXT("Above"), TEXT("Time"), TEXT("Shallow"), TEXT("Deep"), TEXT("Surface"), TEXT("Depth"), TEXT("Thickness"), TEXT("Caustics"), TEXT("Bubbles"), TEXT("Tint"), TEXT("Version") });
		Graph.Link(Graph.Add<UMaterialExpressionTextureCoordinate>(-900, -400), FString(), Custom, TEXT("UV"));
		// UV1: x = cm in from the shore, y = 1 for water seen from above (Top Down levels); nothing for the side view.
		UMaterialExpressionTextureCoordinate* AboveUV = Graph.Add<UMaterialExpressionTextureCoordinate>(-1100, -350);
		AboveUV->CoordinateIndex = 1;
		Graph.Link(AboveUV, FString(), Custom, TEXT("Above"));
		Graph.Link(Graph.Add<UMaterialExpressionTime>(-900, -300), FString(), Custom, TEXT("Time"));
		Graph.Link(Graph.Vector(TEXT("ShallowColor"), FLinearColor(0.12f, 0.6f, 0.86f, 0.5f), -900, -200), TEXT("RGBA"), Custom, TEXT("Shallow"));
		Graph.Link(Graph.Vector(TEXT("DeepColor"), FLinearColor(0.02f, 0.14f, 0.4f, 0.88f), -900, -50), TEXT("RGBA"), Custom, TEXT("Deep"));
		Graph.Link(Graph.Vector(TEXT("SurfaceColor"), FLinearColor(0.85f, 0.97f, 1.f, 1.f), -900, 100), TEXT("RGBA"), Custom, TEXT("Surface"));
		Graph.Link(Graph.Scalar(TEXT("Depth"), 300.f, -900, 250), FString(), Custom, TEXT("Depth"));
		Graph.Link(Graph.Scalar(TEXT("Thickness"), 3.f, -900, 350), FString(), Custom, TEXT("Thickness"));
		Graph.Link(Graph.Scalar(TEXT("Caustics"), 0.6f, -900, 450), FString(), Custom, TEXT("Caustics"));
		Graph.Link(Graph.Scalar(TEXT("Bubbles"), 1.f, -900, 550), FString(), Custom, TEXT("Bubbles"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor::White, -900, 650), FString(), Custom, TEXT("Tint"));
		Graph.Link(Graph.Scalar(TEXT("WaterVersion6"), 1.f, -900, 800), FString(), Custom, TEXT("Version")); // marks this version (older ones are rebuilt)
		OutputColorAndOpacity(Graph, Custom);
		FinishMaterial(Material, Built);
	}

	void BuildWaterDrop(bool bRebuild, TArray<FString>& Built)
	{
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxWaterDrop"), bRebuild || IsOutdated(TEXT("M_OnyxWaterDrop"), TEXT("DropVersion6")));
		if (!Material)
		{
			return;
		}
		SetupWaterSurface(Material);
		FGraph Graph{ Material };
		UMaterialExpressionCustom* Custom = AddCustom(Graph, DropHLSL, TEXT("Onyx Water Drop"),
			{ TEXT("UV"), TEXT("Kind"), TEXT("Seed"), TEXT("ColorRGB"), TEXT("ColorA"), TEXT("Tint"), TEXT("Version") });
		Graph.Link(Graph.Add<UMaterialExpressionTextureCoordinate>(-900, -200), FString(), Custom, TEXT("UV"));
		UMaterialExpressionTextureCoordinate* ShapeUV = Graph.Add<UMaterialExpressionTextureCoordinate>(-1100, -100);
		ShapeUV->CoordinateIndex = 1; // UV1: x = the shape, y = its seed
		Graph.Link(Graph.Mask(ShapeUV, true, false, false, false, -900, -100), FString(), Custom, TEXT("Kind"));
		Graph.Link(Graph.Mask(ShapeUV, false, true, false, false, -900, -75), FString(), Custom, TEXT("Seed"));
		Graph.Link(Graph.Scalar(TEXT("DropVersion6"), 1.f, -900, 300), FString(), Custom, TEXT("Version")); // marks this version (older ones are rebuilt)
		UMaterialExpressionVertexColor* Vertex = Graph.Add<UMaterialExpressionVertexColor>(-900, -50);
		Graph.Link(Vertex, FString(), Custom, TEXT("ColorRGB"));
		Graph.Link(Vertex, TEXT("A"), Custom, TEXT("ColorA"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor::White, -900, 150), FString(), Custom, TEXT("Tint"));
		OutputColorAndOpacity(Graph, Custom);
		FinishMaterial(Material, Built);
	}

	void BuildRain(bool bRebuild, TArray<FString>& Built)
	{
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxRain"), bRebuild || IsOutdated(TEXT("M_OnyxRain"), TEXT("RainVersion1")));
		if (!Material)
		{
			return;
		}
		SetupWaterSurface(Material); // unlit translucent after the 2D lighting composite, like water
		FGraph Graph{ Material };
		UMaterialExpressionCustom* Custom = AddCustom(Graph, RainHLSL, TEXT("Onyx Rain"),
			{ TEXT("UV"), TEXT("Kind"), TEXT("ColorRGB"), TEXT("ColorA"), TEXT("Brightness"), TEXT("Tint"), TEXT("Version") });
		Graph.Link(Graph.Add<UMaterialExpressionTextureCoordinate>(-900, -200), FString(), Custom, TEXT("UV"));
		UMaterialExpressionTextureCoordinate* ShapeUV = Graph.Add<UMaterialExpressionTextureCoordinate>(-1100, -100);
		ShapeUV->CoordinateIndex = 1; // UV1.x: the shape
		Graph.Link(Graph.Mask(ShapeUV, true, false, false, false, -900, -100), FString(), Custom, TEXT("Kind"));
		UMaterialExpressionVertexColor* Vertex = Graph.Add<UMaterialExpressionVertexColor>(-900, 0);
		Graph.Link(Vertex, FString(), Custom, TEXT("ColorRGB"));
		Graph.Link(Vertex, TEXT("A"), Custom, TEXT("ColorA"));
		Graph.Link(Graph.Scalar(TEXT("Brightness"), 3.f, -900, 120), FString(), Custom, TEXT("Brightness"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor::White, -900, 220), TEXT("RGBA"), Custom, TEXT("Tint"));
		Graph.Link(Graph.Scalar(TEXT("RainVersion1"), 1.f, -900, 360), FString(), Custom, TEXT("Version")); // marks this version (older ones are rebuilt)
		OutputColorAndOpacity(Graph, Custom);
		FinishMaterial(Material, Built);
	}

	void BuildFog(bool bRebuild, UMaterialParameterCollection* Collection, TArray<FString>& Built)
	{
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxFog"), bRebuild || IsOutdated(TEXT("M_OnyxFog"), TEXT("FogVersion3")));
		if (!Material)
		{
			return;
		}
		SetupWaterSurface(Material);
		FGraph Graph{ Material };
		UMaterialExpressionCustom* Custom = AddCustom(Graph, FogHLSL, TEXT("Onyx Fog"),
			{ TEXT("UV"), TEXT("Time"), TEXT("Tint"), TEXT("Mood"), TEXT("Density"), TEXT("Speed"), TEXT("Scale"), TEXT("TilingX"), TEXT("Version") });
		Graph.Link(Graph.Add<UMaterialExpressionTextureCoordinate>(-900, -300), FString(), Custom, TEXT("UV"));
		Graph.Link(Graph.Add<UMaterialExpressionTime>(-900, -200), FString(), Custom, TEXT("Time"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor(0.45f, 0.58f, 0.78f, 0.5f), -900, -100), TEXT("RGBA"), Custom, TEXT("Tint"));
		UMaterialExpressionCollectionParameter* Mood = Graph.Add<UMaterialExpressionCollectionParameter>(-900, 50);
		Mood->Collection = Collection;
		Mood->ParameterName = TEXT("BackgroundTint");
		if (Collection && Collection->VectorParameters.Num() > 0)
		{
			Mood->ParameterId = Collection->VectorParameters[0].Id;
		}
		Graph.Link(Mood, FString(), Custom, TEXT("Mood"));
		Graph.Link(Graph.Scalar(TEXT("Density"), 0.6f, -900, 200), FString(), Custom, TEXT("Density"));
		Graph.Link(Graph.Scalar(TEXT("Speed"), 0.04f, -900, 300), FString(), Custom, TEXT("Speed"));
		Graph.Link(Graph.Scalar(TEXT("Scale"), 1.6f, -900, 400), FString(), Custom, TEXT("Scale"));
		Graph.Link(Graph.Scalar(TEXT("TilingX"), 1.f, -900, 500), FString(), Custom, TEXT("TilingX"));
		Graph.Link(Graph.Scalar(TEXT("FogVersion3"), 1.f, -900, 640), FString(), Custom, TEXT("Version")); // marks this version (older ones are rebuilt)
		OutputColorAndOpacity(Graph, Custom);
		FinishMaterial(Material, Built);
	}

	void BuildFogVolume(bool bRebuild, UMaterialParameterCollection* Collection, UTextureRenderTarget2D* Placeholder, TArray<FString>& Built)
	{
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxFogVolume"), bRebuild || IsOutdated(TEXT("M_OnyxFogVolume"), TEXT("FogVolumeVersion6")));
		if (!Material)
		{
			return;
		}
		SetupWaterSurface(Material); // unlit, translucent, after the lighting composite
		FGraph Graph{ Material };
		UMaterialExpressionCustom* Custom = AddCustom(Graph, FogVolumeHLSL, TEXT("Onyx Fog Volume"),
			{ TEXT("UV"), TEXT("World"), TEXT("Time"), TEXT("Tint"), TEXT("Mood"), TEXT("Density"), TEXT("CloudSize"), TEXT("Drift"), TEXT("Ground"),
			  TEXT("Detail"), TEXT("Edge"), TEXT("Amount"), TEXT("Clear0"), TEXT("Clear1"), TEXT("Clear2"), TEXT("Clear3"), TEXT("LightTex"),
			  TEXT("ScreenUV"), TEXT("Scatter"), TEXT("OwnLight"), TEXT("Version") });
		Graph.Link(Graph.Add<UMaterialExpressionTextureCoordinate>(-1100, -500), FString(), Custom, TEXT("UV"));
		Graph.Link(Graph.Add<UMaterialExpressionWorldPosition>(-1100, -420), FString(), Custom, TEXT("World"));
		Graph.Link(Graph.Add<UMaterialExpressionTime>(-1100, -340), FString(), Custom, TEXT("Time"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor(0.66f, 0.72f, 0.8f, 0.6f), -1100, -260), TEXT("RGBA"), Custom, TEXT("Tint"));
		UMaterialExpressionCollectionParameter* Mood = Graph.Add<UMaterialExpressionCollectionParameter>(-1100, -140);
		Mood->Collection = Collection;
		Mood->ParameterName = TEXT("BackgroundTint");
		if (Collection && Collection->VectorParameters.Num() > 0)
		{
			Mood->ParameterId = Collection->VectorParameters[0].Id;
		}
		Graph.Link(Mood, FString(), Custom, TEXT("Mood"));
		Graph.Link(Graph.Scalar(TEXT("Density"), 0.45f, -1100, -40), FString(), Custom, TEXT("Density"));
		Graph.Link(Graph.Scalar(TEXT("CloudSize"), 360.f, -1100, 40), FString(), Custom, TEXT("CloudSize"));
		Graph.Link(Graph.Vector(TEXT("Drift"), FLinearColor(16.f, 0.f, 0.f, 0.f), -1100, 120), FString(), Custom, TEXT("Drift"));
		Graph.Link(Graph.Scalar(TEXT("Ground"), 0.85f, -1100, 240), FString(), Custom, TEXT("Ground"));
		Graph.Link(Graph.Scalar(TEXT("Detail"), 0.45f, -1100, 320), FString(), Custom, TEXT("Detail"));
		Graph.Link(Graph.Vector(TEXT("Edge"), FLinearColor(0.1f, 0.2f, 0.f, 0.f), -1100, 400), FString(), Custom, TEXT("Edge"));
		Graph.Link(Graph.Scalar(TEXT("Amount"), 1.f, -1100, 520), FString(), Custom, TEXT("Amount"));
		int32 Y = 600;
		for (const TCHAR* Clear : { TEXT("Clear0"), TEXT("Clear1"), TEXT("Clear2"), TEXT("Clear3") })
		{
			Graph.Link(Graph.Vector(Clear, FLinearColor(0.f, 0.f, 0.f, 0.f), -1100, Y), TEXT("RGBA"), Custom, Clear);
			Y += 110;
		}
		UMaterialExpressionTextureObjectParameter* LightBuffer = Graph.Add<UMaterialExpressionTextureObjectParameter>(-1100, Y);
		LightBuffer->ParameterName = TEXT("LightBuffer");
		LightBuffer->Texture = Placeholder;
		LightBuffer->SamplerType = SAMPLERTYPE_LinearColor;
		Graph.Link(LightBuffer, FString(), Custom, TEXT("LightTex"));
		Graph.Link(Graph.Add<UMaterialExpressionScreenPosition>(-1100, Y + 140), TEXT("ViewportUV"), Custom, TEXT("ScreenUV"));
		Graph.Link(Graph.Scalar(TEXT("Scatter"), 1.f, -1100, Y + 240), FString(), Custom, TEXT("Scatter"));
		Graph.Link(Graph.Scalar(TEXT("OwnLight"), 0.45f, -1100, Y + 320), FString(), Custom, TEXT("OwnLight"));
		Graph.Link(Graph.Scalar(TEXT("FogVolumeVersion6"), 1.f, -1100, Y + 420), FString(), Custom, TEXT("Version")); // marks this version
		OutputColorAndOpacity(Graph, Custom);
		FinishMaterial(Material, Built);
	}

	void BuildLitSprite(bool bRebuild, TArray<FString>& Built)
	{
		// Lit sprite with normal map. Paper2D swaps "SpriteTexture" for the sprite's sheet and
		// "SpriteAdditionalTexture_0" for its Additional Textures slot 0 (the normal map).
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxLitSprite"), bRebuild || IsOutdated(TEXT("M_OnyxLitSprite"), DrawOrderMarker)
			|| IsOutdated(TEXT("M_OnyxLitSprite"), TEXT("Flash")) || IsOutdated(TEXT("M_OnyxLitSprite"), TEXT("Dissolve"))
			|| IsOutdated(TEXT("M_OnyxLitSprite"), TEXT("LightGuard")) || IsOutdated(TEXT("M_OnyxLitSprite"), TEXT("GlowMask"))
			|| IsOutdated(TEXT("M_OnyxLitSprite"), TEXT("GlowHeat")) || IsOutdated(TEXT("M_OnyxLitSprite"), TEXT("SeeThrough")));
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
		// Hit flash (Onyx Feedback): the sprite turns the flash colour, lit or not, as its amount rises. Custom primitive data:
		// the colour from FlashColorDataIndex, the amount at FlashDataIndex.
		UMaterialExpressionVectorParameter* FlashColor = Graph.Vector(TEXT("FlashColor"), FLinearColor::White, -900, 1150);
		FlashColor->bUseCustomPrimitiveData = true;
		FlashColor->PrimitiveDataIndex = uint8(Onyx2D::FlashColorDataIndex);
		UMaterialExpressionScalarParameter* Flash = Graph.Scalar(TEXT("Flash"), 0.f, -900, 1300);
		Flash->bUseCustomPrimitiveData = true;
		Flash->PrimitiveDataIndex = uint8(Onyx2D::FlashDataIndex);
		UMaterialExpressionLinearInterpolate* Unflashed = Graph.Add<UMaterialExpressionLinearInterpolate>(-300, -200);
		Unflashed->ConstB = 0.f;
		Graph.Link(Color, FString(), Unflashed, TEXT("A"));
		Graph.Link(Flash, FString(), Unflashed, TEXT("Alpha"));
		// Glowing pixels (custom primitive data at GlowPixelsDataIndex, threshold at GlowThresholdDataIndex): pixels brighter
		// than the threshold shine on their own, lit or not, and leave the lit part (no lighting them twice).
		UMaterialExpressionScalarParameter* GlowPixels = Graph.Scalar(TEXT("GlowPixels"), 0.f, -900, 1750);
		GlowPixels->bUseCustomPrimitiveData = true;
		GlowPixels->PrimitiveDataIndex = uint8(Onyx2D::GlowPixelsDataIndex);
		UMaterialExpressionScalarParameter* GlowThreshold = Graph.Scalar(TEXT("GlowThreshold"), 0.f, -900, 1850);
		GlowThreshold->bUseCustomPrimitiveData = true;
		GlowThreshold->PrimitiveDataIndex = uint8(Onyx2D::GlowThresholdDataIndex);
		// A threshold below 0 means the glow mask (Additional Textures slot 1, from a Pixel Light) names the glowing pixels.
		UMaterialExpressionSpriteTextureSampler* GlowMaskTexture = Graph.Add<UMaterialExpressionSpriteTextureSampler>(-900, 1950);
		GlowMaskTexture->bSampleAdditionalTextures = true;
		GlowMaskTexture->AdditionalSlotIndex = 1;
		GlowMaskTexture->ParameterName = FName(TEXT("SpriteAdditionalTexture"), 2); // Paper2D's name for slot 1
		GlowMaskTexture->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"));
		GlowMaskTexture->SamplerType = SAMPLERTYPE_Color;
		UMaterialExpressionMultiply* GlowMask = Graph.Add<UMaterialExpressionMultiply>(-700, 1950);
		Graph.Link(GlowMaskTexture, TEXT("R"), GlowMask, TEXT("A"));
		Graph.Link(Graph.Scalar(TEXT("GlowMask"), 1.f, -900, 2100), FString(), GlowMask, TEXT("B"));
		UMaterialExpressionCustom* Glowing = AddCustom(Graph,
			TEXT("float threshold = Threshold > 0.0 ? Threshold : 0.75;\n")
			TEXT("float peak = max(Color.r, max(Color.g, Color.b));\n")
			TEXT("float mask = (Threshold < 0.0 ? saturate(Mask) : saturate((peak - threshold) / 0.08)) * saturate(Strength);\n")
			TEXT("return float4(Color * (1.0 - mask), mask);"),
			TEXT("Onyx Glowing Pixels"), { TEXT("Color"), TEXT("Strength"), TEXT("Threshold"), TEXT("Mask") });
		Glowing->OutputType = CMOT_Float4;
		Graph.Link(Unflashed, FString(), Glowing, TEXT("Color"));
		Graph.Link(GlowPixels, FString(), Glowing, TEXT("Strength"));
		Graph.Link(GlowThreshold, FString(), Glowing, TEXT("Threshold"));
		Graph.Link(GlowMask, FString(), Glowing, TEXT("Mask"));
		Graph.Out(Graph.Mask(Glowing, true, true, true, false, -100, -200), FString(), MP_BaseColor);
		UMaterialExpressionMultiply* FlashGlow = Graph.Add<UMaterialExpressionMultiply>(-300, 1200);
		Graph.Link(FlashColor, FString(), FlashGlow, TEXT("A"));
		Graph.Link(Flash, FString(), FlashGlow, TEXT("B"));
		// The glow itself: the pixel's own colour, brighter as Glow Pixels rises past 1, and past 1 burning towards white the
		// brighter the pixel is (Glow Heat): the pale middle of a blade goes white-hot while its darker edge keeps its colour.
		UMaterialExpressionCustom* Shine = AddCustom(Graph,
			TEXT("float threshold = Threshold > 0.0 ? Threshold : 0.75;\n")
			TEXT("float peak = max(Color.r, max(Color.g, Color.b));\n")
			TEXT("float mask = Threshold < 0.0 ? saturate(Mask) : saturate((peak - threshold) / 0.08);\n")
			TEXT("float burn = saturate((Strength - 1.0) * Heat) * peak;\n")
			TEXT("return Flash + lerp(Color, peak.xxx, burn) * mask * Strength;"),
			TEXT("Onyx Glow"), { TEXT("Color"), TEXT("Strength"), TEXT("Threshold"), TEXT("Flash"), TEXT("Mask"), TEXT("Heat") });
		Shine->OutputType = CMOT_Float3;
		Graph.Link(Unflashed, FString(), Shine, TEXT("Color"));
		Graph.Link(GlowPixels, FString(), Shine, TEXT("Strength"));
		Graph.Link(GlowThreshold, FString(), Shine, TEXT("Threshold"));
		Graph.Link(FlashGlow, FString(), Shine, TEXT("Flash"));
		Graph.Link(GlowMask, FString(), Shine, TEXT("Mask"));
		Graph.Link(Graph.Scalar(TEXT("GlowHeat"), 0.4f, -900, 2250), FString(), Shine, TEXT("Heat"));
		Graph.Out(Shine, FString(), MP_EmissiveColor);

		UMaterialExpressionMultiply* Alpha = Graph.Add<UMaterialExpressionMultiply>(-550, 0);
		Graph.Link(Base, TEXT("A"), Alpha, TEXT("A"));
		Graph.Link(Vertex, TEXT("A"), Alpha, TEXT("B"));
		// Dissolve (custom primitive data at DissolveDataIndex): the art pixels drop out one by one in a random order, on the
		// world's pixel grid (sprites are snapped to it), as a body comes apart. See-through (at SeeThroughDataIndex): they
		// drop out in an even 4 x 4 pattern on the same grid instead (a tree the player walks behind).
		UMaterialExpressionScalarParameter* Dissolve = Graph.Scalar(TEXT("Dissolve"), 0.f, -900, 1450);
		Dissolve->bUseCustomPrimitiveData = true;
		Dissolve->PrimitiveDataIndex = uint8(Onyx2D::DissolveDataIndex);
		UMaterialExpressionScalarParameter* SeeThrough = Graph.Scalar(TEXT("SeeThrough"), 0.f, -900, 1500);
		SeeThrough->bUseCustomPrimitiveData = true;
		SeeThrough->PrimitiveDataIndex = uint8(Onyx2D::SeeThroughDataIndex);
		UMaterialExpressionCustom* Pixels = AddCustom(Graph,
			TEXT("float2 Cell = floor(float2(Position.x, Position.z) / max(Grid, 0.01));\n")
			TEXT("float Noise = frac(sin(dot(Cell, float2(12.9898, 78.233))) * 43758.5453);\n")
			TEXT("int2 C = int2(Cell) & 3;\n")
			TEXT("int A = C.x ^ C.y;\n")
			TEXT("float Order = (float)(((A & 1) << 3) | ((C.y & 1) << 2) | (A & 2) | ((C.y >> 1) & 1)) / 16.0 + 1.0 / 32.0;\n")
			TEXT("return Alpha * step(Dissolve, Noise * 0.999) * step(SeeThrough, Order);"),
			TEXT("Onyx Dissolve"), { TEXT("Alpha"), TEXT("Dissolve"), TEXT("Position"), TEXT("Grid"), TEXT("SeeThrough") });
		Pixels->OutputType = CMOT_Float1;
		Graph.Link(Alpha, FString(), Pixels, TEXT("Alpha"));
		Graph.Link(Dissolve, FString(), Pixels, TEXT("Dissolve"));
		Graph.Link(SeeThrough, FString(), Pixels, TEXT("SeeThrough"));
		Graph.Link(Graph.Add<UMaterialExpressionWorldPosition>(-900, 1550), FString(), Pixels, TEXT("Position"));
		Graph.Link(Graph.Scalar(TEXT("PixelGrid"), 4.f, -900, 1650), FString(), Pixels, TEXT("Grid"));
		Graph.Out(Pixels, FString(), MP_OpacityMask);

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
		// How much it is kept from washing out (custom primitive data at LightGuardDataIndex): to the composite through the
		// G-buffer's metallic, which nothing 2D uses (no engine lights, no reflections).
		UMaterialExpressionScalarParameter* Guard = Graph.Scalar(TEXT("LightGuard"), 0.f, -550, 750);
		Guard->bUseCustomPrimitiveData = true;
		Guard->PrimitiveDataIndex = uint8(Onyx2D::LightGuardDataIndex);
		Graph.Out(Guard, FString(), MP_Metallic);
		Graph.DrawOrderOffset(-900, 900);
		FinishMaterial(Material, Built);
	}

	void BuildAfterimage(bool bRebuild, TArray<FString>& Built)
	{
		// A sprite's shape filled with light (its vertex colour), see-through: dash trails and other afterimages.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxAfterimage"), bRebuild);
		if (!Material)
		{
			return;
		}
		Material->SetShadingModel(MSM_Unlit);
		Material->BlendMode = BLEND_Translucent;
		Material->TwoSided = true;
		FGraph Graph{ Material };
		UMaterialExpressionSpriteTextureSampler* Base = Graph.Add<UMaterialExpressionSpriteTextureSampler>(-700, 0);
		Base->ParameterName = TEXT("SpriteTexture");
		Base->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		Base->SamplerType = SAMPLERTYPE_Color;
		UMaterialExpressionVertexColor* Vertex = Graph.Add<UMaterialExpressionVertexColor>(-700, 250);
		UMaterialExpressionMultiply* Glow = Graph.Add<UMaterialExpressionMultiply>(-300, 150);
		Graph.Link(Vertex, FString(), Glow, TEXT("A"));
		Graph.Link(Graph.Scalar(TEXT("Intensity"), 1.6f, -700, 450), FString(), Glow, TEXT("B"));
		Graph.Out(Glow, FString(), MP_EmissiveColor);
		UMaterialExpressionMultiply* Alpha = Graph.Add<UMaterialExpressionMultiply>(-300, 0);
		Graph.Link(Base, TEXT("A"), Alpha, TEXT("A"));
		Graph.Link(Vertex, TEXT("A"), Alpha, TEXT("B"));
		Graph.Out(Alpha, FString(), MP_Opacity);
		FinishMaterial(Material, Built);
	}

	void BuildEffectSprite(bool bRebuild, TArray<FString>& Built)
	{
		// Effects drawn in code (sparks, dust, smoke, bursts): see-through and unlit, laid over the finished 2D lighting
		// (after depth of field), so the light composite never touches them: their colours stay as drawn, dark ones too,
		// and nothing under them (a sprite mid hit-flash) shows through wrong.
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxEffectSprite"), bRebuild);
		if (!Material)
		{
			return;
		}
		Material->SetShadingModel(MSM_Unlit);
		Material->BlendMode = BLEND_Translucent;
		Material->TwoSided = true;
		Material->TranslucencyPass = MTP_AfterDOF;
		FGraph Graph{ Material };
		UMaterialExpressionSpriteTextureSampler* Base = Graph.Add<UMaterialExpressionSpriteTextureSampler>(-700, 0);
		Base->ParameterName = TEXT("SpriteTexture");
		Base->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		Base->SamplerType = SAMPLERTYPE_Color;
		UMaterialExpressionVertexColor* Vertex = Graph.Add<UMaterialExpressionVertexColor>(-700, 250);
		UMaterialExpressionMultiply* Color = Graph.Add<UMaterialExpressionMultiply>(-300, -100);
		Graph.Link(Base, TEXT("RGB"), Color, TEXT("A"));
		Graph.Link(Vertex, FString(), Color, TEXT("B"));
		Graph.Out(Color, FString(), MP_EmissiveColor);
		UMaterialExpressionMultiply* Alpha = Graph.Add<UMaterialExpressionMultiply>(-300, 100);
		Graph.Link(Base, TEXT("A"), Alpha, TEXT("A"));
		Graph.Link(Vertex, TEXT("A"), Alpha, TEXT("B"));
		Graph.Out(Alpha, FString(), MP_Opacity);
		FinishMaterial(Material, Built);
	}

	void BuildMirror(bool bRebuild, TArray<FString>& Built)
	{
		// An Onyx Mirror Floor's reflection, a post-process pass after the 2D lighting composite: the floor's stretch of the
		// screen under its surface line shows the finished picture above that line, mirrored, fading out with depth. So
		// everything there is reflected, lit as it is: walls, windows, lights, props, the characters. The floor sets its
		// stretch each frame (MirrorArea: left, right, surface line and depth in viewport UV).
		const UMaterial* Existing = LoadPluginAsset<UMaterial>(TEXT("M_OnyxMirror"));
		UMaterial* Material = BeginMaterial(TEXT("M_OnyxMirror"), bRebuild || IsOutdated(TEXT("M_OnyxMirror"), TEXT("MirrorArea")) || (Existing && Existing->bIsBlendable));
		if (!Material)
		{
			return;
		}
		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = BL_SceneColorBeforeDOF;
		Material->BlendablePriority = 10; // after the 2D lighting composite (priority 0)
		// Every floor on the screen draws its own reflection. (Blendable, Unreal would mix all the floors' instances of it
		// into one pass with one floor's stretch.)
		Material->bIsBlendable = false;
		FGraph Graph{ Material };
		UMaterialExpressionScreenPosition* Screen = Graph.Add<UMaterialExpressionScreenPosition>(-1400, 0);
		UMaterialExpressionVectorParameter* Rect = Graph.Vector(TEXT("MirrorArea"), FLinearColor(0.f, 0.f, 2.f, 0.f), -1400, 200);
		UMaterialExpressionCustom* Where = AddCustom(Graph,
			TEXT("float L = Rect.x, R = Rect.y, S = Rect.z, D = Rect.w;\n")
			TEXT("float inside = step(L, UV.x) * step(UV.x, R) * step(S, UV.y) * step(UV.y, S + D) * step(1e-5, D);\n")
			TEXT("float t = saturate((UV.y - S) / max(D, 1e-5));\n")
			TEXT("float2 m = float2(UV.x, 2.0 * S - UV.y);\n")
			TEXT("float k = inside * saturate(Strength) * pow(1.0 - t, max(Falloff, 0.01)) * step(0.0, m.y);\n")
			TEXT("return float4(m, k, 0.0);"),
			TEXT("Onyx Mirror"), { TEXT("UV"), TEXT("Rect"), TEXT("Strength"), TEXT("Falloff") });
		Graph.Link(Screen, TEXT("ViewportUV"), Where, TEXT("UV"));
		Graph.Link(Rect, TEXT("RGBA"), Where, TEXT("Rect")); // all four values (its first output is only RGB)
		Graph.Link(Graph.Scalar(TEXT("Strength"), 0.5f, -1400, 400), FString(), Where, TEXT("Strength"));
		Graph.Link(Graph.Scalar(TEXT("Falloff"), 1.5f, -1400, 500), FString(), Where, TEXT("Falloff"));
		UMaterialExpressionSceneTexture* Scene = Graph.Add<UMaterialExpressionSceneTexture>(-800, -200);
		Scene->SceneTextureId = PPI_PostProcessInput0;
		UMaterialExpressionSceneTexture* Mirrored = Graph.Add<UMaterialExpressionSceneTexture>(-800, 100);
		Mirrored->SceneTextureId = PPI_PostProcessInput0;
		Mirrored->bFiltered = false; // point sampled: the pixel art stays crisp
		// Its coordinates are viewport UV, like the screen position's: Unreal maps them onto the input's own buffer.
		Mirrored->Coordinates.Connect(0, Graph.Mask(Where, true, true, false, false, -1000, 100));
		UMaterialExpressionCustom* Blend = AddCustom(Graph,
			TEXT("return lerp(Scene.rgb, Reflected.rgb * Tint.rgb, saturate(Amount));"),
			TEXT("Onyx Mirror Blend"), { TEXT("Scene"), TEXT("Reflected"), TEXT("Amount"), TEXT("Tint") });
		Blend->OutputType = CMOT_Float3;
		Graph.Link(Scene, TEXT("Color"), Blend, TEXT("Scene"));
		Graph.Link(Mirrored, TEXT("Color"), Blend, TEXT("Reflected"));
		Graph.Link(Graph.Mask(Where, false, false, true, false, -1000, 300), FString(), Blend, TEXT("Amount"));
		Graph.Link(Graph.Vector(TEXT("Tint"), FLinearColor(0.9f, 0.86f, 0.8f), -1000, 450), FString(), Blend, TEXT("Tint"));
		Graph.Out(Blend, FString(), MP_EmissiveColor);
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
		UMaterial* Material = BeginMaterial(TEXT("PP_OnyxLight2D"), bRebuild || IsOutdated(TEXT("PP_OnyxLight2D"), TEXT("HighlightKnee"))
			|| IsOutdated(TEXT("PP_OnyxLight2D"), TEXT("HaloStrength")));
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
			TEXT("Ambient"), TEXT("SunColor"), TEXT("SunDirection"), TEXT("NormalStrength"), TEXT("Aspect"), TEXT("Softness"), TEXT("OwnGlowTex"),
			TEXT("Guard"), TEXT("Guarding"), TEXT("Knee"), TEXT("HighlightKnee"), TEXT("HaloTex"), TEXT("HaloStrength") })
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
		Graph.Link(Buffer(TEXT("OwnGlowBuffer"), -1000, 1350), FString(), Custom, TEXT("OwnGlowTex"));
		Graph.Link(Buffer(TEXT("HaloBuffer"), -1000, 2150), FString(), Custom, TEXT("HaloTex"));
		Graph.Link(Graph.Scalar(TEXT("HaloStrength"), 1.f, -800, 2300), FString(), Custom, TEXT("HaloStrength"));
		Graph.Link(SceneTexture(PPI_Metallic, -1000, 1550), TEXT("Color"), Custom, TEXT("Guard"));
		Graph.Link(Graph.Scalar(TEXT("LightGuarding"), 1.f, -800, 1700), FString(), Custom, TEXT("Guarding"));
		// Where a fully guarded sprite's brightness starts rolling off.
		Graph.Link(Graph.Scalar(TEXT("LightGuardKnee"), 0.5f, -800, 1850), FString(), Custom, TEXT("Knee"));
		// Where every lit sprite's brightness starts rolling off (1: clips as before).
		Graph.Link(Graph.Scalar(TEXT("HighlightKnee"), 0.75f, -800, 2000), FString(), Custom, TEXT("HighlightKnee"));
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
	BuildRain(bRebuild, Built);
	BuildFog(bRebuild, Collection, Built);
	BuildFogVolume(bRebuild, Collection, Placeholder, Built);
	BuildLightHalo(bRebuild, Built);
	BuildAfterimage(bRebuild, Built);
	BuildEffectSprite(bRebuild, Built);
	BuildMirror(bRebuild, Built);
	BuildComposite(bRebuild, Placeholder, Built);
	return Built;
}

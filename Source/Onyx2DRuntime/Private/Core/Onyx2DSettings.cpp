// Copyright Onyx2D. All Rights Reserved.

#include "Core/Onyx2DSettings.h"
#include "Materials/MaterialInterface.h"

UOnyx2DSettings::UOnyx2DSettings()
{
	LitSpriteMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Onyx2D/Materials/M_OnyxLitSprite.M_OnyxLitSprite")));
	SolidColorMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Onyx2D/Materials/M_OnyxSolid.M_OnyxSolid")));
	GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Onyx2D/Materials/M_OnyxGlow.M_OnyxGlow")));
	LightCompositeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Onyx2D/Materials/PP_OnyxLight2D.PP_OnyxLight2D")));
	LightingParameters = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Onyx2D/Materials/MPC_OnyxLighting.MPC_OnyxLighting")));
	LightHaloMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Onyx2D/Materials/M_OnyxLightHalo.M_OnyxLightHalo")));
}

// Shared part of basic shader and cubemap shader(Sky.hlsl)

//  set 3 directional lights
#ifndef DIR_LIGHTS
#define DIR_LIGHTS  3   
#endif

#ifndef POINT_LIGHTS
#define POINT_LIGHTS    0
#endif

#ifndef SPOT_LIGHTS
#define NUM_POST_LIGHTS 0
#endif

#include "IlluminationUtils.hlsl"

Texture2D gDiffuseMap       :   register(t0);

TextureCube gCubeMap        :   register(t1);

// for static samplers
SamplerState gsamPointWrap          :   register(s0);
SamplerState gsamPointClamp         :   register(s1);
SamplerState gsamLinearWrap         :   register(s2);
SamplerState gsamAnisotropicWrap    :   register(s4);
SamplerState gsamAnisotropicClamp   :   register(s5);

// constant buffer for a rendering objec;
cbuffer cbObject        :   register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer cbCommon        :   register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gCameraPosW;
    float cbObjectPad0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    
    float4 gFogColor;

    LightProperty gLights[Lights];
};

// object(material)'s lighting characteristics
cbuffer cbMaterial      :   register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};












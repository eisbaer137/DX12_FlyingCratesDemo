// Sky.hlsl for rendering far-sighted scene

#include "Shared.hlsl"

struct VertexIn
{
    float3 PosL     :   POSITION;
    float3 NormalL  :   NORMAL;
    float2 TexC     :   TEXCOORD;
};

struct VertexOut
{
    float4 PosH     :   SV_POSITION;
    float3 PosL     :   POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // use local vertex position as cubemap lookup vector.
    vout.PosL = vin.PosL;

    // transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    // sky sphere is always centered at the camera position. (set camera position as the origin)
    posW.xyz += gCameraPosW;

    // set z=w so that z/w = 1, makes the sky sphere always appeared at far plane.
    vout.PosH = mul(posW, gViewProj).xyww;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}










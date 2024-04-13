// BasicShader.hlsl

#include "Shared.hlsl"

struct VertexIn
{
    float3 PosL         :   POSITION;
    float3 NormalL      :   NORMAL;
    float2 TexC         :   TEXCOORD;
};

struct VertexOut
{
    float4 PosH         :   SV_POSITION;
    float3 PosW         :   POSITION;
    float3 NormalW      :   NORMAL;
    float2 TexC         :   TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    // transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // suppose the world matrix is orthogonal matrix
    // so that normal vector can easily be transformed into world space 
    // by simply Applying the world matrix
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // diffuse albedo associated with texture property
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;

    pin.NormalW = normalize(pin.NormalW);

    // vector from surface point to the camera in world space
    float3 toCameraW = gCameraPosW - pin.PosW; 
    float distToCamera = length(toCameraW);  
    toCameraW /= distToCamera;  // normalized
    
    // ambient light 
    float4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    ObjectProperty objProp = {diffuseAlbedo, gFresnelR0, shininess};
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, objProp, pin.PosW,
                            pin.NormalW, toCameraW, shadowFactor);

    float4 litColor = ambient + directLight;

    // add in specular reflections.
    float3 r = reflect(-toCameraW, pin.NormalW);
    float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
    float3 fresnelFactor = SchlickFresnel(gFresnelR0, pin.NormalW, r);
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;

    litColor.a = diffuseAlbedo.a;

    return litColor;
}








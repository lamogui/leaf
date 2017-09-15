cbuffer StandardMaterialData : register(b1)
{
    float3 baseColorMultiplier;
    float metallicOffset;
    float roughnessOffset;
    float3 emissive;
};

Texture2D<float4> baseColorMap: register(t0);
SamplerState baseColorSampler : register(s0);

Texture2D<float4> normalMap: register(t1);
SamplerState normalSampler : register(s1);

Texture2D<float4> metallicMap: register(t2);
SamplerState metallicSampler : register(s2);

Texture2D<float4> roughnessMap: register(t3);
SamplerState roughnessSampler : register(s3);

struct STANDARD_PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 viewPosition : POSITION1;
    float3 marchingStep : POSITION2;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 clipPosition : TEXCOORD1;
};

struct STANDARD_PS_OUTPUT
{
    float4 radiance : SV_TARGET0;
    float4 motion : SV_TARGET1;
};

#define MARCHING_ITERATIONS 64

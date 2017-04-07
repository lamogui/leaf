cbuffer StandardMaterialData : register(b1)
{
    float3 baseColorMultiplier;
    float metallicOffset;
    float roughnessOffset;
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
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct STANDARD_PS_OUTPUT
{
    float4 radiance : SV_TARGET;
};
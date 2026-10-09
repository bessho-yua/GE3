#include "Object3d.hlsli"

cbuffer MaterialBuffer : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float3 gMaterialPadding;
    float4x4 gUvTransform;
};

cbuffer DirectionalLightBuffer : register(b1)
{
    float4 gLightColor;
    float3 gLightDirection;
    float gLightIntensity;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(
        float4(input.texcoord, 0.0f, 1.0f),
        gUvTransform
    );

    float4 textureColor =
        gTexture.Sample(gSampler, transformedUV.xy);

    if (gEnableLighting != 0)
    {
        float3 normal = normalize(input.normal);
        float3 lightDirection =
            normalize(-gLightDirection);

        float NdotL = dot(normal, lightDirection);

        float lighting =
            pow(NdotL * 0.5f + 0.5f, 2.0f);

        output.color.rgb = gMaterialColor.rgb * textureColor.rgb * gLightColor.rgb * lighting * gLightIntensity;
        output.color.a = gMaterialColor.a * textureColor.a;
    }
    else
    {
        output.color =
            gMaterialColor *
            textureColor;
    }

    return output;
}
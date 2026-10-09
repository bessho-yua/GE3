#include "Object3d.hlsli"

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    output.position = mul(input.position, gWVP);
    output.texcoord = input.texcoord;

    output.normal = normalize(
        mul(input.normal, (float3x3) gWorld)
    );

    return output;
}
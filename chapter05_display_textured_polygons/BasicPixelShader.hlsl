#include "BasicShaderHeader.hlsli"

float4 BasicPS(Output input) : SV_Target
{
    return float4(tex.Sample(samplerState, input.uv));
}

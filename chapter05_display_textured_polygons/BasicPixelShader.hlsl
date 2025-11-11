struct VSOut {
    float4 svpos : SV_POSITION;
    float4 pos : POSITION;
};

float4 BasicPS(VSOut input) : SV_Target
{
    return float4((float2(0,1) + input.pos.xy) * 0.5f, 1.0, 1.0);
}

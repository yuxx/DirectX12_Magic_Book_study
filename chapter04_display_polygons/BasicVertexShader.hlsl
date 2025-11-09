struct VSOut {
    float4 svpos : SV_POSITION;
    float4 pos : POSITION;
};

VSOut BasicVS(float4 pos : POSITION)
{
    VSOut output;
    output.svpos = pos;
    output.pos = pos;
    return output;
}
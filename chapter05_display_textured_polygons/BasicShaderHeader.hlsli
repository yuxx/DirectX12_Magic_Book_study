struct Output
{
    // システム用頂点座用
    float4 svpos : SV_POSITION;
    // uv 値
    float2 uv : TEXCOORD;
};

// 0番スロットに設定されたテクスチャ
Texture2D<float4> tex : register(t0);
// 0番スロットに設定されたサンプラー
SamplerState samplerState : register(s0);
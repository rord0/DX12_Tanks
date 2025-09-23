struct PixelShaderInput
{
    float2 UV : UV;
    float3 color : COLOR;
    float  fill : FILL;
};

float rectangle(float2 uv, float w)
{
    float2 bl = step(float2(w, w), uv);
    float pct = bl.x * bl.y;
    float2 tr = step(float2(w, w), 1.0f - uv);
    pct *= tr.x * tr.y;
    return 1 - pct;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float4 pixelColor = {IN.color.xyz, rectangle(IN.UV, 0.05f)};
    return pixelColor;
}

struct PixelShaderInput
{
    float2 UV: UV;
};

Texture2D<float4> texture1 : register(t0);
sampler textureSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    float2 uvScale = float2(2*0.125f, 2*0.125f);
    float2 uvOffset = float2(5 * 0.125f, 3 * 0.125f);

    //uv = (uv * uvScale) + uvOffset;
    float4 texel = texture1.Sample(textureSampler, float2(uv));

    float4 pixelColor = texel;
    return pixelColor;
}
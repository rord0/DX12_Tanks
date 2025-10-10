
struct PixelShaderInput
{
    float2 UV: UV;
    uint instanceTextureID: InstanceTextureID;
    float4 uvTransform : UVTransform;
};

Texture2D<float4> texture1[32] : register(t0);
sampler textureSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    float2 uvScale = IN.uvTransform.xy;
    float2 uvOffset = IN.uvTransform.zw;

    float2 atlasUV = (uv * uvScale) + uvOffset;
    float4 texel = texture1[IN.instanceTextureID].Sample(textureSampler, float2(atlasUV));

    float4 pixelColor = texel;
    return pixelColor;
}
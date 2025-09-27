struct PixelShaderInput
{
    float3 color : COLOR;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float4 pixelColor = {IN.color, 1.0f};
    return pixelColor;
}
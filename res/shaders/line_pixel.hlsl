struct PixelShaderInput
{
    float3 color : COLOR;
};

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float4 pixelColor = {IN.color, 0.5f};
    return pixelColor;
}

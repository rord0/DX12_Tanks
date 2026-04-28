struct PixelShaderInput
{
    float4 color : COLOR;
};

float4 PSmain(PixelShaderInput IN) : SV_TARGET
{
    float4 pixelColor = {IN.color};
    return pixelColor;
}

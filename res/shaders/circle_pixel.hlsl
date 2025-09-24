struct PixelShaderInput
{
    float2 UV : UV;
    float3 color : COLOR;
    float  fill : FILL;
};

float circle(float2 uv, float f)
{
    const float2 center = float2(0.5f, 0.5f);
    float smooth = 0.01f;
    float radius = 0.5f - smooth;
    f = (f/2);

    float2 dist = uv - center;

    float2 outer = 1 - smoothstep(radius - (radius*smooth), radius + (radius*smooth), length(dist)); 
    float2 inner = smoothstep((f) - (f * smooth), f + (f * smooth), length(dist));
    return outer * inner;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float4 pixelColor = {IN.color.rgb, circle(IN.UV, IN.fill)};
    return pixelColor;
}
#define SMOOTH(r,R) (1.0-smoothstep(R-0.005,R+0.005, r))
#define M_PI 3.1415926535897932384626433832795

struct PixelShaderInput
{
    float2 UV: UV;
};

Texture2D<float4> texture1 : register(t0);
sampler textureSampler : register(s0);

float circle(float2 uv, float2 center, float radius)
{
    float r = length(uv - center);
    return SMOOTH(r, radius);
}

float line2(float2 uv, float2 center, float radius)
{
    //angle of the line
    float theta0 = -90.0;
    float2 d = uv - center;
    float r = sqrt( dot( d, d ) );
    if(r<radius)
    {
        //compute the distance to the line theta=theta0
        float2 p = radius * float2(cos(theta0 * M_PI / 180.0), -sin(theta0 * M_PI / 180.0));
        float l = length(d - p*clamp(dot(d,p)/ dot(p,p), 0.0, 1.0));

        return SMOOTH(l,1.0);
    }
    else return 0.0;
}

float4 main(PixelShaderInput IN) : SV_TARGET
{
    float2 uv = IN.UV;
    float2 uvScale = float2(2*0.125f, 2*0.125f);
    float2 uvOffset = float2(5 * 0.125f, 3 * 0.125f);

    uv = (uv * uvScale) + uvOffset;
    float4 texel = texture1.Sample(textureSampler, float2(uv));

    // Circle
    float2 center = {0.5f, 0.5f};
    float3 color = {0.0f, 0.0f, 0.0f};
    // color += circle(uv, center, 0.5f) * float3(1.0f, 1.0f, 1.0f);

    float4 pixelColor = {texel.rgba};
    return pixelColor;
}
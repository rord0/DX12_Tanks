struct PixelShaderInput
{
    float2 UV : UV;
    float3 color : COLOR;
    float  fill : FILL;
};

#define M_PI 3.1415926535897932384626433832795
#define SMOOTH(r,R) (1.0-smoothstep(R-0.005,R+0.005, r))
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

float circle(float2 uv, float f)
{
    f = 1.0 - f;
    const float2 center = float2(0.5f, 0.5f);
    float smooth = 0.01f;
    float radius = 0.5f - smooth;
    f = (f/2);

    float2 dist = uv - center;

    float2 outer = 1 - smoothstep(radius - (radius*smooth), radius + (radius*smooth), length(dist)); 
    float2 inner = smoothstep((f) - (f * smooth), f + (f * smooth), length(dist));
    return outer * inner;
}

float4 PSmain(PixelShaderInput IN) : SV_TARGET
{
    float4 pixelColor = {IN.color.rgb, circle(IN.UV, IN.fill)};
    return pixelColor;
}

struct ModelViewProjection
{
    matrix MVP;
};
ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VSInput
{
    float2 pos          : Position;
    float2 size         : Size;
    float4 fillColor    : FillColor;
    float4 strokeColor  : StrokeColor;
    float  cornerRadius : CornerRadius;
    float  rotation		: Rotation;
    float  strokeWidth  : StrokeWidth;
    uint   vertexID     : SV_VertexID;
};

struct VSOutput
{
    float4 pos          : SV_Position;
    float4 fillColor    : FILL_COLOR;
    float4 strokeColor  : STROKE_COLOR;
    float2 localUV      : LOCAL_UV;       // pixel position relative to rect center
    float2 halfSize     : HALF_SIZE;      // half-extents of the rect
    float  cornerRadius : CORNER_RADIUS;
    float  rotation		: ROTATION;
    float  strokeWidth  : STROKE_WIDTH;
};

VSOutput VSmain(VSInput input)
{
    VSOutput output;

    // Which corner this vertex represents
    float x = (input.vertexID & 1) == 0 ? input.pos.x : input.pos.x + input.size.x;
    float y = (input.vertexID & 2) == 0 ? input.pos.y : input.pos.y + input.size.y;

    output.pos          = mul(ModelViewProjectionCB.MVP, float4(x, y, 0.0, 1.0));
    output.fillColor    = input.fillColor;
	output.strokeColor  = input.strokeColor;
	output.strokeWidth  = input.strokeWidth;

    // Center of the triangle in local space
    float2 center   = input.pos + input.size * 0.5;
    output.localUV  = float2(x, y) - center;   // ranges from -halfSize to +halfSize
    output.halfSize = input.size * 0.5;
    output.rotation = input.rotation;
    output.cornerRadius = input.cornerRadius;

    return output;
}

float sdEquilateralTriangle(in float2 p, in float r, in float a)
{
    float c = cos(a), s = sin(a);
    p = mul(float2x2(c, s, -s, c), p);

    const float k = sqrt(3.0);
    p.x = abs(p.x);
    p -= float2(0.5, 0.5 * k) * max(p.x + k * p.y, 0.0);
    p -= float2(clamp(p.x, -0.5 * r * k, 0.5 * r * k), -0.5 * r);
    return length(p) * sign(-p.y);
}

float4 PSmain(VSOutput input) : SV_Target
{
    // fit r to the smaller constraint so it doesn't overflow either axis
    float r = min(input.halfSize.x, input.halfSize.y);

    // recenter: shift uv so the triangle's bbox center sits at quad center
    float2 uv = input.localUV;

	float cornerRadius = input.cornerRadius;

    float d = sdEquilateralTriangle(uv, r - cornerRadius, input.rotation) - cornerRadius;
    float aa = fwidth(d) * 0.5;

	float strokeWidth = input.strokeWidth;
    float fillAlpha = 1.0 - smoothstep(-aa, aa, d);
    float strokeAlpha = smoothstep(-strokeWidth - aa, -strokeWidth + aa, d) * (1.0 - smoothstep(-aa, aa, d));

    float4 color;
    color.rgb = lerp(input.fillColor.rgb, input.strokeColor.rgb, strokeAlpha);
    color.a   = fillAlpha * lerp(input.fillColor.a, input.strokeColor.a, strokeAlpha);
    return color;
}

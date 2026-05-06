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

    // Center of the rect in local space
    float2 center   = input.pos + input.size * 0.5;
    output.localUV  = float2(x, y) - center;   // ranges from -halfSize to +halfSize
    output.halfSize = input.size * 0.5;
    output.cornerRadius = input.cornerRadius;

    return output;
}

float4 PSmain(VSOutput input) : SV_Target
{
    // Rounded-rect SDF
    // Move to the "corner quadrant" and subtract the corner radius
    float2 q = abs(input.localUV) - input.halfSize + input.cornerRadius;

    // Standard exterior/interior SDF for a rounded box
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - input.cornerRadius;

    // Anti-aliased edge — fwidth gives 1 px worth of screen-space derivative
    float aa = fwidth(d) * 0.5;
	float half_sw = 6.0;

    float fillAlpha = 1.0 - smoothstep(-aa, aa, d);

    float strokeAlpha = smoothstep(-half_sw - aa, -half_sw + aa, d) * (1.0 - smoothstep(-aa, aa, d));

    float4 color;
    color.rgb = lerp(input.fillColor.rgb, input.strokeColor.rgb, strokeAlpha);
    color.a   = fillAlpha * lerp(input.fillColor.a, input.strokeColor.a, strokeAlpha);
    return color;
}

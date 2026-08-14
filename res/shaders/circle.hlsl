struct ProjectionData
{
    matrix VP;
    matrix proj;
};
ConstantBuffer<ProjectionData> ProjectionDataCB : register(b0);

struct VSInput
{
    float2 pos          : Position;
    float2 size         : Size;
	float4 strokeColor	: StrokeColor;
	float4 outerColor	: OuterColor;
	float4 innerColor	: InnerColor;
	float  strokeWidth	: StrokeWidth;
    uint   vertexID     : SV_VertexID;
};

struct VSOutput
{
    float4 pos          : SV_Position;
    float2 uv			: UV;
	float4 strokeColor  : STROKE_COLOR;
	float4 outerColor	: OUTER_COLOR;
	float  strokeWidth  : STROKE_WIDTH;
    float  radius       : RADIUS;
};

float4 PSmain(VSOutput input) : SV_Target
{
    float distFromCenter = length(input.uv);

    // Signed distance from the circle's edge (negative = inside)
    float dist = distFromCenter - input.radius;

    // Anti-aliasing width based on screen-space derivatives
    float aa = fwidth(dist);

    // Base circle mask (1 inside, 0 outside, anti-aliased edge)
    float circleMask = 1.0 - saturate(dist / aa + 0.5);

    // Radial alpha falloff: 0 at center, 1 by fadeDistance
    // Smaller fadeFraction = faster fade (reaches full opacity closer to center)
    float innerRadius = input.radius * 0.85;
    float radialAlpha = smoothstep(innerRadius, input.radius, distFromCenter);

    // Stroke ring: band of strokeWidth thickness sitting just inside the edge
    float strokeDist = abs(dist + input.strokeWidth * 0.5) - input.strokeWidth * 0.5;
    float strokeMask = 1.0 - saturate(strokeDist / aa + 0.5);

	float4 strokeColor = input.strokeColor;
	strokeColor.a *= strokeMask;

    float4 fill = input.outerColor;
    fill.a *= circleMask * radialAlpha;

    // Fill with outerColor, then overlay stroke near the edge
	float4 color;
    color.a = strokeColor.a + fill.a * (1.0 - strokeColor.a);
    color.rgb = (strokeColor.rgb * strokeColor.a + fill.rgb * fill.a * (1.0 - strokeColor.a)) / max(color.a, 0.0001);

    return color;
}

VSOutput VSmain(VSInput input)
{
    VSOutput output;

    // Which corner this vertex represents
    float x = (input.vertexID & 1) == 0 ? input.pos.x - input.size.x / 2.0 : input.pos.x + input.size.x / 2.0;
    float y = (input.vertexID & 2) == 0 ? input.pos.y - input.size.y / 2.0 : input.pos.y + input.size.y / 2.0;

    float2 uv;
	// uv.x = (input.vertexID & 1) == 0 ? 0 : 1;
	// uv.y = (input.vertexID & 2) == 0 ? 1 : 0;
    uv = float2(x - input.pos.x, y - input.pos.y);

    output.pos = mul(ProjectionDataCB.VP, float4(x, y, 0.0, 1.0));
	output.uv = uv;
	output.strokeColor = input.strokeColor;
	output.strokeWidth = input.strokeWidth;
	output.outerColor = input.outerColor;
	output.radius = min(input.size.x, input.size.y) * 0.5;

    return output;
}

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
	float  alpha		: Alpha;
	float  offset		: Offset;
    uint   vertexID     : SV_VertexID;
};

struct VSOutput
{
    float4 pos          : SV_Position;
    float2 uv			: UV;
	float  offset 		: OFFSET;
};

float2 RotateUV(float2 uv, float angle)
{
    float rad = radians(angle);
    float s = sin(rad);
    float c = cos(rad);

    float2 rotatedUV;
    rotatedUV.x = uv.x * c - uv.y * s;
    rotatedUV.y = uv.x * s + uv.y * c;

	return rotatedUV;
}

float4 PSmain(VSOutput input) : SV_Target
{
	float2 uv = RotateUV(input.uv, -45);
	uv.y += input.offset;
	
    float stripeWidth   = 0.025f;
    float stripeSpacing = 0.075f;

    // Repeat pattern
    float pattern = frac(uv.y / stripeSpacing);

    // Distance from stripe center, wrapped 0..0.5
    float stripeHalfWidth = (stripeWidth / stripeSpacing) * 0.5f;
    float dist = abs(pattern - 0.5f);

    // Smooth edge instead of a hard step, for anti-aliasing
    float lineMask = 1.0f - smoothstep(stripeHalfWidth - 0.01, stripeHalfWidth + 0.01, dist);

    float4 col = float4(1.0, 0,0,1.0);
    col.a *= lineMask;
    col.a *= 0.2;

    return col;
}

VSOutput VSmain(VSInput input)
{
    VSOutput output;

    // Which corner this vertex represents
    float x = (input.vertexID & 1) == 0 ? input.pos.x : input.pos.x + input.size.x;
    float y = (input.vertexID & 2) == 0 ? input.pos.y : input.pos.y + input.size.y;

    float2 uv = float2(x, y);
	// uv.x = (input.vertexID & 1) == 0 ? 0 : 1;
	// uv.y = (input.vertexID & 2) == 0 ? 1 : 0;

    output.uv = uv;
    output.pos = mul(ProjectionDataCB.VP, float4(x, y, 0.0, 1.0));
	output.offset = input.offset;

    return output;
}

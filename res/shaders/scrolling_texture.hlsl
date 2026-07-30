struct ProjectionData
{
    matrix VP;
    matrix proj;
};
ConstantBuffer<ProjectionData> ProjectionDataCB : register(b0);

Texture2D<float4> texture1[32] : register(t0);
sampler textureSampler : register(s0);
sampler textureSamplerWrap : register(s1);

struct VSInput
{
    float2 pos          : Position;
    float2 size         : Size;
    float2 uvOffset		: UVOffset;
    float2 tilingAmount : TilingAmount;
    uint   textureID    : TextureID;
    uint   vertexID     : SV_VertexID;
};

struct VSOutput
{
    float4 pos          : SV_Position;
    float2 uv			: UV;
    uint   textureID    : TEXTURE_ID;
};

float4 PSmain(VSOutput input) : SV_Target
{

    float4 texel = texture1[input.textureID].Sample(textureSamplerWrap, input.uv);
    float4 color = float4(texel.r, texel.g, texel.b, 1.0);
	return color;
}

VSOutput VSmain(VSInput input)
{
    VSOutput output;

    // Which corner this vertex represents
    float x = (input.vertexID & 1) == 0 ? input.pos.x : input.pos.x + input.size.x;
    float y = (input.vertexID & 2) == 0 ? input.pos.y : input.pos.y + input.size.y;

    // Generate UV in [0,1] range based on vertex ID
    float2 uv;
	uv.x = (input.vertexID & 1) == 0 ? -input.tilingAmount.x * 0.5 : input.tilingAmount.x * 0.5;
	uv.y = (input.vertexID & 2) == 0 ? input.tilingAmount.y * 0.5 : -input.tilingAmount.y * 0.5;
	uv += input.uvOffset;

    output.uv = uv;
    output.pos = mul(transpose(ProjectionDataCB.proj), float4(x, y, 0.0, 1.0));
	output.textureID = input.textureID;

    return output;
}

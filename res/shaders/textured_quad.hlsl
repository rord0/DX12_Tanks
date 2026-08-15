struct ProjectionData
{
    matrix VP;
    matrix proj;
};
ConstantBuffer<ProjectionData> ProjectionDataCB : register(b0);

Texture2D<float4> texture1[32] : register(t0);
sampler textureSampler : register(s0);

struct VSInput
{
    float3 position : Position;
    float2 UV : UV;

    float4x4 model	 : InstanceModel;
    float  alpha	 : InstanceAlpha;
    uint   textureID : InstanceTextureID;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 UV		: UV;
    float alpha	    : ALPHA;
    uint textureID  : TEXTURE_ID;
};

VSOutput VSmain(VSInput input)
{
    VSOutput output;

    float4x4 MVP = mul(ProjectionDataCB.VP, input.model);
    output.position = mul(MVP, float4(input.position, 1));
	output.textureID = input.textureID;
	output.alpha = input.alpha;
	output.UV = input.UV;

	return output;
}

float4 PSmain(VSOutput input) : SV_TARGET
{
    float4 texel = texture1[input.textureID].Sample(textureSampler, input.UV);

    float4 pixelColor = texel * input.alpha;

    return pixelColor;
}

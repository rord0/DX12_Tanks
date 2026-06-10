struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

Texture2D<float4> texture1[32] : register(t0);
sampler textureSampler : register(s0);

struct VSInput
{
    float4 bounds      : Bounds;
    float4 uv          : UV;
    float4 color       : Color;
	float  strokeWidth : StrokeWidth;
    uint   textureID   : InstanceTextureID;
    uint   vertexID    : SV_VertexID;
};

struct VSOutput
{
    float4 pos       : SV_Position;
    float2 uv        : TEXCOORD0;
    float4 color     : COLOR;
	float strokeWidth : STROKE_WIDTH;
    uint   textureID : TEXCOORD1;
};

VSOutput VSmain(VSInput input)
{
    VSOutput output;

    float x = (input.vertexID & 1) == 0 ? input.bounds.x : input.bounds.z;
    float y = (input.vertexID & 2) == 0 ? input.bounds.y : input.bounds.w;

	float u = (input.vertexID & 1) == 0 ? input.uv.x : input.uv.z;
	float v = (input.vertexID & 2) == 0 ? input.uv.w : input.uv.y;

    output.pos   = mul(ModelViewProjectionCB.MVP, float4(x, y, 0.0, 1.0));
    output.uv    = float2(u, v);
    output.color = input.color;
	output.strokeWidth = input.strokeWidth;
    output.textureID = input.textureID;

    return output;
}

float SDFStep(float dist, float edge, float softness)
{
    return smoothstep(edge - softness, edge + softness, dist);
}

float4 PSmain(VSOutput input) : SV_Target
{
    float dist = texture1[input.textureID].Sample(textureSampler, input.uv).r;

    float sharpness     = 10.0;
    float4 strokeColor = float4(0.12941176470588237, 0.1411764705882353, 0.1450980392156863, 1.0);

    float alpha        = clamp((dist - 0.5) * sharpness + 0.5, 0.0, 1.0);
    float outlineAlpha = clamp((dist - 0.5 + input.strokeWidth) * sharpness + 0.5, 0.0, 1.0);

    float4 color = lerp(strokeColor, input.color, alpha);
    color.a = outlineAlpha;

    return color;
}

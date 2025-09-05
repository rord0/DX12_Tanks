struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexPosUV
{
    float3 Position : Position;
    float2 UV : UV;
};

struct VertexShaderOutput
{
	float2 UV : UV;
    float4 Position : SV_Position;
};

VertexShaderOutput main(VertexPosUV IN)
{
    VertexShaderOutput OUT;

    OUT.Position = float4(IN.Position, 1.0f);
    OUT.UV = IN.UV;

    return OUT;
}
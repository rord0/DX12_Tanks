struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexData
{
    float3 Position : Position;
    float2 UV : UV;
    float3 instancePosition : InstancePosition;
    float2 instanceSize : InstanceSize;
    float  rotationZ : InstanceRotZ;
};

struct VertexShaderOutput
{
	float2 UV : UV;
    float4 Position : SV_Position;
};

VertexShaderOutput main(VertexData IN)
{
    VertexShaderOutput OUT;

    OUT.Position = float4(IN.Position + IN.instancePosition, 1.0f);
    OUT.UV = IN.UV;

    return OUT;
}
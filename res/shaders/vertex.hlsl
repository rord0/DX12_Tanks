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
    float4x4 translation = {
        1, 0, 0, IN.instancePosition.x,
        0, 1, 0, IN.instancePosition.y,
        0, 0, 1, IN.instancePosition.z,
        0, 0, 0, 1
    };

    float4x4 rotationX = {
        cos(IN.rotationZ), -sin(IN.rotationZ), 0, 0,
        sin(IN.rotationZ), cos(IN.rotationZ), 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    float4x4 scale = {
        IN.instanceSize.x, 0, 0, 0,
        0, IN.instanceSize.y, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };

    float4x4 modelMatrix = mul(translation, mul(rotationX, scale));
    float4x4 MVP = mul(ModelViewProjectionCB.MVP, modelMatrix);
    OUT.Position = mul(MVP, float4(IN.Position, 1));
    OUT.UV = IN.UV;

    return OUT;
}
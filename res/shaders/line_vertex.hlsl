struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexData
{
    float3 Position : Position;
    float3 posA: StartPos;
    float2 posB: EndPos;
    float3 instanceColor : InstanceColor;
    float  width : InstanceWidth;
};

struct VSOutput
{
	float3 Color : COLOR;
    float4 Position : SV_Position;
};

VSOutput main(VertexData IN)
{
    VSOutput OUT;
    
    float2 dir = IN.posB.xy - IN.posA.xy;
    float2 xBasis = normalize(dir);
    float2 yBasis = float2(-xBasis.y, xBasis.x);

    float2 offsetA = IN.posA.xy + (IN.width * ((IN.Position.x * xBasis) + (IN.Position.y * yBasis)));
    float2 offsetB = IN.posB.xy + (IN.width * ((IN.Position.x * xBasis) + (IN.Position.y * yBasis)));

    float2 newPos = lerp(offsetA, offsetB, IN.Position.z);

    OUT.Color = IN.instanceColor;
    OUT.Position = mul(ModelViewProjectionCB.MVP, float4(newPos.xy, 0.0f, 1.0f));
    return OUT;
}
struct VSInput {
    float3 pos;
    float3 color;
}

struct VSOutput {
    float4 pos: SV_Position;
    float3 color;
};

struct UniformBuffer {
    float4x4    model;
    float4x4    view;
    float4x4    projection;
};

ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.color = input.color;
    output.pos = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.pos, 1.0))));
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    float3 color = input.color;
    return float4(color, 1.0f);
}

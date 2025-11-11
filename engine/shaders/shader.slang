struct VSInput {
    float3 pos;
    float3 normal;
    float2 texture_coord;
}

struct VSOutput {
    float4 pos: SV_Position;
    float3 normal;
    float2 texture_coord;
};

struct GlobalUniformObject {
    float4x4    view;
    float4x4    projection;
    float4x4    padding_1;
    float4x4    padding_2;
};

struct LocalUniformObject {
    float4 diffuse_color;
    float4 padding_0;
    float4 padding_1;
    float4 padding_2;
};

struct PushConstantBlock {
    float4x4    model;    
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformObject> global_ubo;

[[vk::binding(0, 1)]]
ConstantBuffer<LocalUniformObject> object_ubo;

[[vk::binding(1, 1)]]
Sampler2D sampler;

[[vk::push_constant]]
ConstantBuffer<PushConstantBlock> push_constants;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.texture_coord = input.texture_coord;
    output.pos = mul(global_ubo.projection, mul(global_ubo.view, mul(push_constants.model, float4(input.pos, 1.0))));
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    float4 sampled_texture = sampler.Sample(input.texture_coord);
    // return float4(object_ubo.diffuse_color.xyz * sampled_texture.xyz, 1.0f);
    return sampled_texture;
}

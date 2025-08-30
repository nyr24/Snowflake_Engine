// static float2 positions[3] = float2[](
//     float2(0.0, -0.5),
//     float2(0.5, 0.5),
//     float2(-0.5, 0.5)
// );

// static float3 colors[3] = float3[](
//     float3(1.0, 0.0, 0.0),
//     float3(0.0, 1.0, 0.0),
//     float3(0.0, 0.0, 1.0)
// );

struct VSInput {
    float3 pos;
    float3 color;
}

struct VSOutput {
    float3 color;
    float4 pos: SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.color = input.color;
    output.pos = float4(input.pos, 1.0f);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    float3 color = input.color;
    return float4(color, 1.0);
}

// 纯色管线:只读顶点位置,颜色从 UBO 来。
//
// PushConstants 和 ObjectUbo 的布局必须和 texture.slang 完全一致 ——
// 两个 shader 共用 PipelineCache 里那唯一一条 pipeline layout,
// 而且描述符 layout 是所有 shader 的并集。

struct PushConstants {
    float4x4 viewProj;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct ObjectUbo {
    float4x4 model;
    float4   color;
};
[[vk::binding(0, 0)]] ConstantBuffer<ObjectUbo> ubo;

struct VSInput {
    float3 inPosition;
};

struct VSOutput {
    float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(pc.viewProj, mul(ubo.model, float4(input.inPosition, 1.0)));
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return ubo.color;
}

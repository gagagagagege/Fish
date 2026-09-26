// 贴图管线:顶点位置 + 纹理坐标,片元采样。
//
// PushConstants 和 ObjectUbo 的布局必须和 flat_color.slang 完全一致 ——
// 两个 shader 共用 PipelineCache 里那唯一一条 pipeline layout。
// 这里的 color 不读,但布局要对齐。
//
// set 1,不是 set 0 —— UBO 占着 set 0,而两个必须分开:
// UBO 每个物体都要换 dynamic offset 重绑(绑的是同一个 set,只是偏移不同),
// 贴图按 (view, sampler) 各自一个 set,换图时绑另一个 set。
// 挤在一个 set 里就没法各管各的 —— 换贴图等于改写那个 set,
// 而它可能正被在飞的命令缓冲引用。

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
    float2 inTexCoord;
};

struct VSOutput {
    float4 pos : SV_Position;
    float2 fragTexCoord;
};

[[vk::binding(0, 1)]] Sampler2D albedo;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(pc.viewProj, mul(ubo.model, float4(input.inPosition, 1.0)));
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return albedo.Sample(vertIn.fragTexCoord);
}

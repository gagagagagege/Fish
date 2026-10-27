// 贴图管线:顶点位置 + 纹理坐标,片元按 UBO 里的两个槽位从图像表和采样器表取。
//
// PushConstants 和 ObjectUbo 的布局必须和 flat_color.slang 完全一致 ——
// 两个 shader 共用 PipelineCache 里那唯一一条 pipeline layout。
// color 现在要读 —— 纯色项也走这条管线,绑一张 1×1 白贴图,采样恒为 1,
// 颜色全靠它给。
//
// set 1 是两张表:图像一张、采样器一张。shader 这侧是运行时数组,格数由 layout
// 那侧定;贴图随时创建、槽位由后端发号,所以是边走边往表里填。
// texId / samplerId 走 UBO 不走顶点属性 —— per-item 的顶点格式是 pos3+uv2
// (stride 20),塞不下更多属性,而 UBO 本来就是每物体一份、带 dynamic offset。

struct PushConstants {
    float4x4 viewProj;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct ObjectUbo {
    float4x4 model;
    float4   color;
    uint     texId;      // 图像表的槽位。纯色项不读它
    uint     samplerId;  // 采样器表的槽位
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

// 索引来自 UBO = 非一致,必须 NonUniformResourceIndex。
// 漏了不报错:驱动会按整个 subgroup 算一次描述符,采到隔壁物体的纹理。
[[vk::binding(0, 1)]] Texture2D<float4> textures[];
[[vk::binding(1, 1)]] SamplerState    samplers[];

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(pc.viewProj, mul(ubo.model, float4(input.inPosition, 1.0)));
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    // 纯色项绑白贴图,采样恒为 1,颜色全由 ubo.color 给;
    // 贴图项不设 color,拿的是 DrawItem 的默认值 vec4(1.0),乘上去不影响。
    return textures[NonUniformResourceIndex(ubo.texId)]
        .Sample(samplers[NonUniformResourceIndex(ubo.samplerId)], vertIn.fragTexCoord)
        * ubo.color;
}

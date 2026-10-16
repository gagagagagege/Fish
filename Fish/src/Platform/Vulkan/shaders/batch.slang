// 批处理管线。顶点自带世界坐标、颜色、uv、槽位和平铺系数,片元按槽位选贴图。
//
// 和 texture.slang 分成两个文件,不改那一个 —— per-item 那条路还在跑,它的顶点
// 是 pos3+uv2(stride 20),这里是 pos3+color4+uv2+texId+tiling(stride 44)。合到
// 一个文件就只能有一个 VSInput,而 BuildVertexInput 是按声明顺序做前缀和算
// offset 的,属性加起来 44 字节会把老路径的 stride 20 撑爆。
//
// 贴图走 set 1,和 per-item 那条路同一张表 —— 表只有一个,两条路共用。
//
// albedo 的索引来自顶点属性 = 非一致,必须 NonUniformResourceIndex。
// 漏了不报错:驱动会按整个 subgroup 算一次描述符,采到隔壁顶点的纹理。

struct PushConstants {
    float4x4 viewProj;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct VSInput {
    float3 inPosition;
    float4 inColor;
    float2 inTexCoord;
    uint   inTexId;
    float  inTilingFactor;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 fragColor;
    float2 fragTexCoord;
    // 整数不能插值,漏了 nointerpolation 每个像素拿到的是三个顶点槽位的混合值。
    nointerpolation uint fragTexId;
    float fragTilingFactor;
};

// 运行时数组:格数不写在这里,由 C++ 侧按设备上限定进 layout。
// 反射把运行时数组的 count 报成 0,所以消费方要看 runtimeArray 那个标记。
[[vk::binding(0, 1)]] Sampler2D albedo[];

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    // 变换已经在 CPU 侧烘进 inPosition 了,这里只乘 viewProj。
    output.pos = mul(pc.viewProj, float4(input.inPosition, 1.0));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.fragTexId = input.inTexId;
    output.fragTilingFactor = input.inTilingFactor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return albedo[NonUniformResourceIndex(vertIn.fragTexId)].Sample(vertIn.fragTexCoord * vertIn.fragTilingFactor)
         * vertIn.fragColor;
}

// 批处理管线。顶点自带世界坐标、颜色、uv、两个槽位和平铺系数,片元按槽位选贴图。
//
// 和 texture.slang 分成两个文件,不改那一个 —— per-item 那条路还在跑,它的顶点
// 是 pos3+uv2(stride 20),这里是 pos3+color4+uv2+texId+samplerId+tiling(stride 48)。
// 合到一个文件就只能有一个 VSInput,而 BuildVertexInput 是按声明顺序做前缀和算
// offset 的,属性加起来 48 字节会把老路径的 stride 20 撑爆。
//
// set 1 是两张表,和 per-item 那条路共用。
//
// 两个索引都来自顶点属性 = 非一致,必须 NonUniformResourceIndex。
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
    uint   inSamplerId;
    float  inTilingFactor;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 fragColor;
    float2 fragTexCoord;
    // 整数不能插值,漏了 nointerpolation 每个像素拿到的是三个顶点槽位的混合值。
    nointerpolation uint fragTexId;
    nointerpolation uint fragSamplerId;
    float fragTilingFactor;
};

// 运行时数组:格数不写在这里,由 C++ 侧按设备上限定进 layout。
// 反射把运行时数组的 count 报成 0,所以消费方要看 runtimeArray 那个标记。
[[vk::binding(0, 1)]] Texture2D<float4> textures[];
[[vk::binding(1, 1)]] SamplerState    samplers[];

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    // 变换已经在 CPU 侧烘进 inPosition 了,这里只乘 viewProj。
    output.pos = mul(pc.viewProj, float4(input.inPosition, 1.0));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.fragTexId = input.inTexId;
    output.fragSamplerId = input.inSamplerId;
    output.fragTilingFactor = input.inTilingFactor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return textures[NonUniformResourceIndex(vertIn.fragTexId)]
        .Sample(samplers[NonUniformResourceIndex(vertIn.fragSamplerId)],
                vertIn.fragTexCoord * vertIn.fragTilingFactor)
        * vertIn.fragColor;
}

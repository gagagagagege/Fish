// 3D 光照管线:位置 + 法线 + 贴图,片元做一个方向光的 Lambert + 环境光。
//
// PushConstants / ObjectUbo 必须和 texture.slang、flat_color.slang 逐字段对齐 ——
// 三条 shader 共用同一个 pipeline layout,而描述符 layout 只比 set/binding/type,
// 不比 block 大小。normalMatrix 追加在 ObjectUbo 末尾,两条老 shader 不声明它,
// 只读前 88B,布局仍然成立。
//
// 光照在世界空间做:N 由 normalMatrix 从模型空间变过来,L 是光传播方向取反。
// 颜色是线性值(交换链和贴图都是 sRGB,采样/写回由硬件转换)。

struct PushConstants {
    float4x4 viewProj;
    float4   lightDirection;  // xyz = 世界空间光传播方向,归一化
    float4   lightColor;      // rgb = 线性颜色,强度已乘
    float4   ambientColor;    // rgb = 线性环境光
    float4x4 lightViewProj;   // 光空间的 VP,算阴影坐标用
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct ObjectUbo {
    float4x4 model;
    float4   color;
    uint     texId;        // 图像表的槽位
    uint     samplerId;    // 采样器表的槽位
    float4x4 normalMatrix; // transpose(inverse(mat3(model)))
};
[[vk::binding(0, 0)]] ConstantBuffer<ObjectUbo> ubo;

struct VSInput {
    float3 inPosition;
    float2 inTexCoord;
    float3 inNormal;
};

struct VSOutput {
    float4 pos : SV_Position;
    float2 fragTexCoord;
    float3 fragNormal;    // 世界空间,插值后要归一化
    float4 lightClipPos;  // 光空间裁剪坐标,片元里除 w 再比较
};

// 索引来自 UBO = 非一致,必须 NonUniformResourceIndex。
[[vk::binding(0, 1)]] Texture2D<float4> textures[];
[[vk::binding(1, 1)]] SamplerState    samplers[];

// 阴影图是全局单张,不走 bindless 表。比较采样器由硬件做 ref <= stored。
[[vk::binding(2, 1)]] Texture2D<float>       shadowMap;
[[vk::binding(3, 1)]] SamplerComparisonState shadowSampler;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    const float4 world = mul(ubo.model, float4(input.inPosition, 1.0));
    output.pos = mul(pc.viewProj, world);
    output.fragTexCoord = input.inTexCoord;
    output.fragNormal = mul((float3x3)ubo.normalMatrix, input.inNormal);
    output.lightClipPos = mul(pc.lightViewProj, world);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 N = normalize(vertIn.fragNormal);
    float3 L = normalize(-pc.lightDirection.xyz);

    float4 albedo = textures[NonUniformResourceIndex(ubo.texId)]
        .Sample(samplers[NonUniformResourceIndex(ubo.samplerId)], vertIn.fragTexCoord)
        * ubo.color;

    float ndotl = max(dot(N, L), 0.0);

    // 光空间 NDC → 阴影图 uv。Vulkan 的视口把 NDC y=-1 放在图像第一行,
    // 这里的 0.5*(x+1) 和它一致,不需要额外翻 y。
    const float3 ndc = vertIn.lightClipPos.xyz / vertIn.lightClipPos.w;
    const float2 shadowUv = ndc.xy * 0.5 + 0.5;

    float shadow = 1.0;
    if (shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
        shadowUv.y >= 0.0 && shadowUv.y <= 1.0 && ndc.z <= 1.0) {
        // 减一点深度偏移:背面深度已经压掉大部分 acne,这点补剩下的。
        shadow = shadowMap.SampleCmpLevelZero(shadowSampler, shadowUv, ndc.z - 0.002);
    }

    float3 lit = albedo.rgb * (pc.ambientColor.rgb + pc.lightColor.rgb * ndotl * shadow);
    // 光照只乘 rgb;alpha 保持贴图 × 颜色的结果。
    return float4(lit, albedo.a);
}

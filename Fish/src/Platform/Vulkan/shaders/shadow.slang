// 阴影 pass:只写深度,不产生颜色。
//
// 顶点输入只有位置 —— 法线和 uv 用不到。管线仍是顶点 + 片元两段,
// 片元不声明输出,配合 colorAttachmentCount = 0 的管线。

struct PushConstants {
    float4x4 viewProj;  // 这是光空间的 VP,不是主相机
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct ObjectUbo {
    float4x4 model;
};
[[vk::binding(0, 0)]] ConstantBuffer<ObjectUbo> ubo;

[shader("vertex")]
float4 vertMain(float3 inPosition : POSITION) : SV_Position {
    return mul(pc.viewProj, mul(ubo.model, float4(inPosition, 1.0)));
}

[shader("fragment")]
void fragMain() {}

# 批处理(batching)—— 完成记录

> 2026-10-12。参考 `TheCherno/Hazel` 的 `824104c1`(2020-04-03,"Added basic batching to Renderer2D"),
> 那一版只做了顶点合并、把贴图整条回避掉;这一轮做的是完整版。
>
> **状态:跑通。** 构建 0 错误 0 警告,验证层 0 输出,一帧 404 个绘制项降到 3 次 draw。
> 遗留项见第六节。
>
> 逐文件改动见 `BATCH_CHANGES.md`。

---

## 一、做了什么

`VulkanRendererAPI::DrawFrame` 原来对 `items` 逐项走:dynamic_cast → 管线查找 →
`pushConstants` → UBO 写 + `bindDescriptorSets`(dynamic offset)→ 最多 3 次 bind →
一次 draw。**404 项 = 404 次 drawIndexed + 404 次描述符绑定。**

现在多了一条路:同一 (形状, shader) 的项在 CPU 侧展开成顶点、追加进一个批,
一帧结束时每个批只录一次绘制。

实测(临时计数器,数完已删):

```
一帧的 draw 次数: per-item = 1, batch = 2, 合计 = 3
```

| | 改前 | 改后 |
|---|---|---|
| 四边形 | 402 次 | **1 次**(402 × 6 = 2412 顶点) |
| 三角形 | 1 次 | **1 次**(3 顶点) |
| playground2D 的方块 | 1 次 | 1 次(仍走旧路径) |
| 合计 | **404** | **3** |

---

## 二、形状(已定的决定)

| # | 决定 |
|---|---|
| 1 | 批的 key = (形状, shader);两个批:三角形、四边形 |
| 2 | 都走 `batch.slang`,纯色项绑白贴图 + 顶点 color |
| 3 | 四边形批**非索引**绘制,每 quad 写 6 个顶点(后两个重复成 `0,1,2 / 2,3,0`) |
| 4 | 顶点里烘 world-space 位置 + color + uv + texId |
| 5 | texId 用 `uint32_t`(本来定的是 `uint16_t`,见第五节坑 4) |
| 6 | 纹理库固定容量 64,**应用层指定槽位**,开画前一次填满 |
| 7 | `DrawBatch` = 提交参数 POD(类比 `DrawItem`),不持有状态 |
| 8 | 批的运行时状态直接是 `VulkanRendererAPI` 的成员 |
| 9 | 溢出自动扩容(2 倍起跳) |
| 10 | 旧 API 只加不改,`Submit(DrawItem)` 那条路原样保留 |

---

## 三、机制

### 顶点格式(`Fish/src/Renderer/DrawBatch.h`)

```cpp
struct BatchVertex {
    glm::vec3 position;   // location 0,已烘成 world space
    glm::vec4 color;      // location 1
    glm::vec2 uv;         // location 2
    uint32_t  texId;      // location 3,纹理库槽位
};
```

stride 40,offset 0 / 12 / 28 / 36。**字段顺序不能改** —— `BuildVertexInput`
(`VulkanReflection.cpp:138-148`)按 shader 里 `VSInput` 的声明顺序做前缀和算 offset。

四条 `static_assert(offsetof(...))` 是唯一的哨兵:glm 的 aligned 宏一旦打开,
vec4 的对齐会把后面字段推走,而 `BuildVertexInput` 只检查总和不超 stride,错开查不出来。

### 批状态(`VulkanRendererAPI` 的成员)

```cpp
struct BatchBuffer {          // 每个帧槽一份
    Buffer   buffer;          // host-visible | host-coherent,eVertexBuffer,常驻 map
    void*    mapped = nullptr;
    uint32_t capacity = 0;
};

struct BatchState {
    std::vector<BatchVertex> vertices;   // CPU 侧累积,跨 BatchingSubmit
    std::vector<BatchBuffer> frames;     // k_MaxFramesInFlight 份
    Ref<Shader>              shader;     // 第一个进批的项的 shader
    glm::mat4                viewProjection;
};

std::array<BatchState, k_BatchShapeCount> m_Batches;
```

**每帧槽一份是必须的**(`k_MaxFramesInFlight = 2`)。一帧写这一份时,上一帧的命令缓冲
可能还在读它,共用一份就是 `VUID-vkCmdDraw` 那类报错。

### 纹理库

- set **2**、binding 0、`Sampler2D albedo[64]`
- 槽位由应用层指定(`Renderer::CreateTexture2D(path, slot)`)
- 第一次画批时一次写完,之后不再动
- **没登记的槽位也要填一个合法描述符** —— 见第五节坑 1

### 录制流程

现有 per-item 循环不动 → 追加 `RecordBatches()`:每个批一次管线绑定 +
一次 `pushConstants(viewProj)` + 一次 `bindDescriptorSets(set 2)` + 一次 `draw`。
没有逐项循环。批画完清空 CPU 侧的 `vertices`。

---

## 四、关键设计决定(不要重新推导)

### 1. `batch.slang` 是独立文件,不能合进 `texture.slang`

per-item 那条路还在跑,它的顶点是 `pos3+uv2`(stride 20),批是 stride 40。
`BuildVertexInput` 按声明顺序做前缀和,**属性加起来 40 字节会把老路径的 stride 20
撑爆**,建管线时直接抛。一个 shader 只能有一个 `VSInput`,所以只能分文件。

### 2. 批的贴图数组走 set 2,不能复用 set 1

set 1 是 `TextureDescriptorCache` 的「一贴图一个 set」;批是「一个 set 装整个库」。
同一个 binding 上数组长度 1 对 64,`AddShaderBindings` 的 count 检查会当场抛。
分到 set 2 之后 `VulkanTextureDescriptorCache` 一行没动。

### 3. 空槽必须填满,`descriptorBindingPartiallyBound` 不够

`Sampler2D albedo[64]` 的索引来自顶点属性 = 非一致。验证层**证明不了**哪一格不会被访问,
只认「每一格都被 `vkUpdateDescriptorSets` 写过」。留空洞就是:

```
VUID-vkCmdDraw-None-08114 ... Set 2, Binding 0, Index 3, variable "albedo"
... is being used in draw but has never been updated
```

所以空槽填第一个登记过的贴图的 `DescriptorImageInfo`。填什么不影响画面 ——
正常的 `texId` 指不到那儿。\n**结论:那个特性去掉了。** 少一条硬特性就少一批选不出来的设备。

### 4. 批的 key 不含 viewProjection

一帧里两个 layer 各自 `BeginScene` 会给出两套相机。`SubmitBatch` 里加了断言:

```cpp
FS_CORE_ASSERT(state.viewProjection == batch.viewProjection,
    "同一个批里的 viewProjection 不一致:一个批只能配一个相机");
```

**现在能跑是碰巧的,不是被保证的** —— 两个 layer 的 `OrthographicCameraController`
都是默认构造、`OnEvent` 都不消耗事件,所以两个矩阵恰好逐位相等。
谁动一下其中一个相机的参数,断言就响。

### 5. shader 侧的 `nointerpolation` 和 `NonUniformResourceIndex` 都不能漏

```slang
nointerpolation uint fragTexId;                                   // 整数不能插值
albedo[NonUniformResourceIndex(vertIn.fragTexId)].Sample(...);    // 索引非一致
```

两条漏了都**不报错**:

- 少 `nointerpolation` → 每个像素拿到三个顶点 texId 的插值,采到隔壁顶点的纹理
- 少 `NonUniformResourceIndex` → Slang 不生成 `OpDecorate NonUniform`,驱动按整个
  subgroup 算一次描述符,结果同上

第二条是探针实测出来的:普通写法编出来的 SPIR-V 里**一条 `NonUniform` 都没有**。

---

## 五、踩到的坑

### 1. 非一致索引下,验证层要求整个数组写满

见第四节第 3 条。方案里原本打算靠 `descriptorBindingPartiallyBound` 让「未写也不访问」
合法 —— 实测挡不住。

### 2. 两个 layer 各建一份 shader 会拿到两个 `vk::ShaderModule`

于是同一条管线建两遍,而且批靠 shader 对象本身判「同批」,两份永远判不相等。
修法:`Renderer::CreateShader` 按 (name, path) 缓存。
`Shutdown` 里必须先 `s_ShaderCache.clear()` 再删后端 —— 缓存持有 `vk::raii::ShaderModule`。

### 3. `Renderer::Shutdown` 的顺序

```cpp
m_LayerStack.Clear();    // 层先走,放掉它们那份 Ref
Renderer::Shutdown();    // 缓存再清,最后删后端
```

### 4. `uint16_t` 顶点输入要多两个设备特性(所以退回了 `uint32_t`)

探针实测(`slangc` 编一份带 `uint16_t` 顶点输入的 shader),SPIR-V 头部出来两条:

```
OpCapability Int16
OpCapability StorageInputOutput16
```

对应:

| capability | 需要的特性 |
|---|---|
| `Int16` | `VkPhysicalDeviceFeatures::shaderInt16` |
| `StorageInputOutput16` | `VkPhysicalDevice16BitStorageFeatures::storageInputOutput16` |

而 `VulkanContext.cpp` 的特性列表是**硬要求**,不满足的设备一个都选不出来。
每 quad 省 8 字节换两条硬特性,不值。反射那条链本身没问题(`spirv_reflect.c:3122`
对 16 位报 `R16_UINT`)。

### 5. `compile.bat` 原来只编两个 shader

新增 `batch.slang` 之后忘了加进去就是「构建成功、`.spv` 陈旧」。已补。

---

## 六、遗留项

### 没验证的

| 项 | 说明 |
|---|---|
| **画面** | 我没亲眼看过。要确认:400 个方块的位置和颜色、Checkerboard / ChernoLogo 各自采对(texId 串了在这里暴露)、三角形还在 `(-1.2, -0.5)` |
| **扩容路径** | 初始容量 60000 顶点,实际只用 2415,永远碰不到。要测得把 `k_InitialQuadVertices` 改成几十 |
| **resize** | 要真人拖 |
| **退出** | 那次 `Kill()` 是 `TerminateProcess`,不跑析构,只用来量 CPU |

### 结构性欠账

- **批和 per-item 不能交错。** 批在 per-item 之后画,顺序定死。现在没有深度缓冲,
  顺序就是一切 —— 换一个重叠的布局就能看出来。改前的顺序是「400 方块 → 两张贴图 →
  三角形」,改后是「三角形批 → 四边形批(400 方块 + 两张贴图)」。这次没看出差别
  是因为三角形和方块区域不重叠。
- **一个批一个相机。** 见第四节第 4 条,现在是碰巧成立的。
- **纹理库写一次就不动了。** 之后再 `RegisterBatchTexture` 会抛(见代码里的三处 throw)。
- **`k_MaxTextures = 64`** 受 `VkPhysicalDeviceLimits::maxDescriptorSetSampledImages`
  约束(规范保证的下限是 96)。桌面卡都在百万级,但这是 64 这个数的由来。
- **`MarkDown/RENDERER_ARCHITECTURE.md` 现在只描述旧路径了** —— 批这条路没写进去。

### 还想做的

- 旋转(`DrawRotatedQuad` 那种)—— 现在顶点烘的是轴对齐的世界坐标,加旋转就是
  `transform` 乘以带上旋转的矩阵,顶点侧不用动,但要确认三角形批也要。
- 把 playground2D 也搬过来(现在它保留旧路径,是故意留的验证面)。
- 中途 flush:现在一个批必须一次画完,顶点数只能靠扩容扛。

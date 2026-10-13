# 批处理 —— 改动文件汇总

> 2026-10-12。配套 `BATCH_REFACTOR_TODO.md`。
> 本文只列**这一轮**动过的文件。工作区里还有别的未提交改动,见最后一节。

---

## 一、新增

| 文件 | 内容 |
|---|---|
| `Fish/src/Renderer/DrawBatch.h` | `BatchShape` 枚举(值直接当批号用,不能改顺序)、`BatchVertex` + 4 条 `static_assert`、`k_BatchVertexStride = 40`、`k_NoBatchTextureSlot`、`DrawBatch` POD。纯数据,不含 Vulkan 类型 |
| `Fish/src/Platform/Vulkan/shaders/batch.slang` | 批处理 shader。`VSInput` 四个属性、`nointerpolation uint fragTexId`、`Sampler2D albedo[64]` 在 set 2、`NonUniformResourceIndex`。顶点只乘 `viewProj` |
| `Fish/src/Platform/Vulkan/shaders/batch.spv` | 上面的编译产物(`-target spirv -profile spirv_1_4`) |

---

## 二、修改

### 设备层

| 文件 | 改动 |
|---|---|
| `Fish/src/Platform/Vulkan/VulkanContext.cpp` | `isDeviceSuitable` 和 `createLogicalDevice` 两处各加 `vk::PhysicalDeviceDescriptorIndexingFeatures`,只要 `shaderSampledImageArrayNonUniformIndexing`。**两处必须一致** —— 不同步就是查一个链子里没开的字段 |

### 描述符

| 文件 | 改动 |
|---|---|
| `VulkanDescriptorAllocator.h` | 加 `k_BatchTextureSet = 2` / `k_BatchTextureBinding = 0` / `k_MaxTextures = 64`;`writeCombinedImageSamplers` 声明 |
| `VulkanDescriptorAllocator.cpp` | `AddShaderBindings` 增加 `count` 比较(原来数组长度不一致会**静默截断**);新增复数版写入,一次写完整个数组绑定 |

`BuildLayout` / `BuildPool` **一行没改** —— 反射的 `count` 本来就透传给 `descriptorCount`,
池子按 `count * maxSets` 算。数组这条链是天生支持的。

### 后端

| 文件 | 改动 |
|---|---|
| `VulkanFrameData.h` | `Frames::currentIndex()` —— 批缓冲要挑当前帧槽 |
| `VulkanRendererAPI.h` | `SubmitBatch` / `RegisterBatchTexture` 声明;`BatchBuffer` / `BatchState` 两个内嵌结构;批状态 `m_Batches` 和纹理库三个成员;`EnsureBatchBuffer` / `EnsureBatchTextureLibrary` / `RecordBatches` 三个私有方法 |
| `VulkanRendererAPI.cpp` | 见下 |

`VulkanRendererAPI.cpp` 里加的:

- 匿名命名空间:单位四边形 / 单位三角形的顶点和 uv、`k_QuadOrder`(非索引展开)、
  `k_InitialTriangleVertices = 3000` / `k_InitialQuadVertices = 60000`、`AppendShapeVertices()`
- `Init()`:给两个批各 `resize(k_MaxFramesInFlight)` 个帧槽
- `RegisterBatchTexture()`:查越界 / 查重 / 查「库是否已经定下来」,三处都抛
- `SubmitBatch()`:判 shader 一致 + viewProjection 一致(两条断言),再展开成顶点
- `EnsureBatchBuffer()`:host-visible 常驻 map,2 倍扩容,**不拷旧数据**(每帧从 offset 0 重写)
- `EnsureBatchTextureLibrary()`:一次写完 64 格,空槽填第一个登记过的贴图
- `RecordBatches()`:每批一次管线 + 一次 pushConstants + 一次 set 2 + 一次 draw
- `DrawFrame()`:在 per-item 循环之后、`endRendering()` 之前调用 `RecordBatches`

### 抽象层

| 文件 | 改动 |
|---|---|
| `RendererAPI.h` | 新增 `virtual void SubmitBatch(const DrawBatch&)`,非纯虚(理由同 `DrawFrame`:给待删的 OpenGL 留活路) |
| `Renderer.h` | 新增 `static void BatchingSubmit(DrawBatch)`;`CreateTexture2D` 多收 `uint32_t batchSlot = k_NoBatchTextureSlot` |
| `Renderer.cpp` | 新增按 (name, path) 的 **shader 缓存**;`Shutdown` 里先清缓存再删后端;新增 `BatchingSubmit`;`CreateTexture2D` 建完贴图调 `RegisterBatchTexture` |

### 应用层

| 文件 | 改动 |
|---|---|
| `Playground/src/Playground.cpp` | `ExampleLayer` 改走批处理:建 `batch.spv`,三张贴图按槽位 0/1/2 登记,403 个项全部 `BatchingSubmit`。删掉 4 个 VB/IB 成员 |

### 外部步骤

| 文件 | 改动 |
|---|---|
| `shaders/compile.bat` | 加一行 `batch.slang → batch.spv` |

---

## 三、特意没动的

| 文件 | 为什么 |
|---|---|
| `shaders/texture.slang` / `flat_color.slang` | per-item 那条路还在跑。见 `BATCH_REFACTOR_TODO.md` 第四节第 1 条 |
| `VulkanTextureDescriptorCache.{h,cpp}` | 批的数组走 set 2,和它没关系 |
| `VulkanPipelineCache.{h,cpp}` | 管线按 `PipelineDesc` 缓存,批只是多了一个 `vertexStride = 40` 的 key |
| `VulkanVertexBuffer.{h,cpp}` | 批的顶点缓冲直接建 `Buffer`,不走 `VulkanVertexBuffer`(那个是 device-local、一次性的) |
| `Playground/src/playground2D.{h,cpp}` | **故意**保留旧路径 —— 两条并存,这一项就是旧路径的验证面 |

---

## 四、工作区里不属于这一轮的改动

`git status` 里还会看到这些,是之前几轮留下的,这轮没碰:

```
 M Fish/src/Core/OrthographicCameraController.cpp
 M Fish/src/Platform/Vulkan/VulkanShader.cpp
 M Fish/src/Platform/Vulkan/VulkanShader.h
 M Fish/src/Platform/Vulkan/VulkanTexture.cpp
 M Fish/src/Platform/Vulkan/shaders/texture.slang
 M Fish/src/Platform/Vulkan/shaders/texture.spv
 M Fish/src/Renderer/OrthographicCamera.cpp
 M Fish/src/Renderer/OrthographicCamera.h
 M Playground/src/playground2D.cpp
 M Playground/src/playground2D.h
?? MarkDown/
?? Playground/assets/textures/White.png
```

另外 `D:\Fish` 根目录下那批 `.md`(`PLAN.md` / `REVIEW_DRILLS.md` / `INTERVIEW_NOTES.md` /
`RENDERER_ARCHITECTURE.md` / `DESIGN_REPLAY.md` / `PIPELINE_REFACTOR_TODO.md` /
`BUFFER_REFACTOR_TODO.md`)在 git 里记成已删除 —— 实际内容搬到了 `MarkDown/` 下,还没提交。

---

## 五、重新构建要跑的东西

```bash
# 1. 新增了 Fish/src 下的文件,GLOB_RECURSE 没有 CONFIGURE_DEPENDS,必须先 configure
cmake -S . -B build

# 2. 编译
cmake --build build --config Debug

# 3. 只有改了 .slang 才需要 —— 这一步没接进 CMake,漏跑就是 .spv 陈旧
Fish/src/Platform/Vulkan/shaders/compile.bat

# 4. 跑(工作目录必须是仓库根)
./build/bin/DEBUG/Playground.exe
```

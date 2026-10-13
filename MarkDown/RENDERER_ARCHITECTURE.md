# 渲染器架构（2026-09-23）

> **这份只讲架构**：分层、数据流、初始化/销毁顺序、所有权、边界。
> 过程记录（做了什么、为什么这么做、怎么验的、撞了哪些 bug）在 `PIPELINE_REFACTOR_TODO.md`。
>
> 描述的是 **2026-09-23 这次改动之后**的状态。改动前的状态在第一节的对照表里。

---

## 一、一句话概括

| | |
|---|---|
| **改前** | `VulkanRendererAPI::Init` 把所有资源硬建出来；管线只有一条、描述符布局写死、顶点布局写死；`DrawFrame` 里**没有任何 draw 调用** |
| **改后** | 资源由**应用层**通过 `Renderer::Create*` 创建；管线和描述符布局**从 shader 推导**；后端只做一件事 —— 把应用层攒下的绘制项录成命令 |

---

## 二、改前 vs 改后

| | 改前 | 改后 |
|---|---|---|
| **资源创建** | 全在 `VulkanRendererAPI::Init` 里硬建；贴图路径写死 `"Fish/src/Platform/Vulkan/textures/texture.jpg"` | `Renderer::Create*` 五个静态工厂，数据从应用层来 |
| **谁持有资源** | 后端持有（`m_MainTexture`、`m_Pipeline` …） | **应用层持有**（layer 里的 `Ref<Shader>` / `Ref<VertexArray>` / `Ref<Texture2D>`），后端只在绘制时按需要降级访问 |
| **管线** | 单条，`Init` 里建；shader 路径 / 入口名 / 顶点布局 / 全部固定功能状态写死在 `VulkanPipeline.cpp` | `PipelineCache`，按 `PipelineDesc` 缓存；顶点布局从 shader 反射 |
| **描述符布局** | 写死 `binding 0 = UBO`、`binding 1 = sampler`，在构造函数里拼 | 由所有 shader 声明的绑定取**并集**推导 |
| **描述符集** | 每帧槽一个，构造时就把唯一那张贴图绑死 | **两个 set**：set 0 = UBO（dynamic offset），set 1 = 贴图（按贴图缓存） |
| **每物体数据** | 没有 —— 一帧一个 MVP | UBO 环形缓冲（只放 `model`，dynamic offset 索引）+ 推送常量（`viewProj` + `color`） |
| **顶点布局的真相源** | `VulkanBuffer.h` 里的 `struct Vertex` + 两个静态函数 | **shader 自己**（SPIR-V 反射）；`stride` 由调用方给 |
| **`Submit`** | 硬转 `OpenGLShader` 上传 uniform，然后 `DrawIndexed` | 只把 `DrawItem` 塞进队列，**不碰 GPU** |
| **绘制** | `DrawFrame` 里是个空注释位 | 录制循环消费队列 |
| **贴图"绑定"** | `Texture2D::Bind(slot)`，先绑再画 | 贴图是 `Submit` 的**参数** |
| **后端接口** | `DrawIndexed(const Ref<VertexArray>&)` 纯虚 | 删掉；改成 `DrawFrame(const std::vector<DrawItem>&, const glm::mat4&)` |

---

## 三、分层

```
┌─ 应用层 ──────────────────────────────────────────────────────┐
│  Playground / playground2D                                     │
│  只认识 Fish::Renderer 的静态接口 + Fish 的抽象类型              │
│  （Ref<Shader> / Ref<VertexArray> / Ref<Texture2D>）            │
└────────────────────────┬───────────────────────────────────────┘
                         │ Renderer::Init / Create* / Submit / DrawFrame
┌────────────────────────▼───────────────────────────────────────┐
│  引擎层   Fish::Renderer                                        │
│  · s_RendererAPI   ← RendererAPI*，整个引擎里唯一知道具体后端的  │
│  · s_DrawQueue     ← std::vector<DrawItem>，Submit 攒的         │
│  · s_SceneData     ← ViewProjectionMatrix                      │
└────────────────────────┬───────────────────────────────────────┘
                         │ 虚函数
┌────────────────────────▼───────────────────────────────────────┐
│  抽象接口层   RendererAPI / Shader / VertexArray / VertexBuffer  │
│               / IndexBuffer / Texture2D                        │
│  **不出现任何 vk:: 类型**（这条是硬约束，见第七节）               │
└────────────────────────┬───────────────────────────────────────┘
                         │ : public
┌────────────────────────▼───────────────────────────────────────┐
│  后端层   Fish/src/Platform/Vulkan/                             │
│                                                                │
│  VulkanRendererAPI ── 持有全部后端对象，录制命令                 │
│    ├ VulkanShader / VulkanVertexBuffer / VulkanIndexBuffer /    │
│    │ VulkanVertexArray / VulkanTexture2D    ← 适配类            │
│    ├ VulkanReflection        ← SPIR-V 反射                      │
│    ├ PipelineCache           ← 管线 + pipeline layout           │
│    ├ DescriptorAllocator     ← 描述符 layout + pool             │
│    ├ TextureDescriptorCache  ← 每贴图一个 set                    │
│    └ Frames / FrameData      ← 帧槽（UBO 环形缓冲 + 命令缓冲）    │
└────────────────────────────────────────────────────────────────┘

（OpenGL 那一侧还留着 `OpenGLRendererAPI` / `OpenGLShader` / …，
  是待删的遗留，不走这条路径。）
```

**依赖方向是单向的**：应用层 → 引擎层 → 抽象接口 ← 后端层。
后端实现抽象接口，但**抽象接口不知道后端存在**。

---

## 四、一帧的数据流

### CPU 侧：从 `OnUpdate` 到 `drawIndexed`

```
Application::run()  每一帧
 │
 ├─ 每个 layer 的 OnUpdate(ts)
 │   ├─ Renderer::SetClearColor(color)
 │   ├─ Renderer::BeginScene(camera)        ← 存 ViewProjectionMatrix 进 s_SceneData
 │   ├─ Renderer::Submit(...) × N           ← 只 push_back 进 s_DrawQueue，不碰 GPU
 │   └─ Renderer::EndScene()
 │
 └─ Renderer::DrawFrame()
      └─ s_RendererAPI->DrawFrame(s_DrawQueue, s_SceneData->ViewProjectionMatrix)
           └─ 返回后 s_DrawQueue.clear()

VulkanRendererAPI::DrawFrame(items, viewProjection)
 │
 ├─ EnsureRenderState()          ← 只有第一帧真的干活，之后是判空
 ├─ 消费 m_FramebufferResized → m_SwapChain.recreate()   ← 必须在 acquire 之前
 ├─ m_Frames.current()           ← 当前帧槽
 ├─ waitForFences(帧槽的 fence)
 ├─ acquireNextImage()
 ├─ resetFences
 ├─ commandBuffer.reset() + begin
 ├─ 图像布局 Undefined → ColorAttachmentOptimal
 ├─ beginRendering（loadOp = eClear，clearValue 取 m_ClearColor）
 ├─ setViewport / setScissor（按交换链 extent）
 │
 ├─ frame.ResetObjects()         ← UBO 环形缓冲的写游标归零
 │
 ├─ for (每个 DrawItem)          ← ★ 录制循环，见下
 │
 ├─ endRendering
 ├─ 图像布局 ColorAttachmentOptimal → PresentSrcKHR
 ├─ commandBuffer.end()
 ├─ submit（等 presentCompleteSemaphore，点亮 inFlightFence，发 renderFinishedSemaphore）
 ├─ presentKHR
 └─ m_Frames.advance()
```

### 录制循环：每个 `DrawItem` 做什么

```
for each item:
  ① dynamic_pointer_cast 到 Vulkan 具体类（shader / vertexArray / texture）
     拿不到就 continue —— 引擎里同时存在 OpenGL 遗留对象
  ② 组 PipelineDesc（shader 模块 + 入口名 + stride + 固定功能状态）
     → PipelineCache.GetOrCreate(desc, shader->vertexInput())
     → 管线变了才 bindPipeline
  ③ pushConstants({viewProj, color})               ← 每物体一次，80 字节
  ④ frame.PushObjectUbo(item.transform) → dynamicOffset
     → bindDescriptorSets(set 0, 帧槽的 UBO set, dynamicOffset)   ← 每物体一次
  ⑤ 贴图变了才 bindDescriptorSets(set 1, 贴图 set)
  ⑥ 顶点数组变了才 bindVertexBuffers / bindIndexBuffer
  ⑦ drawIndexed(索引数, 1, 0, 0, 0)
```

**"变了才发"的目的是去重。** 下面是**实测**的第 100 帧的命令计数（临时计数打的，
不是按代码推的）：

> `items=404  bindPipeline=4  bind贴图=2  bind顶点数组=3  管线缓存里共 4 条`

| 命令 | 次数 | 为什么 |
|---|---|---|
| `pushConstants` + UBO 的 `bindDescriptorSets` | **404** | 每物体的 transform 和 color 都不同，去重不了 |
| `bindPipeline` | **4** | 400 方块 + 2 贴图 + 三角形(stride 28) + playground2D(stride 12)，四条管线 |
| `bindDescriptorSets`(set 1，贴图) | **2** | 两张贴图 |
| `bindVertexBuffers` / `bindIndexBuffer` | **3** | 三个顶点数组 |
| `drawIndexed` | 404 | |

### GPU 侧：shader 和引擎之间的契约

| 位置 | 内容 | 谁写 |
|---|---|---|
| 推送常量 offset 0，80 字节，vertex+fragment | `{mat4 viewProj; vec4 color}` | 每物体 push 一次 |
| 描述符 set 0 / binding 0，`eUniformBufferDynamic`，vertex | `{mat4 model}`，64 字节 | 环形缓冲，dynamic offset 索引 |
| 描述符 set 1 / binding 0，`eCombinedImageSampler`，fragment | 贴图 | `TextureDescriptorCache` |
| 顶点属性 location 0…N | 由 shader 声明的 `Location` 决定 | 调用方给 `stride`，其余从反射来 |

这些数字的**真相源是 `.slang` 文件**（`Fish/src/Platform/Vulkan/shaders/`）。
引擎侧的对应常量在 `VulkanDescriptorAllocator.h`（`k_ObjectUboSet` / `k_ObjectUboBinding` /
`k_TextureSet` / `k_TextureBinding`）和 `VulkanFrameData.h`（`ObjectUbo` / `PushConstants`）。
**两边改一边就会错，而验证层只在某些情况下会报。**

---

## 五、初始化顺序 —— 一条延迟链

这次架构上最绕的一处。**根因是：shader 由应用层创建，比 `Renderer::Init` 晚。**

```
Application 基类构造
 └─ Renderer::Init(nativeWindow)
     └─ VulkanRendererAPI::Init
         ├─ instance / debugMessenger / surface / VulkanContext
         ├─ SwapChain
         ├─ CommandPool ×2（主池 + 一次性传输池）
         └─ Frames  ← UBO 环形缓冲 + 命令缓冲 + 信号量 + fence
                       ★ 但【不含】描述符集 —— 那时还没有 layout

Playground 构造（PushLayer 里）
 ├─ Renderer::CreateShader(path) ×N
 │   └─ VulkanShader 构造
 │       ├─ 反射顶点属性 → 存起来（管线要用）
 │       ├─ 反射描述符绑定 → DescriptorAllocator::AddShaderBindings
 │       │    └─ 并集变大时【重建 layout】（这时池还没建，安全）
 │       └─ createShaderModule
 ├─ Renderer::CreateVertexBuffer / CreateIndexBuffer   ← 走 staging + 一次性命令拷贝
 ├─ Renderer::CreateVertexArray
 └─ Renderer::CreateTexture2D

第一次 Renderer::DrawFrame
 └─ VulkanRendererAPI::EnsureRenderState()
     ├─ Frames::EnsureUboSets     ← 这时 layout 已定，分配帧槽的 UBO 描述符集
     │                              池也在这一步才真正建（配额取决于并集最终形状）
     ├─ m_TextureSets.emplace(...)
     └─ m_PipelineCache.emplace(...)   ← 需要 layoutHandles()
```

**依赖链**：`shader → 描述符 layout → 池 / pipeline layout → 描述符集 / 管线`

**为什么不能提前**：`DescriptorAllocator` 的 layout 是"所有 shader 声明的绑定的并集"，
而 shader 是应用层给的。`PipelineCache` 又需要这个 layout 来建它的 pipeline layout。

**顺带一个取舍**：`AddShaderBindings` 只在并集**变大**时才重建 layout，
并要求那时**还没分配过描述符集**（`m_pool == nullptr`）——
并集增长意味着旧 layout 被销毁重建，已经发出去的描述符集会变成野句柄。

---

## 六、销毁顺序

**这次改动里最容易出事的地方**，两轮才修对。

```
main 返回 → ~Playground → ~Application（函数体）
 │
 ① Renderer::WaitIdle()          ← GPU 空转。最后一帧的命令缓冲还引用着
 │                                  layer 的顶点缓冲和贴图，不等就销毁会报
 │                                  VUID-vkDestroyBuffer-buffer-00922
 ② m_LayerStack.Clear()          ← 销毁 layer，连带它持有的全部 GPU 资源
 │                                 ★ 必须在 ③ 之前。LayerStack 是 Application 的
 │                                   成员，成员析构晚于析构函数体 —— 不显式清一次，
 │                                   顺序就变成"设备先没、资源后销毁"，全泄漏
 ③ Renderer::Shutdown()          ← delete s_RendererAPI
      └─ ~VulkanRendererAPI
          ├─ device.waitIdle()
          └─ 成员按声明逆序销毁：
             m_PipelineCache      ← pipeline 引用 pipeline layout
             m_TextureSets        ← 描述符集，析构会 vkFreeDescriptorSets
             m_Frames             ← 描述符集 + 命令缓冲（命令缓冲要早于 CommandPool）
             m_TransientPool
             m_CommandPool
             m_DescriptorAllocator ← pool + layout，必须晚于所有描述符集
             m_SwapChain
             m_DeviceContext
             m_Surface / m_DebugMessenger / m_Instance / m_Context
```

**`VulkanRendererAPI` 的成员声明顺序是被销毁顺序约束的**（`VulkanRendererAPI.h:35-53`）。
往那个列表里插东西时按这条排 —— **排错了不报编译错误，是退出时踩空句柄。**

---

## 七、四条边界（这次定死的）

### 1. `Renderer.h` / `RendererAPI.h` 里不出现任何 `vk::` 类型

`RendererAPI.h` 会被 `Renderer.h` 包含，而 `Renderer.h` 会被 `Playground.cpp`
经 `includings.h` 包含。Playground 的 vcxproj **没有** Vulkan 的编译定义
（`VULKAN_HPP_NO_STRUCT_CONSTRUCTORS` / `NOMINMAX` / `GLFW_INCLUDE_NONE` / `DEBUG`
都是 `PRIVATE`，不向链接方传播）。所以：

- `Renderer.h` 只用前向声明 `class VulkanContext;`
- `DrawItem` 里只有 Fish 的抽象类型和 glm
- `Renderer::Create*` 的签名里也没有 `vk::`

### 2. 资源创建走 `Renderer` 的静态方法，不进 `RendererAPI` 虚函数

把"引擎有哪几种资源"写进后端接口，意味着加一种资源要动 `RendererAPI` 和**所有**实现
（包括已经没人维护的 `OpenGLRendererAPI`）。而且 `RendererAPI` 现在全是"动作"函数
（`Init` / `DrawFrame` / `SetClearColor` / `WaitIdle`），没有一个"创建"。

实现靠 `dynamic_cast<VulkanRendererAPI*>(s_RendererAPI)` —— 和 `Renderer::Init` 里
那个 `switch` 一样，都属于"引擎层是唯一知道具体后端的地方"。

### 3. 顶点布局的真相源是 shader，不是引擎

`VertexBuffer::GetLayout` / `SetLayout` 还留在抽象接口上（满足接口），
但在 Vulkan 路径上**存下来的 `BufferLayout` 没有任何人读**。
管线要的顶点输入是 `(location, vk::Format, offset, stride)`，
前三个从 SPIR-V 反射来，`stride` 由调用方给。

### 4. `Submit` 只入队，不碰命令缓冲

命令缓冲只在 `RendererAPI::DrawFrame` 里开着。这也是 Renderer2D 的形状 ——
`Submit` 攒批、`Flush` 时录。

---

## 八、目录地图

```
Fish/src/
├── Renderer/                      ← 抽象接口层
│   ├── Renderer.{h,cpp}             静态门面 + s_DrawQueue + Create* 工厂
│   ├── RendererAPI.h                DrawItem + 5 个虚函数
│   ├── Shader.h / VertexArray.h / Buffer.h / Texture.h
│   ├── OrthographicCamera.h         （相机，无改动）
│   └── ...
├── Platform/Vulkan/               ← 后端层
│   ├── VulkanRendererAPI.{h,cpp}       持有全部后端对象 + 录制
│   ├── VulkanReflection.{h,cpp}        ★新  SPIR-V 反射
│   ├── VulkanPipelineCache.{h,cpp}     ★新  管线 + pipeline layout 缓存
│   ├── VulkanDescriptorAllocator.{h,cpp}   描述符 layout + pool（改）
│   ├── VulkanTextureDescriptorCache.{h,cpp} ★新  每贴图一个 set
│   ├── VulkanFrameData.{h,cpp}        帧槽 + UBO 环形缓冲（改）
│   ├── VulkanShaderAdapter.{h,cpp}     ★新  VulkanShader : Shader
│   ├── VulkanVertexArray.{h,cpp}       ★新  顶点/索引缓冲 + 顶点数组适配
│   ├── VulkanTexture2D.{h,cpp}         ★新  VulkanTexture2D : Texture2D
│   ├── VulkanBuffer.{h,cpp}           +createDeviceLocal
│   ├── VulkanShader.{h,cpp}           读文件 + 建 module（工具，非适配类）
│   ├── VulkanContext / SwapChain / Image / Texture / CommandPool / Surface / ValidationLayers
│   └── shaders/                       flat_color.slang/.spv、texture.slang/.spv、compile.bat
├── Platform/OpenGL/               ← 遗留，待删
└── Core/                          ← Application / Layer / LayerStack / Window / Log

Fish/vendor/SPIRV-Reflect/         ★新  spirv_reflect.{c,h}（Apache 2.0）
```

---

## 九、架构上还没到位的地方

| | 事 | 影响 |
|---|---|---|
| **1** | **Y 轴朝向**：Vulkan 的 framebuffer 空间 Y 朝下，`glm::ortho` 出的是 OpenGL 约定 → 场景上下翻 | 画面能看，但和 OpenGL 路径方向相反。翻在哪层是个取舍 |
| **2** | **没有深度测试**：管线 `pDepthStencilState = nullptr` | 遮挡完全由绘制顺序决定。多 pass 或 3D 时是硬阻塞 |
| **3** | **`PipelineCache::Invalidate` 没有调用者**：`SwapChain::recreate()` 不通知任何人 | 交换链 format 变了旧管线就不兼容。现在不出错是**实测碰巧**（本机 resize 之间 format 没变过），不是规范保证 |
| **4** | **顶点输入只支持单 binding** | 一个顶点跨两个 buffer（位置和实例数据分开）、实例化，都要先改 `BuildVertexInput` |
| **5** | **描述符设施只支持一套 layout** | 多描述符集（比如 set 0 每帧、set 1 每物体、set 2 材质）要改 `DescriptorAllocator` |
| **6** | **shader 编译没有构建集成** | `compile.bat` 手动跑，`slangc` 路径写死。改完 shader 忘了重编就拿到旧 `.spv` |
| **7** | **创建路径的 `dynamic_cast` 失败时只返回 `nullptr`** | `Renderer::Create*` 拿不到 Vulkan 后端时 `FS_CORE_ASSERT` + `return nullptr`。而 `FS_CORE_ASSERT` 的开关链条是 `FS_DEBUG` → `macro.h:11-13` 派生 `FS_ENABLE_ASSERTS` → 断言体。**`FS_DEBUG` 只在 Debug 配置定义**（`CMakeLists.txt`），所以 Release 下断言整个是空的，只剩一个静默的 `nullptr` |

# 管线重构 —— 完成记录

> 更新于 2026-09-23。对应 `PLAN.md` 的 M2（`VulkanRendererAPI`）。
>
> 覆盖上一轮盘出的成本表前三项 —— **#1 管线管理 / #2 资源创建 / #3 描述符与绑定模型**。
> 这三项咬合面重叠（管线 ← 顶点布局 / shader / 描述符布局 ← UBO / 纹理），
> 所以一起定、一起改。附带把 5 个适配类（原本归 W6/W8/W9）也做了。
>
> **状态：全部完成。** 构建 0 错误 0 警告；稳态跑 20 秒 0 条验证层输出；
> `WM_CLOSE` 退出与 resize 两条边界路径各自独立验过；**画面已截屏确认**。
> 死代码也已清理（见第六节）。

---

## 怎么读这份文档（分两天）

**不按文档顺序读。** 建议的顺序是先建立地图、再看细节 —— 直接从头读 S1–S8 会
在还不知道"最终形状"和"哪些是定死的"的情况下啃细节。

> 行号是写这份的时候的，自己改过文档的话会飘。

### 第一天 · 约 40 分钟 —— 建立地图

| 顺序 | 读哪 | 行 | 为什么先读它 | 大约 |
|---|---|---|---|---|
| 1 | **三、关键设计决定** | 415 | 7 条"不要重新推导"的结论。先知道哪些是定死的，后面读细节时不会一直分心去想"为什么不那样做" | 8 分钟 |
| 2 | **二、最终形状** | 394 | 现在的结构，一屏看完 | 3 分钟 |
| 3 | **四、验证结果** | 451 | **先知道验到什么程度、哪里没验。** 否则读 S1–S8 时容易把没验的当成验过的 | 6 分钟 |
| 4 | **一、S1–S8 逐任务** | 57 | 主体。每步的「**为什么这么做**」是重点，「怎么验的」可以快读 | 25 分钟 |

### 第二天 · 约 40 分钟 —— 细节和账

| 顺序 | 读哪 | 行 | 重点 | 大约 |
|---|---|---|---|---|
| 5 | **五、bug** | 484 | 14 条**按重要性排，不按发现顺序**。先读 **7 / 8 / 9 / 10 / 14** 这五条 —— 它们是现场抓出来的、也是最可能再犯的；`1–6` 和 `13` 是环境与 API 细节，可以快读 | 25 分钟 |
| 6 | **九、待办总表** | 845 | 哪些要你拍板（A 组）、哪些我直接做就行（B 组） | 5 分钟 |
| 7 | **六、遗留 / 待定** + **八、方法论教训** | 745 / 809 | 六是背景，八是这次踩出来的五条工作方法 | 8 分钟 |
| 8 | **七、验证命令** | 781 | **当手册用，不用通读** —— 跑构建/跑程序/编 shader/跑验证脚本的时候来查 | 用到再翻 |

### 如果只有一天

读 **三 → 二 → 四 → 五（只读 7 / 8 / 9 / 10 / 14）**，约 50 分钟。
这五部分覆盖了"现在长什么样""验到什么程度""哪里会再犯"。

### 几个读的时候值得停一下的地方

- **一 · S6e**（录制路径的验证）—— 这一步花的时间最长，因为连撞三个 bug。
  看它是怎么从"卡死"定位到具体位置的。
- **五 · bug 9**（异常吊着）—— 现象和"死锁"一模一样，但根因完全不同。
  这篇里它出现了两次（bug 7 和 bug 8 的表象都是它）。
- **五 · bug 14**（cullMode）—— "验证层干净 ≠ 画面对"的最干净例子。
  构建、稳态、退出、resize、CPU 全部通过，而画面是空的。
- **八 · 第 6 条**（截屏）—— 以后"画面我看不了"这句话可以不用说了。

---

## 一、S1–S8 逐任务

八个任务是我动手前拆的步骤。下面按当时的顺序写：**做了什么 / 为什么这么做 / 怎么验的**。

### S1 · vendor SPIRV-Reflect 并接进构建

**做了什么**

| 动作 | 位置 |
|---|---|
| 拷 `spirv_reflect.c` + `spirv_reflect.h`（5552 + 2467 行，Apache 2.0） | `D:\Vulkan_SDK\Source\SPIRV-Reflect\` → `Fish/vendor/SPIRV-Reflect/` |
| 加进 `target_sources`，照 `stb_image` 的写法 | `CMakeLists.txt:92-95` |
| 加 include 目录（**PRIVATE**，不向 Playground 传播） | `CMakeLists.txt:114-117` |
| 定义 `SPIRV_REFLECT_USE_SYSTEM_SPIRV_H` | `CMakeLists.txt:107` |
| 给这一个 `.c` 关掉预编译头 | `CMakeLists.txt:100-101` |

**为什么这么做**

- **vendor 源码，不链 SDK 的预编译库。** SDK 的 `Lib/` 下有 `spirv-reflect-static.lib`，
  但 `find_package(Vulkan)` **没有暴露对应 target**（查过：`D:\Vulkan_SDK\cmake\` 不存在），
  链它就得手写路径。而 vendor 源码和 `stb_image` 现在的做法一致，也不绑 SDK 目录布局。
- **用系统 `spirv.h`，不另外拷一份。** `spirv.h` 必须和 `vulkan_core.h` 同版本，
  多拷一份就是两个版本源。

**怎么验的**：构建通过，`build/Fish.dir/Debug/spirv_reflect.obj` 产出，exe 链接成功。

**这一步踩了两个坑**，都记在第五节（bug 1、bug 2）。

---

### S2 · 写 VulkanReflection：从 SPIR-V 读顶点属性和描述符绑定

**做了什么**：新建 `Fish/src/Platform/Vulkan/VulkanReflection.{h,cpp}`（67 行的头）。

| 函数 | 位置 | 作用 |
|---|---|---|
| `ReflectVertexInput(spirv, vertexEntry)` | `VulkanReflection.cpp:51` | 只取**指定入口点**的 Input 变量 |
| `ReflectDescriptorBindings(spirv)` | `VulkanReflection.cpp:89` | 遍历**所有入口点**后合并去重 |
| `BuildVertexInput(reflected, stride)` | `VulkanReflection.cpp:128` | 生成 Vulkan 的 binding / attribute 描述 |

**为什么这么做**（这一步的三条事实都是反汇编 `slang.spv` 现场验出来的）：

1. **不能"把所有 `Input` 变量当顶点属性"。** 反汇编里片元的插值输入 `%vertIn_fragTexCoord`
   也是 `Input` 存储类、也带 `Location`。混进来就会多出属性和 location 冲突。
   正确做法是只取 Vertex 那个 `OpEntryPoint` 接口列表里的。
2. **不能只反射第一个入口点。** SPIRV-Reflect 的模块级字段注释写着
   `// Uses value(s) from first entry point`。`slang.spv` 里顶点入口排在前面，
   只取模块级会**漏掉片元用的那张贴图**（binding 1）。
3. **SPIR-V 里没有 vertex binding 号和 stride。** 顶点输入只有 `Location` 装饰，
   `OpEntryPoint` 只给类型。所以反射能给出 location 和 format，给不出
   "这个属性属于 0 号 buffer、一笔顶点多少字节" —— 那两样由调用方补。

**`BuildVertexInput` 的排布规则**：offset 按 location 顺序做前缀和，stride 由调用方给。
所以属性必须从 offset 0 起紧密排，"宽松"只能宽松在尾部。这不是理论问题 ——
`flat_color.slang` 只声明 location 0（位置 12 字节），而它的顶点数据是 pos3+uv2 共 20 字节。
按属性自己加起来会算成 12，读到的位置就是错的。

**怎么验的**：临时在 `VulkanRendererAPI::Init` 里跑一次反射，打印结果，和手写的反汇编逐条对：

| 反射出来的 | 反汇编里手核的 |
|---|---|
| 顶点属性 3 条，location 0/1/2，format 103/106/103 | `input_inPosition`(v2float) / `input_inColor`(v3float) / `input_inTexCoord`(v2float) |
| offset 0/8/20，stride 28 | `struct Vertex` = vec2(8) + vec3(12) + vec2(8) |
| 描述符 2 条：binding 0 type 6(VERTEX) + binding 1 type 1(FRAGMENT) | `ubo`@0 / `texture`@1 |
| 片元入口的 Input **1 条**（location 1，float2） | `vertIn_fragTexCoord` —— 没混进顶点属性 |

**这一步踩的坑**：`vk::blockSize` 在这个 SDK 的 vulkan-hpp 里不存在（bug 4）。

---

### S3 · 写 PipelineDesc + PipelineCache

**做了什么**：新建 `VulkanPipelineCache.{h,cpp}`（头 117 行）。

```cpp
// VulkanPipelineCache.h:37
struct PipelineDesc
{
    vk::ShaderModule vertexModule   = nullptr;
    vk::ShaderModule fragmentModule = nullptr;
    std::string_view vertexEntry    = "vertMain";
    std::string_view fragmentEntry  = "fragMain";
    uint32_t         vertexStride   = 0;          // 反射给不出来,调用方补
    vk::PrimitiveTopology topology;  vk::CullModeFlags cullMode;
    vk::FrontFace frontFace;         vk::PolygonMode polygonMode;
    float lineWidth;                 vk::SampleCountFlagBits samples;
    vk::PipelineColorBlendAttachmentState blendState;   // 整块传
};
```

`PipelineCache::GetOrCreate(desc, reflectedVertexInput)`（`VulkanPipelineCache.cpp:99`）
命中就返回已有的，没有就建一条存下来。

**为什么这么做**

- **`vk::ShaderModule` 当 key，不用 `Ref<Shader>`。** 查找在每帧 400+ 次的路径上，
  `shared_ptr` 会带引用计数原子操作。句柄是 uint64 比较。
- **`std::string_view` 而不是 `const char*` 存入口名。** 后者比的是指针，
  两个翻译单元里的 `"vertMain"` 不保证同一个地址。
- **`blendState` 整块传，不拆成 bool + 6 个 enum。** 混合的字段之间没有约束，
  拆开只是把 vk 结构体重抄一遍。
- **只放"逐条管线会变"的东西。** 描述符集布局、push constant 范围、颜色附件格式
  这三样在一次运行里是恒定的，所以放在 `PipelineCache` 的**构造参数**里 ——
  放进 `PipelineDesc` 意味着每帧 400 多次绘制都要复制三个 vector。
  要多套附件格式（阴影 pass、后处理）时，按配置各建一个 cache。
- **顶点输入也只放 stride，不放整个 `VulkanVertexInput`。** 后者带着 vector，
  放进 key 就等于每次查找都比一遍 vector。反射结果缓存在 shader 那边，
  只在未命中时才用得上。

**怎么验的**：临时跑——
① 同一 desc 两次 `GetOrCreate` 返回同一对象（打地址比较）→ `true`；
② 换 stride 后返回不同对象 → `true`。
而且这一步**真的建出了一条 graphics pipeline**，验证层没报 —— 建管线时它会比对
`VkGraphicsPipelineCreateInfo`，这条通过说明"反射 → `BuildVertexInput`"这条链是自洽的。

**这一步踩的坑**：`vk::raii::DescriptorSetLayout` 没有默认构造函数（bug 5）、
匿名命名空间里的 `operator==` 会和 vulkan-hpp 生成的在 ADL 上撞（bug 6）。

---

### S4 · 重做 DescriptorAllocator

**做了什么**：`VulkanDescriptorAllocator.{h,cpp}` 整体重写。

| 改前 | 改后 |
|---|---|
| `DescriptorAllocator(context, maxSets)`，构造函数里拼死 layout | 默认构造，layout 由 shader 反射登记（`AddShaderBindings`，`.cpp:25`） |
| `allocate(Buffer&, ImageView&, Sampler&)` 分配即写入 | 拆成 `allocateSet(set)` / `writeUniformBuffer` / `writeCombinedImageSampler` |
| layout 写死 binding 0=UBO、binding 1=sampler | 从并集建，每个 set 号一套（`BuildLayout`，`.cpp:72`） |
| pool 在构造函数里建 | 推迟到第一次分配（`BuildPool`，`.cpp:116`） |
| `UniformBufferObject{model,view,proj}` 放在这个头里 | 挪去 `VulkanFrameData.h`，改名 `ObjectUbo{model}` |
| 自由函数 `updateUniformBuffer(extent, ubo)` | **删掉**（零调用者，里面是写死的旋转 + lookAt 演示代码） |

**为什么这么做**

- **layout 由反射给。** 引擎定不了 shader 声明了什么。代价是它得等到第一个 shader
  出现才存在，所以这个类变成延迟初始化。
- **`allocate` 拆开是因为两条路径的写时机不同**：UBO 分配一次之后只改 dynamic offset、
  不再写 set；贴图分配 + 当场写一次之后缓存住。
- **`range` 必须是一个元素的大小，不能写 `VK_WHOLE_SIZE`。** 这条原计划写反了，
  见第五节 bug 10。

**顺带**：`VulkanFrameData` 的描述符集从构造函数里拿出来（`EnsureUboSet`，`.cpp:39`），
因为它要的 layout 那时还不存在。

**怎么验的**：临时跑——① 反射建 layout 成功（2 条绑定）；② 同样绑定重复调是空操作；
③ 绑定不一致时抛异常；④ UBO 描述符集分配 + 写入成功。

**注**：这一步我写的"绑定必须完全一致"，到 S6 才发现是错的（bug 7），改成了并集。
下面 S6 里写的是最终形状。

---

### S5 · 写 5 个适配类

**做了什么**

| 类 | 文件 | 说明 |
|---|---|---|
| `VulkanShader : Shader` | `VulkanShaderAdapter.{h,cpp}` | 一个 `.spv` 两个入口点共用一个 module |
| `VulkanVertexBuffer : VertexBuffer` | `VulkanVertexArray.{h,cpp}` | |
| `VulkanIndexBuffer : IndexBuffer` | 同上 | 索引统一 `uint32_t` |
| `VulkanVertexArray : VertexArray` | 同上 | |
| `VulkanTexture2D : Texture2D` | `VulkanTexture2D.{h,cpp}` | |

**为什么单开 `VulkanShaderAdapter.*` 而不是塞进已有的 `VulkanShader.*`**：
后者里的 `shader` 是全后端共用的工具类（读文件 + 建 module），
混在一起会让人以为它们有关系。

**形状上的两个点**：

- `Bind()` / `UnBind()` 全是**空实现**。Vulkan 里没有"绑定"这个状态 ——
  管线在录制时才 bind，顶点缓冲是 `vkCmdBindVertexBuffers` 命令，贴图是描述符集里的绑定。
- `VertexBuffer::GetLayout` / `SetLayout` **留着但存下来的 layout 没人读**。
  顶点属性从 shader 反射来，接口要求的这两个函数是装饰性的。

**顺带加的**：`Buffer::createDeviceLocal(data, size, usage, context, transientPool)`
（`VulkanBuffer.cpp:89`）。现有的 `createVertexBuffer` / `createIndexBuffer` 是它的特例
（顶点数据不必是某个具体结构体、索引不必固定 uint16），留着不动。

**怎么验的**：临时把 5 个各构造一遍——
shader 反射出 3 条顶点属性、模块句柄非空；顶点/索引/顶点数组建好（stride 20、索引 6）；
`Checkerboard.png` 读出 64×64；加第二个顶点缓冲时正确抛异常；
layout 建好后分配 UBO 描述符集通过。

**这一步顺带改了一处签名**：`VulkanVertexBuffer` 的 `size` 参数用 `size_t` 而不是 `uint32_t`。
调用方传的几乎总是 `sizeof(...)`，收窄成 `uint32_t` 会在 `make_shared` 的模板路径上报
`C4267`（"从 size_t 转换到 uint32_t，可能丢失数据"）—— 这条警告在直接函数调用里不出现，
只在模板转发时出现，所以很容易到 S7 才被发现。

---

### S6 · 帧槽环形缓冲 + 新 shader + DrawFrame 录制路径

这是最大的一步，内部又分了 5 小步。

#### S6a · 环形缓冲

```cpp
// VulkanFrameData.h:20
struct ObjectUbo { glm::mat4 model; };          // 64 字节,每物体一份

// VulkanFrameData.h:32
struct PushConstants { glm::mat4 viewProj; glm::vec4 color; };   // 80 字节
```

`FrameData` 里：`m_uniformBuffer` 从"一份"变成 `k_MaxObjectsPerFrame(1024) × m_uboStride`，
构造时**整块映射一次**，基址存 `m_uboMappedBase`；
`PushObjectUbo(model)`（`.cpp:56`）写第 n 份并返回它的 dynamic offset，每帧开头 `ResetObjects()`。

**为什么 view/proj 走推送常量、model 走 UBO**

- `Renderer::SceneData` 里本来就只有**一个** `ViewProjectionMatrix`（`Renderer.h:58-61`），
  不需要分开放 view 和 proj —— 推送常量只要 64 字节而不是 128。
- 推送常量的规范下限是 128 字节（`maxPushConstantsSize`），加一个 `vec4 color` 到 80，
  还剩 48 字节余量。放 UBO 里每个物体都要重复写那 128 字节，纯浪费。
- **stride 从 `physicalDevice.limits.minUniformBufferOffsetAlignment` 现查**，不写死 64。
  `sizeof(ObjectUbo)` 恰好 64，本机两个候选设备（RTX 4060 = 64、AMD 780M = 16）
  都能整除，所以没有浪费 —— 这正是选"只放 model"的收益。

**不要用 `Buffer::map(offset, size)` 写环形缓冲** —— 它内部缓存了指针，
第二次调用返回同一个地址。整块映射一次、各物体自己算偏移。

#### S6b · 两个新 shader

`flat_color.slang` / `texture.slang`，每个编成一个 `.spv`，里面带 `vertMain` + `fragMain`。
`compile.bat` 改成编多个。

**这一步做的一个实验**：Slang 会不会把**声明了但没用**的 `Sampler2D` 优化掉？
反汇编结论：**会**。`flat_color.spv` 里根本没有 `Binding 1`，反射出来只有 1 个绑定。
这个事实直接决定了描述符 layout 不能要求"每个 shader 绑定一致"（bug 7）。

#### S6c · DrawItem + 队列

```cpp
// RendererAPI.h:18
struct DrawItem
{
    Ref<Shader>      shader;
    Ref<VertexArray> vertexArray;
    Ref<Texture2D>   texture;      // 可以为空:不采样的管线
    glm::mat4        transform;
    glm::vec4        color;        // 只有纯色管线读它
};
```

`Renderer::Submit` 只往 `s_DrawQueue` 里 push；`Renderer::DrawFrame()` 把队列和
`ViewProjectionMatrix` 交给后端后清空。`RendererAPI::DrawFrame()` 改成
`DrawFrame(const std::vector<DrawItem>&, const glm::mat4&)`，`DrawIndexed` 删掉。

**为什么 `DrawItem` 里不能有 `vk::` 类型**：`RendererAPI.h` 会被 Playground 间接包含，
而 Playground 的 vcxproj 没有 Vulkan 的编译定义（`diary.txt` 记过这条约束）。

**为什么贴图变成 Submit 的参数**：Vulkan 里没有 `Bind(slot)` 那种全局状态，
贴图是"跟着这一次绘制走"的输入。原来那两行 `m_Texture->Bind()` 删掉了。

#### S6d · 两个描述符集 + 录制循环

**描述符集从 1 个拆成 2 个**：set 0 = UBO（dynamic offset），set 1 = 贴图。
原因是 `vkCmdBindDescriptorSets` 对**同一个 set 号**连绑两次，第二次会**顶掉**第一次 ——
UBO 每个物体都要换偏移重绑，没法和一个"只在换图时才重绑"的绑定挤在一个 set 里。

顺带把缓存粒度做干净了：set 0 每帧槽一份（2 个），set 1 每张贴图一份
（`TextureDescriptorCache`，按 `(view, sampler)` 缓存）。不用给 (帧槽 × 贴图) 做组合键。

`DrawFrame` 的录制循环（`VulkanRendererAPI.cpp:240`）：

```
for each item:
    降到后端实现 (dynamic_pointer_cast)
    组 PipelineDesc → PipelineCache.GetOrCreate
    管线变了才 bindPipeline
    推推送常量 (viewProj + color)
    写环形 UBO → 取 dynamic offset
    绑 set 0 (每物体一次,offset 不同)
    贴图变了才绑 set 1
    顶点数组变了才绑顶点/索引缓冲
    drawIndexed
```

#### S6e · 验证

**这一步的验证花的时间最长，因为撞了三个 bug（7、8、9）**。中间靠一套临时打点定位，
见第五节 bug 9 的"诊断手法"。

最终数字：**456 fps，每帧 404 个绘制项全部画出去、0 跳过，0 条验证层输出**。

---

### S7 · Renderer::Create*

**做了什么**：`Renderer` 上新增 5 个静态工厂（`Renderer.cpp:97-148`）。

```cpp
static Ref<Shader>       CreateShader(const std::string& name, const std::string& spvPath);
static Ref<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, uint32_t stride);
static Ref<IndexBuffer>  CreateIndexBuffer(const uint32_t* indices, uint32_t count);
static Ref<VertexArray>  CreateVertexArray(const Ref<VertexBuffer>&, const Ref<IndexBuffer>&);
static Ref<Texture2D>    CreateTexture2D(const std::string& path);
```

**为什么放这儿而不是别的两个位置**

- **不放 `RendererAPI` 虚函数**：那等于把"引擎有哪几种资源"写进后端接口，
  加一种资源要动所有实现，包括已经没人维护的 `OpenGLRendererAPI`。
  而且 `RendererAPI` 现在全是"动作"函数（Init / DrawFrame / SetClearColor），
  没有一个是"创建"。
- **不扩展 `Shader::Create` 那批现有工厂**：它们是 OpenGL 形状的（`float*` + 字节数、
  返回裸指针、没有 `BufferLayout`），而且 Vulkan 侧建资源要 device 和一次性命令池，
  静态工厂拿不到 —— 每个工厂都得自己 `dynamic_cast` 一遍。
  这是个取舍，选了"新开一个干净入口"。

实现靠 `dynamic_cast<VulkanRendererAPI*>(s_RendererAPI)` 拿具体后端 ——
和已有的 `GetDeviceContext()`（`Renderer.cpp:88`）一个套路。

**签名上的一处**：`size` 用 `size_t` 不用 `uint32_t`。调用方传的几乎总是 `sizeof(...)`，
收窄成 `uint32_t` 会在模板路径上报 `C4267`。

---

### S8 · Playground 改造

- `Playground.cpp` 的两段内联 GLSL 删掉，改成从 `.spv` 文件加载。
- `m_Shader`（原来那个建了从来没 Submit 过的 VertexPosColor shader）**删掉**。
- `dynamic_pointer_cast<OpenGLShader>` 上传 uniform 的几处删掉。
- `ShaderLibrary` 不再用（它内部走 `Shader::Create`，Vulkan 下没分支）。
- 两个 `PushLayer` 放开。
- **保留原来的三角形**（pos3+color4，stride 28）并把它画出来。

保留三角形是故意的：它 stride 28、方块 stride 20、`playground2D` 的三角形 stride 12 ——
**三种 stride 走三条管线**，"多格式机制"因此被真的走到了，而不是停在纸面上。

402 次 Submit（400 方块 + 2 贴图）之外的记账：每帧实际是 **404 个绘制项**
（400 + 2 + 三角形 1 + playground2D 1）。

---

## 二、最终形状

```
全局（程序期）:
  DescriptorAllocator         layout 按所有 shader 声明的绑定取并集,每个 set 号一套
                              池推迟到第一次分配时建
  TextureDescriptorCache      每张贴图一个 set(set 1),按 (view, sampler) 缓存
  PipelineCache               按 PipelineDesc 缓存 VkPipeline,一条 pipeline layout

每帧槽 FrameData[i] (i∈{0,1}):
  Buffer m_uniformBuffer      1024 × 对齐后 stride,构造时整块映射
  DescriptorSet m_uboSet      set 0,binding 0,range = sizeof(ObjectUbo)
  CommandBuffer / Semaphore / Fence

每帧 DrawFrame(items, viewProjection):
  每物体: 推推送常量(80B) → 写环形 UBO → 绑 set 0(dynamic offset) → drawIndexed
  变了才发: bindPipeline / 绑 set 1 / 绑顶点索引缓冲
```

---

## 三、关键设计决定（不要重新推导）

1. **描述符 layout 取并集，不要求每个 shader 声明一致。**
   Slang 会把没用到的 `Sampler2D` 整个删掉（编译后反汇编验过），
   所以 `flat_color` 反射出来只有 1 个绑定、`texture` 有 2 个。
   如果要求一致，layout 长什么样就取决于**先建哪个 shader** —— 那是假自由度。

2. **布局/池/pipeline layout 全部延迟初始化，触发点是"第一次分配描述符集"。**
   描述符 layout 来自 shader，而 shader 是应用层的 layer 构造时才创建的，
   比 `VulkanRendererAPI::Init` 晚。整条依赖链：
   `shader → 描述符 layout → 池 + pipeline layout → 描述符集 / 管线`。
   `VulkanRendererAPI::EnsureRenderState()`（`VulkanRendererAPI.cpp:139`）是唯一入口。

3. **UBO 和贴图分在两个描述符集。** 见 S6d。
   引擎侧的约定常量在 `VulkanDescriptorAllocator.h:21-25`
   （`k_ObjectUboSet=0` / `k_ObjectUboBinding=0` / `k_TextureSet=1` / `k_TextureBinding=0`），
   它们**不参与建 layout**，只说明"往哪儿写"。

4. **`FrameData` 的环形缓冲整块映射一次，不用 `Buffer::map(offset, size)`。**
   后者内部缓存指针，第二次调用返回同一地址（`BUFFER_REFACTOR_TODO.md` 第二节第 6 条
   定的设计决定），不适用于"每帧写很多个不同偏移"。

5. **`PipelineCache` 的 `GetOrCreate` 返回引用，只能当帧用完就丢。**
   `unordered_map` 扩容会让引用悬空。这条写进注释了。

6. **`vkCreateGraphicsPipelines` 的 `pipelineCache` 参数继续传 `nullptr`。**
   那是驱动级持久化缓存（读盘写盘、绑驱动版本），和 `PipelineCache`（内存里的
   desc→pipeline 表）是两回事，不要为名字像就接上。

7. **`VulkanPipelineCache::Invalidate` 现在没有调用者。**
   `SwapChain::recreate()` 不通知任何人。本次只让缓存**具备**被通知的能力，没改 `SwapChain`。
   分两层说：**规范保证的**是"format 变了旧管线就不兼容"；**实测碰巧的**是这台机器
   resize 之间 format 从没变过（`chooseSwapSurfaceFormat` 每次挑同一个）。

---

## 四、验证结果

| 项 | 结果 | 怎么验的 |
|---|---|---|
| 构建 | **0 错误 0 警告** | `cmake --build build --config Debug` |
| 稳态 | 20 秒 **0 条验证层输出** | stdout + stderr 全重定向到文件 |
| 真在干活 | 456 fps；每帧 **404 个绘制项全部画出去、0 跳过** | 临时帧统计 |
| CPU | 12 秒墙钟烧 **11.92 秒 CPU**（满载） | PowerShell 读进程 `TotalProcessorTime` |
| 退出 | `WM_CLOSE` → 正常退出，**stderr 空** | 脚本先断言窗口句柄非 0 再 `PostMessage(0x0010)` |
| resize | 客户端 1600x900 → 784x561 → 1484x961，**全程 stderr 空** | 先断言客户端尺寸真的变了，否则判 vacuous |

### 画面

**已截屏确认**（做法见第八节第 6 条）。2026-09-23 截到的三张：

| 配置 | 看到什么 |
|---|---|
| 全开 | 400 个蓝紫色方块组成的 20×20 网格 + 两张贴图 + `playground2D` 的方块 |
| 只关掉 400 个方块的循环 | 棋盘格和 Cherno logo 叠在一起（两张贴图都在） |
| 只关掉 `playground2D` | 网格 + 棋盘格 + logo 都清楚可见 |

**仍未验的路径**：

- 多 binding 的顶点输入（现在所有顶点数据都只用一个 binding）
- `PipelineCache::Invalidate` 路径（没有调用者）
- 深度测试（管线里 `pDepthStencilState = nullptr`）
- **Y 轴朝向**：Vulkan 的 framebuffer 空间 Y 朝下，而 `glm::ortho` 出的是 OpenGL 约定，
  所以整个场景在 Vulkan 路径下是**上下翻的**（网格出现在右下方而不是右上方）。
  原型里的 `updateUniformBuffer` 是靠 `proj[1][1] *= -1` 补偿的，本次没搬。
  这是个待你定的取舍：翻在哪一层（后端录制时补偿 / 改相机）见第六节

---

## 五、这次撞到的 bug

**编号按重要性排，不按发现顺序** —— 因为最重要的几条要放在最前面。
下面这张表是全量索引，正文按编号从 7 往下排：

| # | 一句话 | 类别 |
|---|---|---|
| **7** | 描述符绑定取并集时只比 binding 号，没比 set 号 | 逻辑错误（现场 bug） |
| **8** | `ResetObjects()` 定义了但从来没被调用 | 逻辑错误（现场 bug） |
| **9** | 异常从 `DrawFrame` 逃出去之后进程既不退出也不打印 | 诊断陷阱 |
| **10** | `VK_WHOLE_SIZE` 用在 dynamic offset 上是错的 | **计划写错** —— 实验推翻 |
| **11** | 退出时 23 个对象泄漏（`VUID-vkDestroyDevice-device-05137`） | 析构顺序 |
| **12** | 修完 11 之后，资源在 GPU 还在用时被销毁 | 析构顺序 |
| **1** | `spirv_reflect.c` 被预编译头打到（`STL1003`） | 构建配置 |
| **2** | `spirv_reflect.h` 的默认 include 路径是从 SDK 源码树推的 | 构建配置 |
| **3** | `vk::ShaderStageFlags` 不是枚举，`static_cast<int>` 转不了 | API 细节 |
| **4** | `vk::blockSize` 在这个 SDK 的 vulkan-hpp 里不存在 | API 细节 |
| **5** | `vk::raii::DescriptorSetLayout` 没有默认构造函数 | API 细节 |
| **6** | 匿名命名空间的 `operator==` 和 vulkan-hpp 的在 ADL 上撞 | API 细节 |
| **13** | `GLOB_RECURSE` 无 `CONFIGURE_DEPENDS`，且失败被 `&&` 短路掩盖 | 构建配置 |
| **14** | `cullMode = eBack` 把所有图元剔光，画面上只剩清屏色 | **Vulkan 约定差异** |

### bug 7 · 描述符绑定的并集只比了 binding 号，没比 set 号

**现象**：建第二个 shader 时进程卡住，日志停在 `texture shader` 之前。

**根因**：`AddShaderBindings` 里找"已有绑定"时只比了 `b.binding == incoming.binding`。
UBO 在 `(set 0, binding 0)`，贴图在 `(set 1, binding 0)` —— 被判成同一个绑定的两种类型，
于是抛"同一个 binding 被声明成两种不同的描述符类型"。

**修法**：改成比 `(set, binding)` 两个数。顺带把排序也改成按 `(set, binding)` ——
`BuildLayout` 靠 `m_bindings.back().set + 1` 判断有几套 layout，
只按 binding 排的话最后一个元素的 set 未必最大。

**为什么值得记**：这是"一个东西的标识由两个数组成"的经典写法错误。
`set` 是后加进设计的（原来只有 set 0），加的时候忘了它也是标识的一部分。

---

### bug 8 · `ResetObjects()` 定义了但从来没被调用

**现象**：跑到第 5 帧卡死。

**根因**：环形缓冲的写游标 `m_objectCount` 跨帧一路累加。
- 帧 1（slot 0）：0 → 404
- 帧 2（slot 1）：0 → 404
- 帧 3（slot 0）：404 → 808
- 帧 5（slot 0 第二次）：808 → 到第 217 个时超过 `k_MaxObjectsPerFrame(1024)` → 抛异常

**修法**：在录制循环前调 `frame.ResetObjects()`。

**为什么值得记**：`k_MaxObjectsPerFrame` 这个断言**本来就是为了抓这类错**，
它确实抓到了 —— 问题是异常没被接住，表现成"卡死"而不是崩溃，
所以第一眼看不出是断言。中间我一度以为是 fence 死等（见 bug 9）。

---

### bug 9 · 异常从 `DrawFrame` 逃出去之后，进程既不退出也不打印

这条不是代码 bug，是**诊断陷阱**，两次踩到（bug 7 和 bug 8 的现象都是它造成的）。

**现象**：进程活着、CPU 占用极低（0.75 秒/10 秒）、stdout/stderr 什么都没有、
`timeout` 到点才把它杀掉。看起来像死锁。

**根因**：`std::runtime_error` 从 `VulkanRendererAPI::DrawFrame` 一路逃到 `main()`，
没有 try/catch。留在这里的表现是进程吊着，没有任何输出。

**怎么绕过去的**（这套手法以后可以复用）：在 `DrawFrame` 里按顺序埋临时打点 ——

```
进入 DrawFrame #n  →  EnsureRenderState 完成 #n  →  等 fence 前 #n  →  等 fence 后 #n
                  →  acquire 返回 #n imageIndex=?  →  帧末统计
```

一跑就看出"第 5 帧过了 acquire 就不动了"，范围立刻缩到录制/提交/呈现那一段。
**如果只在帧尾打一条，就只能看到"第 4 帧之后没有了"，什么也定位不了。**

**为什么值得记**：`CLAUDE.md` 记的那三类"验证要到什么程度"里，
"跑十几秒无验证层输出"这条**在这两种情况下都会通过** ——
进程活着、验证层确实没输出。**得再加一条"证明它真在干活"**，也就是 CPU 时间那条。
这次正是 CPU 时间 0.75s/10s 那个异常数字把方向带对的。

---

### bug 10 · `VK_WHOLE_SIZE` 用在 dynamic offset 上是错的

**现象**：跑起来画面上是错的（更重要的是验证层直接报错）。

**根因**：原计划写的是 UBO 描述符 `range = VK_WHOLE_SIZE`，
理由是"避免手写 64 这个数字和 shader 侧脱钩"。**这是错的。**

```
vkCmdBindDescriptorSets(): pDynamicOffsets[0] is 64, but must be zero since the buffer
descriptor's range is VK_WHOLE_SIZE in descriptorSet #0 binding #0 descriptor[0].
VUID-vkCmdBindDescriptorSets-pDescriptorSets-06715
```

规范原话：range 用 `VK_WHOLE_SIZE` 时，对应的 dynamic offset **必须是 0**。
那样环形缓冲就退化成只有一个槽。

**修法**：`range = sizeof(ObjectUbo)`。VUID 记进注释了（`VulkanFrameData.cpp:45-48`）。

**为什么值得记**：这条是**计划里写错、实施时验出来**的。
如果当时没做这个实验，代码会"能跑"（画面上只有第一个物体的 model 是对的），
而且只有验证层会报 —— 属于不看验证层输出就发现不了的那类。

---

### bug 11 + 12 · 退出时资源泄漏，修完变成资源在 GPU 还在用时被销毁

**现象（11）**：`WM_CLOSE` 正常退出时验证层报
`VUID-vkDestroyDevice-device-05137`，**23 个对象泄漏**（VkBuffer / VkDeviceMemory）。

**根因（11）**：`LayerStack` 是 `Application` 的**成员**，成员析构发生在
析构函数体**之后**。而 `~Application()` 的函数体里调 `Renderer::Shutdown()` 销毁设备。
所以顺序是：**设备先没 → layer 的 GPU 资源后销毁**。资源那时已经销毁不了了。

**修法（11）**：`Application::~Application()` 里显式 `m_LayerStack.Clear()`
（新加的方法，`LayerStack.cpp:15`）再 `Renderer::Shutdown()`。

**现象（12）**：修完 11，新的报错来了 ——
`VUID-vkDestroyBuffer-buffer-00922` / `VUID-vkDestroySampler-sampler-01082`，
"资源正被 VkCommandBuffer 使用中"。

**根因（12）**：最后一帧的命令缓冲还在飞，它引用着 layer 的顶点缓冲和贴图。
顺序等于是：**GPU 还在用 → 资源被销毁**。

**最终顺序**：`Renderer::WaitIdle()` → 销毁 layer → 销毁设备。
顺带加了 `RendererAPI::WaitIdle()`（虚函数，空实现给待删的 OpenGL 用）——
放在 `RendererAPI` 上而不是让 `Application` 直接碰 device，
因为"是不是 Vulkan"是后端细节，`Application` 不该知道。

**为什么值得记**：这是 `CLAUDE.md` 第三节第 4 条说的那类问题 ——
**只在析构路径上暴露**。用 `taskkill /F`（`TerminateProcess`，不跑析构函数）
或者只看"稳不稳定"都发现不了，**必须走 `WM_CLOSE` 正常退出并且看 stderr**。
而且修了第一层才暴露出第二层，两层的原因完全不同。

---

### bug 14 · `cullMode = eBack` 把所有图元剔光，画面上只剩清屏色

**现象**：跑起来窗口是一片纯色（`89,89,89`，正好是 `SetClearColor({0.1,0.1,0.1,1})`
的 sRGB 编码值）。而验证层**一条都不报**，404 个 `drawIndexed` 也确实发出去了。

**根因**：Vulkan 的 framebuffer 空间 **Y 轴朝下**，OpenGL 朝上。`glm::ortho` 出的是
OpenGL 约定的矩阵，投到 Vulkan 里三角形的绕序就翻了 —— 原本的正面变成了背面，
`cullMode = eBack` 把它们全剔掉。管线里没有任何一处会因此报错，
**这是纯约定差异，不是 API 误用**。

**修法**：2D 一律 `cullMode = eNone`（`VulkanPipelineCache.h:54`）。修完立刻能看到
400 个方块、两张贴图和三角形。

**为什么值得记**：它同时是"验证层干净 ≠ 画面对"的最干净例子。
这一轮我按 `CLAUDE.md` 第三节的清单验完了构建、稳态、退出、resize、CPU ——
**全部通过**，而画面是空的。清单里唯一没覆盖的就是"画面本身"。

**顺带澄清一个当时被误判的现象**：关掉剔除之后看截图，中间那块只有纯蓝，
一度以为"贴图和三角形没画出来"。实际是 `playground2D` 那一层的方块 ——
它的 `m_SquareColor` 和 `ExampleLayer` **一模一样**，1.5 倍大、没有深度测试、
画在最后 —— 把贴图完整盖住了。把那一层关掉就全露出来了。
**不是 bug，是两层各画各的完整场景必然重叠。**

---

### bug 1 · `spirv_reflect.c` 被预编译头打到

**现象**：`yvals_core.h(23,1): error C1189: STL1003: Unexpected compiler, expected C++ compiler`。

**根因**：`target_precompile_headers(Fish PRIVATE "Fish/src/fspch.h")`
会给 Fish 目标下**所有**源文件加 `/FI fspch.h` —— 包括这个 `.c`。
`fspch.h` 里全是 STL，C 编译器吃到就报错。

**修法**：给这一个文件关掉 PCH：
```cmake
set_source_files_properties(Fish/vendor/SPIRV-Reflect/spirv_reflect.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
```

---

### bug 2 · `spirv_reflect.h` 的默认 include 路径是从 SDK 源码树布局推的

**现象**：`error C1083: 无法打开包括文件: "./include/spirv/unified1/spirv.h"`。

**根因**：头文件里是 `#include "./include/spirv/unified1/spirv.h"`，
按 SDK 自己的 `Source/SPIRV-Reflect/` 目录布局写的。只拷两个文件过去就断了。

**修法**：头文件本来就留了开关 ——
定义 `SPIRV_REFLECT_USE_SYSTEM_SPIRV_H` 就走 `<spirv/unified1/spirv.h>`，
而 SDK 的 `Include/spirv/unified1/spirv.h` 跟着 `Vulkan::Vulkan` 的 include 路径一起进来。
**不另外拷一份 `spirv.h`**：它必须和 `vulkan_core.h` 同版本。

---

### bug 3 · `vk::ShaderStageFlags` 不是枚举，`static_cast<int>` 转不了

**现象**：`error C2440: 无法从 const vk::ShaderStageFlags 转换为 int`（编译期）。

**根因**：`vk::ShaderStageFlags` 是 `vk::Flags<ShaderStageFlagBits>` 包装类，
不是枚举，没有到整型的直接转换。

**修法**：转两层 —— `static_cast<uint32_t>(static_cast<VkShaderStageFlags>(flags))`。
（这条出现在临时打点里，但同样的转换以后写日志/断言还会用到。）

---

### bug 4 · `vk::blockSize` 在这个 SDK 的 vulkan-hpp 里不存在

**现象**：`error C2039: "blockSize": 不是 "vk" 的成员`。
（`vulkan_format_traits.hpp` 里的 `blockSize` 是较新版本才有的。）

**修法**：不维护格式→字节数的表，直接从反射出的数值特征算：
`size = (numeric.scalar.width / 8) * vector.component_count`。
顺带挡掉矩阵：SPIR-V 里矩阵顶点属性是每个 location 一个列向量、
反射出来 `numeric.matrix.column_count != 0`，按标量×分量算会得到错误的字节数
而且**验证层不报**，所以那里直接抛异常。

---

### bug 5 · `vk::raii::DescriptorSetLayout` 没有默认构造函数

**现象**：`error C2672: std::construct_at 未找到匹配的重载函数`，
展开后是 `vk::raii::DescriptorSetLayout::DescriptorSetLayout(void)` 被隐式删除。

**根因**：`std::vector<vk::raii::DescriptorSetLayout>::resize()` 需要默认构造。

**修法**：先建到一个临时 vector（`reserve` + `emplace_back`），再整体 `move` 赋值给成员。

---

### bug 6 · 匿名命名空间里的 `operator==` 会和 vulkan-hpp 的在 ADL 上撞

**修法**：本来想给 `vk::PipelineColorBlendAttachmentState` 写一个逐字段比较的
`operator==`。vulkan-hpp 给结构体也生成了 `operator==`，两个同名同签名、
不同命名空间，`a.blendState == b.blendState` 会歧义。
改成普通函数名 `SameBlendState(a, b)`，绕开重载解析。

**为什么不用 `memcmp`**：结构体里可能有填充字节，内容不确定。

---

### bug 13 · `GLOB_RECURSE` 没有 `CONFIGURE_DEPENDS`，而且失败被 `&&` 短路掩盖了

**现象**：新加的 `VulkanReflection.cpp` 没进构建，
链接时报 `LNK2019: 无法解析的外部符号 Fish::ReflectVertexInput...`。

**根因**：两层。
① `CMakeLists.txt:74` 的 `file(GLOB_RECURSE FISH_SOURCES ...)` **没有 `CONFIGURE_DEPENDS`** ——
增删源文件后必须重跑 configure。这条 `CLAUDE.md` 第三节已经记了，还是踩了。
② 当时我把命令写成 `cmake -S . -B build > /dev/null 2>&1 && cmake --build ...`，
**configure 失败被 `&&` 短路，整个构建没跑**，而输出被 `/dev/null` 吞了，
grep 的结果是"什么都没有"，看起来和"构建成功且无警告"一模一样。

**修法**：configure 和 build 分开跑、输出不重定向到 `/dev/null`。

**为什么值得记**：②是新的教训 —— **把"没输出"当成"通过了"**。
`grep -iE "error|warning"` 在"构建根本没跑"和"构建成功"两种情况下输出相同。
这跟 bug 9 是同一类错误：**验证手段本身无效**。

---

## 六、遗留 / 待定

### 死代码 —— 2026-09-23 已清理

**删掉的（都确认过零引用）：**

| 东西 | 说明 |
|---|---|
| `m_MainTexture` + 它的 `texture::loadFromFile` 调用 | 加载了但没人用（原来给旧 `Frames` 绑贴图） |
| `VulkanPipeline.h` / `VulkanPipeline.cpp` | 整个 `Pipeline` 类，被 `PipelineCache` 取代 |
| `recordCommandBuffer`（`VulkanCommandPool.{h,cpp}`） | 零调用者。它是 `VulkanPipeline` 唯一的引用者，一起走 |
| `shaders/slang.spv` + `shaders.slang` | 没有引用了，`compile.bat` 里也早就没编它 |
| `Buffer::createVertexBuffer` / `createIndexBuffer` | 被 `createDeviceLocal` 取代（索引不必再固定 uint16） |
| `struct Vertex` + `getBindingDescription` / `getAttributeDescriptions` | 反射取代了引擎侧那份顶点布局定义。上面两条删完它就没人引用了 |
| `SwapChain::imageCount()` | 零调用者 |
| `VulkanContext::checkDeviceExtensionSupport()` | 零调用者（`isDeviceSuitable` 走的是内联的 `ranges::all_of`） |
| `Image::getFormat()` / `Image::getMemory()` / `Image::getHandle()` | 零调用者（`getExtent()` 留着 —— `VulkanTexture2D::GetWidth/Height` 在用） |
| `Buffer::getSize()` / `Buffer::getMemory()` | 零调用者 |
| `Frames::frameCount()` / `Frames::current() const` | 零调用者 |

**连带修的**：`VulkanFrameData.h` 一直在靠 `VulkanBuffer.h` 传递包含 `glm::glm.hpp`
（`ObjectUbo` 用了 `glm::mat4`）。删掉 `VulkanBuffer.h` 里那个传递包含后编译就断了，
所以给 `VulkanFrameData.h` 补了自己的 `#include "glm/glm.hpp"` ——
**头文件本来就该包含它用到的东西**。

### 还没删的（等你发话）

- `Renderer::GetDeviceContext()`（`Renderer.h:56` / `Renderer.cpp:88`）—— 零调用者，
  用途已经被 `Renderer::Create*` 取代。**删的时候被 Visual Studio 锁住了写不进去**
  （`devenv` 开着，只有 `Renderer.h` / `Renderer.cpp` 这两个文件写不了）。
  下次顺手删掉即可。

> 可执行的待办统一收在**第九节**，这里只留"为什么会有这些遗留"的背景。

---

## 七、验证命令

```bash
cd /d/Fish
cmake -S . -B build          # 增删 Fish/src/ 下的文件后【必须】重跑:
                             # CMakeLists.txt:74 的 GLOB_RECURSE 没有 CONFIGURE_DEPENDS
cmake --build build --config Debug
```

```bash
# 工作目录必须是仓库根 —— 程序读的是 "Fish/src/Platform/Vulkan/shaders/*.spv"
# 和 "Playground/assets/..." 这种仓库根相对路径
./build/bin/DEBUG/Playground.exe
```

改完 shader 要手动重跑编译（没有任何 CMake 规则）：
```bash
cd Fish/src/Platform/Vulkan/shaders
D:\Vulkan_SDK\Bin\slangc.exe flat_color.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -o flat_color.spv
D:\Vulkan_SDK\Bin\slangc.exe texture.slang     -target spirv -profile spirv_1_4 -emit-spirv-directly -o texture.spv
```

验证脚本（PowerShell，纯 ASCII —— PS 5.1 按 ANSI 读 `.ps1`，写中文会被拆坏）：
- `C:\Users\Ivan\AppData\Local\Temp\fish_exit_test.ps1` —— `WM_CLOSE` 退出
- `C:\Users\Ivan\AppData\Local\Temp\fish_resize_test.ps1` —— resize，**断言客户端尺寸真的变了**

---

## 八、这次的方法论教训

1. **"进程活着 + 验证层没输出"不等于在干活。**
   要么加"证明它真在干活"这一条（CPU 时间 / 帧率 / 绘制项计数），
   要么就会被 bug 9 那种"异常吊着"的状态骗过去。

2. **把"没输出"当成"通过了"。**
   `grep error|warning` 在"构建没跑"和"构建成功"下输出一样（bug 13）。
   命令的退出码和"确实执行了"要分开确认。

3. **验证脚本必须断言前置条件真的成立。**
   resize 脚本第一版因为 `-UsingNamespace` 和 `Add-Type` 自动加的 `using` 冲突，
   `MoveWindow` 和 `GetClientRect` 全部抛异常，脚本照样跑完并打出 `FAIL: test is vacuous`
   —— **是那个断言把"测了个寂寞"暴露出来的**。没有它，输出会是"跑完了，没报错"。

4. **边界路径要单独跑，而且每一层修完可能暴露下一层。**
   退出路径修了第一层（资源生命周期）才暴露出第二层（GPU 还在用）。
   一次只修一层、每层都重新验，比一次改一大片再验可靠。

5. **计划里的技术细节也可能是错的。**
   `VK_WHOLE_SIZE` 那条（bug 10）是计划里写错的。实施时实验一下比信计划便宜。

6. **"画面我确认不了"是可以绕开的 —— 截屏。**
   这一轮之前，画面一直是"我看不了、只能让你看"。做法：
   PowerShell 用 `SetWindowPos(HWND_TOPMOST)` 把窗口强制置顶（`SetForegroundWindow`
   会被 Windows 挡掉，`CopyFromScreen` 就会截到压在下面的别的窗口），
   `GetClientRect` + `ClientToScreen` 定位，`Graphics.CopyFromScreen` 抓成 PNG。
   脚本在 `%TEMP%ish_shot.ps1`，**并且输出颜色直方图**：
   只有 2 种颜色 ⇒ 画面是空的（bug 14 就是这么一眼看出来的）；
   上百种 ⇒ 内容画出来了。
   两个坑：① 必须报 `GetWindowText` 的标题，否则可能截错窗口；
   ② 置顶 + 截图之间要等一下，DWM 合成不是瞬时的。


---

## 九、待办总表

分三组：**A 要你拍板的（取舍）/ B 我可以直接做的（没取舍）/ C 已知欠账（等后续里程碑）**。

### A. 要你定的

| # | 事 | 为什么需要你定 | 位置 |
|---|---|---|---|
| **A1** | **Y 轴朝向**：场景现在在 Vulkan 路径下是**上下翻的** | 翻在哪一层是取舍 —— 放后端（录制推送常量时补偿）不影响 OpenGL 路径；放 `OrthographicCamera` 会同时影响两边。原型里的做法是在 `updateUniformBuffer` 里 `proj[1][1] *= -1` | 第四节末尾 |
| **A2** | **`playground2D` 和 `ExampleLayer` 同色重叠** | 两层的 `m_SquareColor` 都是 `(0.2,0.3,0.8)`，都画完整场景，没有深度测试 → 后画的盖住先画的。要么给其中一个换色，要么只开一层。**这是 Playground 的结构问题，不是渲染器的问题** | `Playground.cpp:114` |
| **A3** | **`CLAUDE.md` 第三节那句缩进说明要不要改** | 原话是「`Platform/Vulkan/` 下是 **4 空格**, Fish 原有代码是 tab」—— **反了**。实测 26 个文件里 22 个用 tab，只有 `VulkanContext.{h,cpp}` / `VulkanValidationLayers.{h,cpp}` 用 4 空格 | `CLAUDE.md` 第三节 |
| **A4** | **`PLAN.md` 要不要同步** | 这次一次把 **W4–W9 的活全做完了**（逐周表里它们还标着"未开始"）。而 `PLAN.md` 第六节自己的规矩是「进度快 → 只把当周标成完成，后面几周的定义一个字都不改」。怎么记这笔你定 | `PLAN.md:220-231` |
| **A5** | **提交，以及这份文档要不要纳入版本管理** | 仓库规矩是只在你要求时提交。`PIPELINE_REFACTOR_TODO.md` 现在还是 untracked | — |

### B. 我可以直接做的

| # | 事 | 说明 |
|---|---|---|
| **B1** | 删 `Renderer::GetDeviceContext()` | 零调用者（用途已被 `Renderer::Create*` 取代）。**上次删的时候被 Visual Studio 锁住了** —— `devenv` 开着时只有 `Renderer.h` / `Renderer.cpp` 写不进去，下次做之前先关 VS |
| **B2** | 改窗口标题 | `Fish/src/Core/Window.h:13` 还是 Hazel 模板留下的 `"Hazel Engine"` |
| **B3** | 把写死的 `sizeof(PushConstants)` / `sizeof(ObjectUbo)` 换成反射值 | `VulkanRendererAPI.cpp` 的 push constant range 用 `push_constant_blocks[0].size`；`VulkanFrameData.cpp` 的描述符 `range` 用 `descriptor_binding.block.size`。**注意两者语义不同**：push constant range 只需覆盖，描述符 range 必须是一个元素的大小 |
| **B4** | 把 shader 编译接进 CMake | 现在是手动跑 `compile.bat`（`slangc` 路径还写死 `D:\Vulkan_SDK\Bin`，没有任何 CMake 规则）。加 `add_custom_command` 可以省掉"改完 shader 忘了重编"这个坑 |

### C. 已知欠账

| # | 事 | 归哪 |
|---|---|---|
| **C1** | 深度测试没有 | 管线里 `pDepthStencilState = nullptr`（`VulkanPipelineCache.cpp`） |
| **C2** | `PipelineCache::Invalidate` 没有调用者 | `SwapChain::recreate()` 不通知任何人。现在不出错是**实测碰巧**（本机 resize 之间 format 没变过），不是规范保证 |
| **C3** | 多 binding 的顶点输入没走过 | `BuildVertexInput` 生成的是单 binding。现在所有顶点数据都只用一个 |
| **C4** | ImGui 层没建 | 归 M6。`Playground.cpp` / `playground2D.cpp` 里的 `OnImGuiRender` 现在不会被调用（`Application::run` 里判空跳过） |
| **C5** | `PLAN.md` 那张「画面验收欠账登记」表 | W6/W7/W9 那几行现在**实际达标了**（静态图形能画出来、三角形走通 Submit、贴图四边形走通 Submit），可以结账。C1–C3 属于新的欠账 |

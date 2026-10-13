# 设计复演 · 2026-09-23 那轮

> 建立于 2026-09-24。做法见 `CLAUDE.md` 第二节「协作模式」。
>
> **做法**:不看结论,自己把 2026-09-23 那轮的每个决策点重做一遍。
> 改前的代码在 `HEAD`(`39a1f0d`),用 `git show HEAD:<文件>` 读,**工作区一个字都不用改**。
>
> **揭晓时机**:一个目标内的决策点全部选完,才揭晓当年实际选了什么。
>
> **最终产出**:7 个目标全部走完后,把差异汇总成一份
> **「对现有代码的修改清单」**(见文末)。走完之前不动代码。

---

## 目标清单

| # | 目标 | 状态 |
|---|---|---|
| 1 | 一帧画 400+ 个物体,每个物体自己的 transform 和 color | ✅ 已完成 |
| 2 | 一张场景里两种材质(纯色 / 贴图) | ✅ 已完成 · **无需改代码** |
| 3 | 三种不同 stride 的顶点格式共存(12 / 20 / 28) | ✅ 已完成 · **无需改代码** |
| 4 | 贴图是每次绘制的输入,不再是全局状态 | ✅ 已完成 2026-09-26 · **无需改代码** |
| 5 | 应用层能创建 shader / 顶点缓冲 / 索引缓冲 / 顶点数组 / 贴图 | ✅ 已完成 2026-09-26 · **有修改清单** |
| 6 | DrawFrame 里真的录 draw 命令并提交 | ⬜ |
| 7 | 退出 / 最小化 / resize 不崩、不泄漏 | ⬜ |

---

# 目标 1 · 一帧画 400+ 个物体,每个物体自己的 transform 和 color

**✅ 已完成 2026-09-24**

## 1.1 · per-object 数据(transform 64B)走哪条通路?

**我的选择:B** —— 一份 UBO,每物体一份,用 dynamic offset 索引

**我的理由**:(未给)

**我预期的代价:**
- 需要环形缓冲(按帧槽复用)+ 每物体一次 `vkCmdBindDescriptorSets`
- stride 必须按 `minUniformBufferOffsetAlignment` 对齐,该值因设备而异
- **追加**:400 个物体必须共用同一个 stride;要么规定一个最大 stride,
  小 stride 的物体用空 vec 占位,这样所有物体才能共用一个 UBO

**当年实际:B** ✅ 一致

**差异:** 无。

---

## 1.2 · viewProj 和 color 放哪?

**我的选择:B** —— `model` + `color` 在 UBO,`viewProj` 走推送常量

**我的理由**:model 和 color 是 per-object 的,vp 矩阵是每帧更新的

**我预期的代价:**
- UBO `sizeof` = 80,按 `minUniformBufferOffsetAlignment = 64` 对齐后 **stride = 128**
  (算式:`ceil(80/64) * 64 = 128`)
- 对齐多出来的 48 字节是白占的

**我对 1.1 那条追加代价的回看:**
现在是一致的。之后加其他 uniform 还有 48 字节余量(<48),那时候有的 object 加了
有的没加、size 可能不一样,但每个都能填满 128,仍然按 stride 索引。

**当年实际:C** ⚠️ 分歧 —— `color` 走推送常量,不在 UBO 里

```cpp
// VulkanFrameData.h:22-38
struct ObjectUbo     { glm::mat4 model; };                       // 64B → UBO,stride 64
struct PushConstants { glm::mat4 viewProj; glm::vec4 color; };   // 80B → 推送常量
```

**差异(两条不同的原则):**

| | 原则 | color 归谁 |
|---|---|---|
| 我 | 按**变化频率**分层:per-object 的进 per-object 容器 | UBO |
| 当年 | 按**能不能塞进推送常量**分层:128 字节塞得下就塞 | 推送常量 |

**代价(按 `sizeof` 和本机两台候选设备算):**

| | `sizeof` | RTX 4060(对齐 64) | AMD 780M(对齐 16) |
|---|---|---|---|
| 我(B) | 80 | **stride 128**(浪费 60%) | stride 80(不浪费) |
| 当年(C) | 64 | stride 64 | stride 64 |

400 个物体在 4060 上:我的方案每帧多写 `400 × 64 = 25 KB`。

**我的方案换来的东西**:推送常量只用 64/128,**剩 64**;当年用 80/128,**剩 48**。
**多 16 字节余量 vs 每物体 64 字节带宽。**

**待定**:那 16 字节余量后面打算用来放什么?如果答案是没有,这一处应该改成 C。

---

## 1.3 · 环形缓冲一帧最多装几个物体?超了怎么办?

**我的选择:动态增长 + 双缓冲** —— 不够时在渲染完当前帧后丢弃,下一帧用建好的更大的那块

**我的理由 / 我预期的代价:**
- 双缓冲多申请一块内存
- 每次遇到更多 object 就扔老建新,可能创建太频繁
- **所以要有摊销算法(空间换时间):上限不够时一次性创建一块较大的**,
  避免"差一点就够但还是要重建"

**当年实际:写死上限 + 超了抛异常** —— 没有动态增长,没有双缓冲

```cpp
// VulkanFrameData.h:42
inline constexpr uint32_t k_MaxObjectsPerFrame = 1024;
// VulkanFrameData.cpp:58-60
if (m_objectCount >= k_MaxObjectsPerFrame)
    throw std::runtime_error("FrameData: 这一帧的物体数超过 k_MaxObjectsPerFrame(...)");
```

注释里的理由:*"超了直接抛 —— 静默溢出会画错东西,验证层不报。"*

**差异:我的更好** ✔

- 我修正后的方案(动态增长、**单缓冲**、帧开头检查+重建)不崩溃,是生产可用的形状
- 当年是"够用就行"的临时方案,超上限 = 程序死

---

## 1.4 · 环形缓冲什么时候可以被覆盖 / 替换?

**1) 结构:选 (a)** —— 帧槽各一个(已有的 2 个),双缓冲再加一层 → 共 4 块

**2) 覆盖旧数据 vs 替换整个缓冲,时机要求一样吗:**
门是同一道(帧开头那个 fence),但**错了会不会被发现完全不同**:

| | 覆盖旧数据 | 替换整个缓冲 |
|---|---|---|
| 碰什么 | `VkDeviceMemory`(一次 `memcpy`) | 描述符集 + 旧 `VkBuffer` 对象 |
| 早了会发生什么 | GPU 还在读 → 画面花、数据错 | 描述符 → VUID 报错;旧 buffer → `VUID-vkDestroyBuffer-buffer-00922` |
| 验证层抓得到吗 | **抓不到** | 抓得到 |

**3) 新的缓冲最早在哪一帧能用上:当前帧** ✅ 答对

```
帧 N-2  槽 0 的 buffer 被写满 → submit → 点亮槽 0 的 fence
帧 N-1  用槽 1,没碰槽 0
帧 N   开头 等槽 0 的 fence 点亮（:167）→ 帧 N-2 那批命令执行完
        → 槽 0 的 buffer 和描述符集【都空闲了】
        → 建更大的 buffer（设备操作,不和在飞的冲突）
        → vkUpdateDescriptorSets（槽 0 的描述符集此刻没人在用）
        → 然后才 resetObjects + 录制
```

**"更新必须在录制之前"这条答对了** —— `vkCmdBindDescriptorSets` 是把句柄录进命令缓冲的,
录完再改描述符,这一帧录进去的还是旧的。

**4) 在哪一行代码上做:**(跳过)

**当年实际:当年没有这条路径** ⚠️

- **覆盖旧数据**:位置就是我说对的那个点 —— `VulkanRendererAPI.cpp:235` 的 `frame.ResetObjects()`,
  注释原话:*"到这个点已经等过这个帧槽的 fence,GPU 不再读它那块 UBO 了。"*
- **替换整个缓冲**:**不存在。** 缓冲大小写死,从不替换。

**当年是用"固定大小"把这一题整个绕过去了**,代价是"超上限"变成没处理的失败。

**我自己的修正(当场推出来的):**
(a) 结构下**不需要双缓冲**,一个就够。因为帧 N 开头等的就是槽 0 的 fence,
那一刻槽 0 的 buffer 已经空闲,销毁重建不需要额外同步。
**双缓冲是给 (b) 准备的** —— 全局单缓冲时,帧 N 要换它就得更新槽 1 的描述符,
而槽 1 的命令还在飞 → 规范禁止。

---

## 目标 1 小结

| 决策点 | 我 | 当年 | 评价 |
|---|---|---|---|
| 1.1 通路 | B | B | ✅ 一致 |
| 1.2 viewProj/color | B | **C** | ⚠️ 两条不同的原则;我的每物体多付 64 字节(4060) |
| 1.3 上限 | 动态增长 | 1024 + 抛 | ✔ **我的更好** |
| 1.4 替换时机 | 当前帧 | 无此路径 | ✔ 我的方案必须解决它,我解决了 |

---

# 目标 2 · 一张场景里两种材质(纯色 / 贴图)

**✅ 已完成 2026-09-24 · 无需修改代码**

改前状态:管线只有一条,在 `VulkanRendererAPI::Init` 里建;shader 路径
(`shaders/slang.spv`)、入口名、顶点布局(`Vertex::getBindingDescription()`)、
全部固定功能状态,统统写死在 `VulkanPipeline.cpp`。

## 2.1 · 多条管线怎么管?

**我的选择:C** —— 用一组 key 在 map 里缓存,第一次用到时建

**我的理由**:用一组 key 来描述 pipeline 很直观,而且做复用方便判断

**子问题 a:11 条建管线的输入,哪些进 key、哪些进构造参数?**

我的回答:**1、2、9、10、11 进 key**

**子问题 b:为什么不能拿 `vk::Pipeline` 或它的指针当 key?**

我的回答:直接拿管线或者其指针当 key,就要在比较前把双方管线都创建出来,
这样就没有比较的意义了

**当年实际:C** ✅ 一致

**差异 —— 只在子问题 a:五条里只有 1、2 对。** 实际归属是**四类**,不是两类:

| # | 输入 | 实际放哪 |
|---|---|---|
| 1 | shader 模块 + 入口名 | **key** ✅ |
| 2 | 顶点输入描述 | **key**(只放 `vertexStride`)✅ 方向对,粒度更细 |
| 3 | input assembly | **key** ❌ |
| 4 | viewport / scissor | **动态状态**,整条不进管线 ❌ |
| 5 | rasterization | **key** ❌ |
| 6 | multisample | **key** ❌ |
| 7 | depth / stencil | **没有**(`pDepthStencilState = nullptr`) |
| 8 | color blend | **key** ❌ |
| 9 | 动态状态列表 | **buildPipeline 里的局部常量** ❌ |
| 10 | pipeline layout | **构造参数** ❌ |
| 11 | 颜色附件格式 | **构造参数** ❌ |

**漏掉的两件事:**

1. **判据有两层。** 第一层是"会不会变";第二层是**比较成本** —— 这个 key 每帧要查
   400 多次。标量 / 小 POD 留在 key 里无所谓,带 `std::vector` 的必须挪出去。
   (11 条里只有 #2 和 #9 带 vector。)
2. **粒度是字段,不是结构体。** `topology` 进 key 而 `primitiveRestartEnable` 写死 False;
   `rasterizationSamples` 进 key 而 `sampleShadingEnable` 写死。同一个结构体拆开用。

**子问题 b 的补充**:除了"要先创建才能比",还有一条更硬的 ——
**句柄只能比"是不是同一个",不能比"长得一样不一样"。** 两条用**完全相同** desc 建出来的
管线,句柄是两个不同的值,拿句柄当 key 就永远命不中。

## 2.2 · 顶点布局的真相源在哪?

**我的选择:B** —— 从 shader 反射

**我的理由**:更灵活;调用方只用传两个参数,更轻便

**子问题:反射给不出的 `binding` 号和 `stride`,分别由谁补?**

我的回答:stride 好像能从反射得到的数据中算出来,binding 号没头绪

**当年实际:B** ✅ 一致

**差异 —— 只在子问题:两条都要改。**

| | 反射给什么 | 实际谁决定 | 为什么 |
|---|---|---|---|
| **`stride`** | 每个属性的字节数。**加起来是下界,不是 stride** | **调用方给** | 它是**缓冲的属性**,只有建缓冲的人知道 |
| **`binding` 号** | **语义里根本不包含它** | **宿主(引擎)定**,这一版硬编码 0 | **shader 语言里没有"绑定号"这个概念** |

**关键反例**:`flat_color` 只声明 `float3`(12 字节),实际顶点数据是 20 字节
(pos3 + uv2)。按反射加起来会算成 12,读到的位置就是错的。

**两条答案不一样的原因**:"一个是**宿主的自由**(东西放在哪是宿主说了算),
一个是**缓冲的属性**(一笔多少字节只有建缓冲的人知道)。"

## 2.3 · 两个 shader 声明的绑定不一样,layout 取并集还是要求一致?

**我的选择:取并集**

**我的理由**:这样 pipeline layout 用的也是同一个,是否用不同的 pipeline
判断标准就不会落在 layout 上

**当年实际:并集** ✅ 一致,推理也对

**并集实际做的事:把「pipeline layout」从「逐条管线的输入」那一列挪走** ——
它变成 cache 的构造参数(#10),不进 key,`PipelineDesc` 里一个字都不放。

反面好处:如果要求一致,layout 的形状就取决于**先建哪个 shader**,那是假自由度
(换个创建顺序结果就变)。

**差异 —— 代价漏了三条:**

1. **并集只要变大,旧 layout 就作废。** 重建必须在**还没分配过描述符集**之前
   (`VulkanDescriptorAllocator.cpp:32-34` 的守卫,池存在就抛)。
   **这一条是整条延迟初始化链的起点** —— 池推迟到第一次分配才建、pipeline layout 推迟、
   `FrameData` 的描述符集从构造函数搬出来,源头都在这儿。
2. **同一个 `(set, binding)` 声明成不同类型 → 没有正确的并集**,必须抛(`:49-53`)。
3. **所有管线都被绑在一套更大的 layout 上。** 即使 shader 不用 set 1,它也在管线布局里;
   描述符池要为并集里所有类型出配额。尾部风险:可能撞上
   `maxPerStageDescriptorUniformBuffers` 之类的上限,**而没有任何单个 shader 用到那么多**。

## 目标 2 小结

| 决策点 | 我 | 当年 | 评价 |
|---|---|---|---|
| 2.1 管线怎么管 | C | C | ✅ 一致 |
| 2.1 子:11 条归类 | 1,2,9,10,11 进 key | 四类 | ⚠️ 只有 1、2 的方向对 |
| 2.1 子:为什么不能用句柄当 key | — | — | ✅ 对 |
| 2.2 真相源 | B | B | ✅ 一致 |
| 2.2 子:binding / stride 谁补 | "stride 能算,binding 没头绪" | stride 调用方给,binding 硬编码 0 | ⚠️ 两条都要改 |
| 2.3 layout 并集还是一致 | 并集 | 并集 | ✅ 一致;代价漏了三条 |

### 修改清单:无

**三个主决策点全部与当年一致。**

一个观察:**目标 1 出现了分歧(1.2 的 B vs C),目标 2 没有。** 差别在于
1.2 有两条真的不同的原则(按变化频率分层 vs 按 128 字节能不能塞下),而目标 2 的
三个主决策点是"在给定约束下最自然的那条"。真正出问题的是三个子问题,
它们问的是同一件事:**"这个细节该归哪一层"。**

---

# 目标 3 · 三种不同 stride 的顶点格式共存(12 / 20 / 28)

**✅ 已完成 2026-09-25 · 无需修改代码**

改前状态:顶点属性全部从 shader 反射来,`stride` 由调用方在建缓冲时给,管线按
`(shader 模块, stride, 渲染状态)` 缓存。三条 stride 分别来自
`Playground.cpp:29`(28)、`Playground.cpp:41`(20)、`playground2D.cpp:20`(12)。

## 3.1 · 顶点格式的真相源在哪一层?

**我的选择:A** —— 引擎侧不要顶点类型,顶点数据是 `const void* + size + stride`,
属性全从 shader 反射

**我的理由**:从 shader 拿数据更灵活

**我预期的代价**:只有"需要多引入一个反射(SPIRV-Reflect)"

**当年实际:A** ✅ 一致

```cpp
// Renderer.h:40
static Ref<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, uint32_t stride);

// VulkanVertexArray.h:42-45
Buffer       m_buffer;
BufferLayout m_layout;
uint32_t     m_stride = 0;
```

**差异 —— 代价少了两条:**

1. **`BufferLayout` 在这条路径上变成装饰性接口。** `GetLayout` / `SetLayout`
   留着、也真的存了(`VulkanVertexArray.h:36-37`),但**没有任何人读** ——
   属性和 offset 全从反射来。接口层留下的空壳,不是 bug。
2. **两侧布局一致没有任何校验手段。** 调用方写 `7 * sizeof(float)`(`Playground.cpp:29`),
   shader 侧写 `float3 + float4`(`flat_color.slang`),两个数字在两个文件里。

**对上这条选择、当年删掉的东西**:`struct Vertex` +
`getBindingDescription` / `getAttributeDescriptions`
(`PIPELINE_REFACTOR_TODO.md` 第六节死代码表)。**代价是编译期校验一起没了。**

---

## 3.2 · 一笔顶点的属性怎么摆到 GPU 侧?

**我的选择:A**(最终) —— 交错单流,所有属性挂 binding 0

**我最初的答案是 C**(多 binding 交错),转向 A 的过程:

| 步骤 | 谁 | 内容 |
|---|---|---|
| 1 | 我 | 选 C,理由是"以后引入多套 binding 时能处理",但**不确定**多 binding 下 stride 怎么算 |
| 2 | Claude | 规范事实:stride 是 **binding 的属性**,attribute 只带 `(binding, offset)`;`offset` 相对该 binding 的元素起点 |
| 3 | 我 | 追问:那是不是"把反射的属性并集存进一个 binding、stride 由类型算出来、再建缓冲"? |
| 4 | Claude | 两条阻塞:① 反射算的是"shader 要多少",不是"缓冲里有多少";② `CreateVertexBuffer` 建缓冲时就要 stride,而 shader 与缓冲的配对发生在 `Submit` |
| 5 | 我 | 改答案:**stride 在建缓冲时由调用方给,传给那个收集了全部属性的 binding 0** |
| 6 | Claude | 提醒这句描述的是 **3.2A**,不是 3.2C;两者不能并存 |
| 7 | 我 | 确认选 A |

**C 独有的一条硬代价** —— 多 binding 需要一张 **location → binding 归属表**,
而**反射给不出**(SPIR-V 顶点输入只有 `Location` 装饰,没有 binding 号),
必须由调用方声明 —— **也就是 3.1B**。

> **3.1A + 3.2C 不自洽。** 这是从 C 转向 A 的真正原因,不是"多 binding 不好"。

**当年实际:A** ✅ 最终一致

```cpp
// VulkanReflection.cpp:130-134   只产出 1 条 binding,binding 号硬编码 0
//                                 inputRate 硬编码 eVertex
// VulkanVertexArray.cpp:27-29    第二个顶点缓冲直接抛
```

**差异 —— 代价漏了四条**(第 4 条是我追问之后自己推出来的):

| # | 代价 | 什么时候撞上 |
|---|---|---|
| 1 | 实例化做不到 —— `inputRate` 是 binding 的属性,一个 binding 只能一个值 | 加每实例数据时 |
| 2 | 属性顺序钉死成 location 升序、从 0 起无空隙 | 数据带 padding / 要跳过前面字段 |
| 3 | 不能只更新流的一部分 | 位置静态、颜色每帧变 |
| 4 | **不能按用途裁剪数据** —— 交错缓冲"全有或全无",一个缓冲服务属性需求不同的多个 shader 时,stride 必须是并集,谁都得付 | 同一网格被多个 pass 用(阴影 / 深度预 pass) |

**"没什么缺点"成立的前提**:场景永远是一次 draw、一个缓冲、属性顺序固定、
速率只有 `eVertex`。当前代码全满足 —— **这四条现在都不付,全是未来的账**。

**一条收益**:`AddVertexBuffer` 第二个直接抛,把约束变成运行时报错,而不是静默画错。

### 顺带澄清的一件事:顶点输入**不能**做跨管线并集

讨论中出现过"由多条管线的 shader 得到属性并集,所有管线共用一个 binding"。有两种读法:

| 读法 | 成立吗 | 为什么 |
|---|---|---|
| 都用 **binding 号 0**,每条管线各自一份描述 | ✅ 现在就是这样 | 顶点输入状态是**逐管线**的,字段可以不一样 |
| 所有管线**共用同一份 attribute 描述**(真并集) | ❌ | 见下 |

真并集走不通,**和目标 3 直接冲突**:binding 的 `stride` 是这份描述的一部分,
共用一份就只能有一个 stride,12 / 20 / 28 共存立刻不成立。

第二层:并集会让每条管线声明它不读的属性,而**属性声明了就要取数** ——
缓冲里没有那个字段就是越界读取,有也是白取。而且 offset 是前缀和推的,
并集里多一个属性,后面所有属性的 offset 全部平移。

**并集在描述符 layout 那边是"不得不"(全局一份),在顶点输入这边是"不必,而且有害"。**
多个 shader 共用一个顶点缓冲靠的是**前缀链**(属性序列是数据内存序列的前缀),
不是并集 —— `m_SquareVA` 配 `flat_color`(前 12 字节)和 `texture`(前 20 字节)就是。

---

## 3.3 · 每个属性的 offset 谁定?

**我的选择:A** —— 按反射出的属性顺序做前缀和

**我的理由**:灵活,而且和 3.1A / 3.2A 配套

**我预期的代价**:多一步计算,"代价很小"

**当年实际:A** ✅ 一致

```cpp
// VulkanReflection.cpp:138-148
uint32_t offset = 0;
for (const auto& attribute : reflected.attributes) {
    result.attributes.push_back({ .location = ..., .offset = offset, ... });
    offset += attribute.size;
}
```

**差异 —— 代价漏了一条,但不是"多一步计算":**

真正的代价是**前缀和隐含一条约定**:数据在内存里的属性顺序 == shader 里 location
升序,且从 offset 0 起无空隙(`VulkanReflection.h:61-64` 的注释写的就是这条)。
这条约定**没有任何东西检查**,表达不了:

- 数据里带 padding
- 只想读数据结构里的第 2、3 个字段

**"多一步计算"这条估对了可以忽略** —— 一次循环,和反射同量级。

---

## 3.4 · 反射出的属性总字节数 ≠ stride 时怎么办?

**我的选择:A** —— 只查"大于"就抛,小于允许(尾部富余)

**我的理由**:小于是正常的

**我预期的代价**:没有

**当年实际:A** ✅ 一致

```cpp
// VulkanReflection.cpp:152-154
if (offset > stride)
    throw std::runtime_error("SPIRV-Reflect: 顶点属性共 N 字节,超过调用方给的 stride M");
```

注释里的理由:*"stride 是调用方给的,属性加起来的字节数可能比它小(尾部有余量,
正常),但不可能比它大 —— 那说明顶点数据装不下 shader 要的属性。"*

**差异 —— "没什么代价"这句对,但理由和想的不一样。**

要分清三类:

| 类 | 例子 | 3.4A 抓得到吗 |
|---|---|---|
| shader 要的比数据提供的**多** | 声明 pos3+color4(28)配 stride 20 的数据 | ✅ 抛 |
| shader 要的比数据提供的**少**(尾部富余) | 三角形数据 28 字节,`flat_color` 只声明 pos3(12) | ✅ 合法,本来就该放行 |
| **顺序 / 类型不一致但总和没超** | 数据是 pos3+color4(28),shader 声明 pos3+uv2(20 ≤ 28) | ❌ **静默读错** |

第三类是唯一的漏洞,验证层抓不到。但**它不是"选 A 没选 B"造成的** ——
用 B(必须相等)去堵它,会把第二类合法用例一起否掉:

| draw | 数据 stride | 反射属性总和 | B 的结果 |
|---|---|---|---|
| 400 个方块(`flat_color` + `m_SquareVA`) | 20 | 12 | **抛** |
| 三角形(`flat_color`) | 28 | 12 | **抛** |
| 2 个贴图方块(`texture`) | 20 | 20 | 通过 |

**404 个绘制项里 B 会杀掉 402 个。B 在这套场景下直接不可用。**

第三类的根源在 **3.1A**(引擎不知道数据里每个字段是什么),不在 3.4。
要堵它只能让调用方声明布局 —— 又回到 3.1B。

---

## 目标 3 小结

| 决策点 | 我 | 当年 | 评价 |
|---|---|---|---|
| 3.1 真相源 | A | A | ✅ 一致;代价漏了"`BufferLayout` 成空壳""布局一致无校验" |
| 3.2 属性怎么摆 | **A**(初选 C) | A | ✅ 最终一致;**C 与 3.1A 不自洽**,追问 stride 语义后自己改回 A |
| 3.3 offset 谁定 | A | A | ✅ 一致;真正的代价是那条隐性顺序约定,不是"多一步计算" |
| 3.4 总字节 vs stride | A | A | ✅ 一致;B 在此场景下会杀掉 402/404 个 draw,实际只有一个答案 |

### 修改清单:无

**四个决策点全部与当年一致**(3.2 从 C 转向 A 之后)。

一个观察:**这一轮的价值全在 3.2 的转向过程里。** 初选 C 不是错在
"多 binding 不好",而是错在**没先查清 stride 归谁** —— 查清之后,
"location → binding 归属表"这条硬要求自己就浮出来了,而它和已定的 3.1A 直接冲突。

和 目标 2 对比:目标 2 是主决策点全中、子问题全错;目标 3 是**主决策点全中,
但理由是后补的** —— 3.1 的代价、"一个 binding 装全部"的四条缺点、
"小于是正常的"为什么对,都是追问之后才补上的。

---

# 目标 4 · 贴图是每次绘制的输入,不再是全局状态

**✅ 已完成 2026-09-26 · 无需修改代码**

改前状态(HEAD `39a1f0d`):

| 坐标 | 现状 |
|---|---|
| `VulkanRendererAPI.h:45` | `texture m_MainTexture;` —— 一个成员,整个后端一张 |
| `VulkanRendererAPI.cpp:67-68` | `Init` 里建,路径写死 `".../textures/texture.jpg"` |
| `VulkanRendererAPI.cpp:70-71` | `Frames(...)` 把它的 view / sampler 传进每个帧槽 |
| `VulkanFrameData.cpp:21` | 帧槽**构造时**就 `allocate(...)`,描述符集写一次,活到程序结束 |
| `VulkanDescriptorAllocator.cpp:12-14` | 一个 set 两个 binding:0 = UBO(vertex),1 = 贴图(fragment) |
| `shaders.slang:36-38` | 片元无条件 `texture.Sample(...)` |
| `Renderer.h:19` | `Submit(shader, vertexArray, transform)` —— 签名里没有贴图 |
| `Playground.cpp:160-163` | `m_Texture->Bind(); Submit(...); m_LogoTexture->Bind(); Submit(...)` |
| `VulkanRendererAPI.cpp:246-248` | `DrawIndexed` 抛异常 —— 一条 draw 都没录 |

两条补充:`Playground.cpp` 里 `PushLayer` 两行是注释掉的,所以那对 `Bind(); Submit();` 当时不执行。
HEAD 不是"能跑、只是贴图是全局的",是**两边对不上** —— 调用侧写着 OpenGL 的 `Bind(slot)`,后端侧只有一张写死的贴图、而且根本画不出来。

---

## 4.1 · 贴图从哪条路进后端?

**我的选择:贴图当参数,经 FrameData 把描述符集传进 `DrawFrame`**

**我的理由**:(未给)

**我预期的代价**:(未给)

**当年实际:贴图是 `DrawItem` 的一个字段** ⚠️ 方向一致,传的东西不同

```cpp
// RendererAPI.h:22
Ref<Texture2D> texture;   // 可以为空:不采样的管线
// Renderer.h:39-40               两个 Submit 重载,一个收 color,一个收 texture
// VulkanRendererAPI.cpp:246-253  后端在录制时 dynamic_pointer_cast 降级
// VulkanRendererAPI.cpp:271-278  后端自己去 cache 查 set
```

**差异:** 都是"贴图跟着绘制项走,不是先绑后画"。差别在**传的是什么**:当年进后端的是
`Ref<Texture2D>` 这个抽象类型,`vk::` 一步都没离开 `Platform/Vulkan/`;我传的是
`vk::raii::DescriptorSet`,它要跨过 `RendererAPI.h`,而那个头刻意不含任何 `vk::` 类型
(`RendererAPI.h:11-13` —— Playground 的 vcxproj 没有 Vulkan 编译定义)。

---

## 4.2 · 贴图的描述符集和 UBO 放一个 set 还是分开?

**我的选择:分开**

**我的理由**:(未给)

**我预期的代价**:(未给)

**当年实际:分开** ✅ 一致

```cpp
// VulkanDescriptorAllocator.h:17-20
inline constexpr uint32_t k_ObjectUboSet = 0;
inline constexpr uint32_t k_TextureSet   = 1;
```

理由记在 `VulkanDescriptorAllocator.h:14-16`:同一个 set 号连绑两次,第二次会顶掉第一次。
UBO 每个物体都要换 dynamic offset 重绑,贴图只在换图时重绑,两者不能挤在一个 set 里。

**差异:** 无。

---

## 4.3 · 贴图的描述符集什么时候建、活多久?

**我的选择:C** —— 贴图创建时建自己的 set,每帧在 `DrawFrame` 里 update

**我的理由**:(未给)

**我预期的代价**:(未给)—— 这一行空着,下面两处就是这么来的

**当年实际:B** ⚠️ 我这版撞红线,不是"代价大"

```cpp
// VulkanTextureDescriptorCache.h:20-23
// 每张贴图一个描述符集(set 1),按 (view, sampler) 缓存。
// 这些集合活到程序结束,不参与任何重置 —— 贴图是长命资源,每帧重建它们没有意义。
```

**差异 —— 两处硬问题:**

1. **"贴图自己的 set" 和 "每帧 update" 不能并存。** set 里的 (view, sampler) 在贴图创建时就定死了,
   没有可更新的内容;真要每帧改内容,那个 set 就不是贴图的,是"这一帧的"。
2. **一份共享 set 加 2 个帧槽,必然踩 `VUID-vkUpdateDescriptorSets-None-03047`** —— 没带
   `UPDATE_AFTER_BIND` / `UPDATE_UNUSED_WHILE_PENDING` 位的绑定,不能被任何处于 pending 状态的
   命令缓冲使用。帧 N 在槽 0 上绑了它,帧 N+1 开头改写它时槽 0 的命令可能还在飞。

**顺带两条:**

- **顺序约束(我这版新引入的)**:建 set 要求 layout 已存在,而 layout 是第一个 shader 登记绑定时才建的。
  贴图比 shader 早建就抛(`VulkanDescriptorAllocator.cpp:143-147`)。
- **两版的代价对比:**

| | 我的 | 当年 |
|---|---|---|
| 谁分配 / 谁持有 | 贴图构造时 / 贴图对象 | 第一次绘制时 / `TextureDescriptorCache` |
| 写几次 | 每帧 | 一次 |
| 贴图销毁顺序 | 靠 `Application.cpp:45-47` 的 `Clear()` 早于 `Shutdown()` 成立,约束散在场外 | 收在 cache 一个类里 |
| `CreateTexture2D` 的参数 | 多一个 `DescriptorAllocator&` | 不变 |
| cache 返回引用的悬空风险 | 没有 | 有(map 扩容,`VulkanTextureDescriptorCache.h:29-30`) |

---

## 4.4 · 不采样的绘制项绑不绑 set 1?

**我的选择:不绑** —— 不采样的项跟采样的项走的不是同一条管线,各走各的

**当年实际:一样** ✅ 一致

```cpp
// VulkanRendererAPI.cpp:270-286
// 没有贴图的绘制项不绑 set 1 —— 它的管线不采样,那个绑定用不到。
```

**一条澄清,不改结论:** 两条管线**共用同一条 pipeline layout**(目标 2 的 2.3,layout 取并集),不是各有一条。
所以"不绑 set 1"能成立,靠的是 `flat_color` 的 SPIR-V 里没有这个绑定,**不是** layout 里没有它。

**一个观察(不改):** `:284-286` 的 `else { boundTexture = nullptr; }` 不必要。描述符集绑定在命令缓冲里
跨 `vkCmdBindPipeline` 保持(两条管线共用同一套 layout,天然兼容),纯色项绑不绑都不改它。
删掉那两行,序列 `[贴图 A, 纯色, 贴图 A]` 会少一次重绑。当前 Playground 是"纯色在前、贴图在后",一次都省不到。

---

## 目标 4 小结

| 决策点 | 我 | 当年 | 评价 |
|---|---|---|---|
| 4.1 通路 | 贴图当参数,传描述符集 | 贴图当参数,传 `Ref<Texture2D>` | ⚠️ 方向一致;传的东西越过了 `RendererAPI.h` 的 vk:: 边界 |
| 4.2 set 怎么分 | 分开 | 分开 | ✅ 一致 |
| 4.3 set 谁建、活多久 | 贴图建 + 每帧 update | cache 建 + 写一次 | ⚠️ 两处硬问题(自相矛盾 + 03047) |
| 4.4 不采样项 | 不绑 set 1 | 不绑 set 1 | ✅ 一致 |

### 修改清单:无

**四个决策点里两个一致,两个不一致 —— 而不一致的那两个当年是对的。** 和 目标 1 的 1.3 正好反过来。
4.3 那处不是"我的更好",是"我这版跑不起来":`UPDATE_AFTER_BIND` 没开,共享 set 一改写就撞 03047。

对比 目标 3:那一轮的主决策点理由是后补的;这一轮 4.1 和 4.3 的代价两行**都是空的**,
而两处都出了问题 —— 第三行空着确实是最危险的信号。

---

# 目标 5 · 应用层能创建 shader / 顶点缓冲 / 索引缓冲 / 顶点数组 / 贴图

**✅ 已完成 2026-09-26 · 有修改清单(见文末)**

改前状态(HEAD `39a1f0d`),五个入口签名全是 OpenGL 形状:

| 坐标 | 签名 | 什么形状 |
|---|---|---|
| `Shader.h:13` | `static Ref<Shader> Create(filepath)` | — |
| `Shader.h:14` | `static Ref<Shader> Create(name, vertexSrc, fragmentSrc)` | 收 **GLSL 源码字符串** |
| `VertexArray.h:21` | `static VertexArray* Create()` | **裸指针**,无参数 |
| `Buffer.h:96` | `static VertexBuffer* Create(float* vertices, uint32_t size)` | 非 const,**没有 stride** |
| `Buffer.h:111` | `static IndexBuffer* Create(uint32_t* indices, uint32_t count)` | 非 const |
| `Texture.h:20` | `static Ref<Texture2D> Create(const std::string& path)` | — |

五个里**只有一个有实现**,`Shader.cpp:7-16`,switch 里**没有 `case Vulkan`**;其余四个连 switch 都没有。
调用点集中在 `Playground.cpp:48,50,59,69,103,108-109,121,123,132` 和 `playground2D.cpp:9,20,22,30`。

两条挡路的:

1. **静态工厂拿不到 device。** `VertexArray* Create()` 一个参数都没有,而 Vulkan 建缓冲要 `VulkanContext*` 加一次性命令池。
2. **贴图根本没有应用层入口** —— 后端自己建,路径写死(`VulkanRendererAPI.cpp:67-68`)。

顺带:`Renderer::GetDeviceContext()`(`Renderer.h:56`)HEAD 里就存在而且**零调用者**(`REVIEW_DRILLS.md` 第 1 题)。
所以"拿不到 device"不是没手段,是那批工厂压根没想去拿。

---

## 5.1 · 创建入口放哪?

**我的选择:B** —— 收到 `Renderer` 上,一组 `Renderer::Create*` 静态函数

**我的理由**:(未给)

**我预期的代价:** `Renderer` 会变得有点大,但是合理的

**当年实际:B** ✅ 一致

**差异:** 无。"变大"的具体形状是两处 —— `Renderer.h` 加一行声明,`Renderer.cpp` 加一段(dynamic_cast + 判空 + 转发)。

---

## 5.2 · 后端怎么分派?

**我的选择:B** —— `Renderer` 里 `dynamic_cast` 到具体后端

**我的理由**:(未给)

**我预期的代价:** 文件稍微轻便一点,但是 `dynamic_cast` 有开销

**当年实际:B** ✅ 一致

**差异 —— 代价的位置和估的不一样:**

`dynamic_cast` 全落在**创建路径**:`Renderer.cpp` 里 5 处,初始化时一共约 10 次。
每帧的 cast 在别处 —— `VulkanRendererAPI.cpp:246-253`,404 items × 2~4 次 `dynamic_pointer_cast`。
两个量级差两个数量级,所以这条在这里基本不付。

真正相乘的是**另一个轴**:加一个后端,每个 `Create*` 里要加一条分支(现在 5 个)。
对照 A(往 `RendererAPI` 加虚函数):它相乘的轴是"资源类型 × 后端",加一种资源要每个后端各实现一遍。

**讨论时我把 A 的这条代价安到 B 头上了** —— `Renderer.cpp:82-84` 那段注释讲的是 A。

---

## 5.3 · 签名怎么定?

### a) `CreateShader` 收什么?

**我的选择:** 三个都可以(GLSL 源码 / SPIR-V 路径 / SPIR-V 字节),可以重载,但现在只收文件路径

**当年实际:只收路径** ✅ 方向一致

```cpp
// Renderer.h:39
static Ref<Shader> CreateShader(const std::string& name, const std::string& spvPath);
```

**差异 —— 收路径的代价漏了一条:收路径 = 把"工作目录"固化进接口。**

`Playground.cpp:17` 那些 `shaderDir + "flat_color.spv"` 是仓库根相对路径,而 `CLAUDE.md` 第三节整节在讲这条约束
(工作目录必须是 `D:\Fish`,否则读不到)。收 SPIR-V 字节这个依赖就消失,代价转到调用方(它自己读文件),
失败模式也从"打不开"变成"打开了但不是合法 SPIR-V"。

当年也没有重载 —— 只有这一条路径。

### b) 返回值用 `Ref<>` 还是裸指针?

**我的选择:`Ref<>`**

**当年实际:`Ref<>`** ✅ 一致

### c) `CreateVertexBuffer` 的完整签名?

**我的选择:** 现有参数 + stride

**当年实际:一致** ✅

```cpp
// Renderer.h:40
static Ref<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, uint32_t stride);
```

`const void*` 和 `size_t` 是两处和 HEAD 不同的细节(HEAD 是 `float*` + `uint32_t`)。
stride 归调用方这条在 目标 3 的 3.1 已经定过。

### d) `CreateVertexArray` 收不收 vb / ib?

**我的选择:抛弃 `VertexArray`** —— 把"绑 vb + ib"合并到录制命令缓冲时多收参数

**当年实际:保留** ⚠️ 分歧

```cpp
// Renderer.h:42-43
static Ref<VertexArray> CreateVertexArray(const Ref<VertexBuffer>& vertexBuffer,
    const Ref<IndexBuffer>& indexBuffer);
// VulkanVertexArray.h:69-70
// Fish::VertexArray 在 Vulkan 上的实现。它本身不持有 GPU 资源 ——
// 顶点缓冲和索引缓冲都是共享指针,这里只是把两者配成一对。
```

**差异:** 我的判断在事实上成立 —— **Vulkan 里没有"顶点数组"这个对象**,顶点格式逐管线、绑定逐命令,
`VulkanVertexArray` 不映射任何 GPU 资源。差的是"哪个方向想表达这条约束"。

代价(我这版要付的):

| # | 代价 | 什么时候撞 |
|---|---|---|
| 1 | `Submit` / `DrawItem` 的输入从 1 个 `Ref<VertexArray>` 变 2 个,`Renderer.h:25-28` 两个重载变四个 | 改签名 |
| 2 | 缓存 key 从 1 个指针变 (vb, ib) 两个 —— **只比 vb 会在"同 vb 不同 ib"时漏绑 index buffer** | 改录制循环 |
| 3 | 应用层要分开持有 vb / ib(现在 `ExampleLayer` 只有 `m_SquareVA`) | 改 Playground |

**收益:** `AddVertexBuffer` 第二个直接抛那道运行时报错没了,换成"签名上表达不了第二个"。
和 目标 3 的 3.2 那条收益同一个形状。

顺带:`Renderer.cpp:130-137` 的 `CreateVertexArray` 是五个 `Create*` 里**唯一不判后端**的(直接 `make_shared<VulkanVertexArray>`)。
抛掉这个入口,那处不一致跟着消失。

---

## 5.4 · 旧的那批静态工厂怎么办?

**我的选择:B** —— 删干净,"没必要存在了"

**当年实际:留着** ⚠️ 分歧

```cpp
// Renderer.h:32-35
// 现有那几个静态工厂(Shader::Create / VertexBuffer::Create / ...)是
// OpenGL 形状的 …… 那批工厂留着只服务于 OpenGL 遗留。
```

**差异:** 两条路最终都会删掉这批工厂,差的只是**时机** —— 当年等 `Platform/OpenGL/` 整批一起删。

删的实际范围比"5 个声明"大,而且会**编不过**:

| 删什么 | 连带 |
|---|---|
| `Shader.h:13-14` + `Shader.cpp:7-25` | `ShaderLibrary::Load`(`Shader.cpp:41,48`)直接调它 → `ShaderLibrary` 整个类要么一起删要么重写 |
| `VertexArray.h:21` + `VertexArray.cpp` | 该文件 20 行,只有这一个函数 |
| `Texture.h:20` + `Texture.cpp` | 该文件 20 行,只有这一个函数 |
| `Buffer.h:96,111` + `Buffer.cpp` | 该文件 32 行,除两个工厂还有 `IndexBuffer::GetCount()`(基类定义,不是工厂) |

`ShaderLibrary` 全仓库**零调用者**,所以"一起删"没有连带损失。

**不撞红线** —— 这些都在 `Fish/src/Renderer/` 下,不在 `Platform/OpenGL/` 里。
代价是:`OpenGLShader` / `OpenGLVertexArray` / `OpenGLBuffer` / `OpenGLTexture` 还有基类可继承、还能编过,
但**没有任何东西能建它们了**。

---

## 目标 5 小结

| 决策点 | 我 | 当年 | 评价 |
|---|---|---|---|
| 5.1 入口 | `Renderer::Create*` | `Renderer::Create*` | ✅ 一致 |
| 5.2 分派 | `dynamic_cast` | `dynamic_cast` | ✅ 一致;代价估错了位置(创建路径 vs 录制路径) |
| 5.3a shader 收什么 | 文件路径 | 文件路径 | ✅ 一致;漏了"工作目录被固化进接口" |
| 5.3b 返回值 | `Ref<>` | `Ref<>` | ✅ 一致 |
| 5.3c VertexBuffer 签名 | 现有 + stride | `(const void*, size_t, uint32_t)` | ✅ 一致 |
| 5.3d VertexArray | **抛弃** | 保留 | ⚠️ **分歧,我这版更干净** |
| 5.4 旧工厂 | 删干净 | 留着 | ⚠️ 分歧,差在时机 |

### 修改清单

| 决策点 | 改成什么 | 为什么 | 影响面 |
|---|---|---|---|
| 5.3d | 删掉 `VertexArray`,绘制项改成收 vb + ib | Vulkan 里顶点数组不映射任何 GPU 对象,它只是个配对容器 | `RendererAPI.h:21`、`Renderer.h:25-28`、`Renderer.cpp:130-137`、`VulkanRendererAPI.cpp:246-291`、`VulkanVertexArray.{h,cpp}`、`Playground.cpp` |
| 5.4 | 删五个静态工厂 + `ShaderLibrary` | 引擎层的入口只剩 `Renderer::Create*` | `Shader.{h,cpp}`、`Buffer.{h,cpp}`、`VertexArray.{h,cpp}`、`Texture.{h,cpp}` |

**7 个决策点里 5 个与当年一致。** 和 目标 1-4 对比:这一轮的"预期代价"都写了,而且都没写错方向 ——
5.2 那处错的是**位置**(创建路径 vs 录制路径),不是有无。

---

# 目标 6–7

*(待开始)*

---

# 汇总 · 对现有代码的修改清单

**7 个目标走完之后才填。** 每一条格式:

```
| 目标 | 决策点 | 改成什么 | 为什么 | 影响面 |
```

*(空)*

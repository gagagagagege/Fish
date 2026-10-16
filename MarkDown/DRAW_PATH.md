# 从创建资源到画到屏幕上（2026-10-16）

> **复习用**：跟着调用链走一遍 —— 从应用层建 shader / 贴图，到 `DrawFrame` 把它们用上。
> 描述的是 **2026-10-16 bindless 那轮之后**的状态。
>
> 分工：**这份只讲"走一遍"**。架构上的取舍（为什么这么分层、为什么延迟初始化）在
> `RENDERER_ARCHITECTURE.md`；过程记录（做了什么、撞了什么坑）在 `MarkDown/*_REFACTOR_TODO.md`。
>
> 行号都对过，改代码之后要重新对。

---

## 零、一句话

**应用层建 shader 的时候，描述符 layout 就定下来了；建贴图的时候，一个描述符都没产生。
两者的描述符全部推迟到第一次 `DrawFrame`。**

---

## 一、时间轴

| 阶段 | 时刻 | 发生什么 |
|---|---|---|
| **构造期** | `Playground()` 里，第一帧之前 | shader 建完 → **2 个 `VkDescriptorSetLayout`**；贴图建完 → **0 个描述符** |
| **第一帧** | `Application::run` 第一次循环 | `EnsureRenderState` 建 **2 个池、3 个 `DescriptorSet`、3 次 `vkUpdateDescriptorSets`** |
| **每帧** | 之后每一帧 | 什么都不再建。只有录制 |
| **GPU** | 提交之后 | 顶点着色器读 UBO，片元着色器按 UBO/顶点里的槽位从表里取图 |

---

## 二、阶段一 · 构造期

### 1. 建 shader —— **layout 在这里诞生**

```
Playground.cpp:31-33   Renderer::CreateShader("FlatColor"/"Texture"/"Batch", ".../*.spv")
```

`Renderer::CreateShader`（`Renderer.cpp:96-111`）
- `:98-100` 按 `(name, path)` 查 `s_ShaderCache`，命中直接返回。
  **两个 layer 各建一份，同名同路径只会真建一次** —— 否则会拿到两个 `vk::ShaderModule`，
  同一条管线建两遍，而且批靠 shader 对象本身判"同批"，两份永远判不相等
  （`BATCH_REFACTOR_TODO.md` 第五节坑 2）
- `:107-108` `std::make_shared<VulkanShader>(ctx, m_DescriptorAllocator, name, path)`

`VulkanShader` 构造函数（`VulkanShader.cpp:40-59`）

| 行 | 做什么 |
|---|---|
| `:44` | `readFile(path)` —— 读 `.spv` 的**字节** |
| `:50-52` | 字节 → `std::vector<uint32_t>`。反射要的是指令流 |
| **`:54`** | **`descriptorAllocator.AddShaderBindings(context, ReflectDescriptorBindings(words).bindings)`** |
| `:56` | `m_vertexInput = ReflectVertexInput(words, "vertMain")` —— 顶点属性，**不走描述符** |
| `:58` | `createShaderModule` |

**`:54` 这一行是整个链条上唯一的输入。**

`AddShaderBindings`（`VulkanDescriptorAllocator.cpp:21-73`）
- `:24` 记下 `m_context` —— 后面 `textureTableSize()` 要用它查设备上限
- `:26-29` `m_layoutFrozen` 检查。**"所有 shader 必须在第一次绘制之前建完"这条就是它**
- `:31-63` 逐条和已有的 `m_bindings` 比。key 是 `(set, binding)`，**不看是哪个 shader**
  - 三种不一致会抛：type 不同、数组形式不同（定长 vs 运行时）、定长长度不同
  - `stages` 取并集
- `:65-66` 并集没变大 → 直接返回，不重建 layout
- `:72` `BuildLayout()`

三份 shader 建完，`m_bindings` 是：

| set | binding | type | 谁声明的 |
|---|---|---|---|
| 0 | 0 | `eUniformBuffer` | 三份都有 |
| 1 | 0 | `eCombinedImageSampler`，**runtimeArray** | `texture.slang` + `batch.slang` |

`BuildLayout`（`VulkanDescriptorAllocator.cpp:75-125`）—— **只建 layout，不建池，不分配 set**

```
:77     setCount = m_bindings.back().set + 1        → 2 套
:83     updateAfterBind = IsUpdateAfterBindSet(set) → set==1
:93     descriptorCount = DescriptorCount(binding)
          set 0 → binding.count                    = 1
          set 1 → runtimeArray → textureTableSize()
                    :146-156  getProperties2<...>() 查设备
                    min(1048576, k_MaxBindlessTextures=4096) = 4096
:100-105  set 1 的 binding 加 eUpdateAfterBind | ePartiallyBound
:108-110  DescriptorSetLayoutBindingFlagsCreateInfo 挂 pNext
:116-118  set 1 的 layout 加 eUpdateAfterBindPool
:121      vkCreateDescriptorSetLayout
```

> ⚠️ **`descriptorCount` 不能读 `binding.count`。** 运行时数组反射出来是 0
> （`spirv_reflect.c:1920` 填哨兵 → `:2263` 相乘）。见 `BINDLESS_REFACTOR_TODO.md` 第四节第 1 条。

**到此为止：0 个池、0 个描述符集。**

### 2. 建贴图 —— **描述符层面什么都没发生**

```
Playground.cpp:57-59   Renderer::CreateTexture2D(path) ×3
playground2D.cpp:22    Renderer::CreateTexture2D(path) ×1
```

`Renderer::CreateTexture2D`（`Renderer.cpp:135-150`）→ `VulkanTexture2D` → `texture::loadFromFile`
（`VulkanTexture.cpp:18-53`）

```
stbi 解码(翻转 y) → staging buffer 拷进去 → Image(deviceLocal, eTransferDst|eSampled)
:45-50  一次性命令缓冲:Undefined → eTransferDstOptimal → copy → eShaderReadOnlyOptimal
:52     建 sampler(linear/linear, repeat, 各向异性)
```

**只有三样 GPU 资源：image、imageView、sampler。没有描述符。**

`:146` `texture->SetBindlessIndex(vulkan->RegisterTexture(texture->getTexture()))`

### 3. `RegisterTexture` —— 发号，攒着

`VulkanRendererAPI.cpp:109-136`

```
:112     capacity = textureTableSize()          → 4096   ← 第一次摸到设备上限
:113-117 越界检查(发满了就抛)
:119     slot = m_NextTextureSlot++             → 0, 1, 2(playground2D 那张是 3)
:120-123 DescriptorImageInfo{sampler, imageView, eShaderReadOnlyOptimal}
:125-126 m_TextureInfos[slot] = info
:131-133 if (m_TextureTableSet != nullptr) ...  ← 表还没建,跳过
```

四张贴图建完：`m_TextureInfos` 有 4 条，`m_TextureTableSet` 还是 `nullptr`。

---

## 三、阶段二 · 第一帧

`Application::run`（`Application.cpp:78-91`）：
**两个 layer 的 `OnUpdate` 先跑完**（`:87-89`，只入队、不碰命令缓冲），**然后** `Renderer::DrawFrame()`（`:90`）。

`Renderer::DrawFrame`（`Renderer.cpp:78-82`）→ `s_RendererAPI->DrawFrame(s_DrawQueue)` → 清空队列。

`VulkanRendererAPI::DrawFrame`（`:343`）**第一件事就是 `EnsureRenderState()`**（`:345`）。

### `EnsureRenderState`（`:195-218`）—— 三步，顺序不是随便排的

#### 3a. `:200` set 0 —— UBO

`Frames::EnsureDescriptorSets`（`VulkanFrameData.cpp:94-99`）→ 每个帧槽 `EnsureDescriptorSet`（`:50-60`）

```
:55   allocateSet(k_ObjectUboSet = 0)
        DescriptorAllocator.cpp:215-218
          PoolIndex(0) = 0,池不存在 → BuildPool(0, k_DescriptorSetBudget = 128)
            挑 set==0 的 binding → descriptorCount = 1 × 128
            flags = eFreeDescriptorSet                    ← 不加 UAB
        :221  m_layoutFrozen = true     ← 从这一刻起不能再加 shader
        :223  vkAllocateDescriptorSets
:57-59 writeUniformBuffer(set, 0, {环形缓冲, 0, sizeof(ObjectUbo)})
        ← 第一次 vkUpdateDescriptorSets
```

两个帧槽各一套，所以这一步建 **2 个 set + 2 次写**。

#### 3b. `:201` set 1 —— 纹理表

`EnsureTextureTable`（`:180-190`）

```
:185  allocateSet(k_TextureSet = 1)
        PoolIndex(1) = 1,池不存在 → BuildPool(1, k_TextureTableSetCount = 1)
          descriptorCount = 4096 × 1
          flags = eFreeDescriptorSet | eUpdateAfterBind
        vkAllocateDescriptorSets
          layout 带 eUpdateAfterBindPool ↔ 池带 eUpdateAfterBind   ← 03044 要求配对
:188  writeCombinedImageSamplers(set, 0, 0, m_TextureInfos)   ← 4 格一次写完
```

**`:188` 是整条路的转折点。** 从那之后 `RegisterTexture` 的 `:131-133` 才会生效
（但当前没有调用点 —— 全部 4 张贴图都在构造期创建）。

**4096 格只写了 4 格，剩 4092 个空洞** —— 这是 `ePartiallyBound` 在兜的东西。

#### 3c. `:211-217` pipeline layout

`VulkanPipelineCache` 构造函数（`VulkanPipelineCache.cpp:78-94`）

```
:212    layoutHandles()  → 把两条裸句柄交出去,同时置 m_layoutFrozen
:88     setLayoutCount = m_setLayouts.size()      → 2
:90     pushConstantRangeCount = 1
           offset 0, size sizeof(PushConstants) = 64, vertex|fragment
        vkCreatePipelineLayout
```

**pipeline layout 不持有池、不持有 set**，它只引用那两条 layout 句柄。

#### 之后每帧

`EnsureTextureTable` / `EnsureDescriptorSets` 都早退（已经有了）；只有 swapchain 格式变了
才 `Invalidate`（`:205-208`）。

---

## 四、阶段三 · 录制（`DrawFrame` 后半段）

### 绑定：两次，各自一条线

| 位置 | 绑什么 | 频率 |
|---|---|---|
| `:414-417` | **set 1 = 纹理表** | **一帧一次**，两条路共用 |
| `:466-467` | set 0 = UBO + `dynamicOffset` | **每个绘制项一次** |

`:414` 那次一帧只发一条命令 —— **纹理选择已经是 UBO 里的一个数**，没有"换贴图重绑 set"这回事。

`:466` 那次还在逐项发，因为 UBO 是环形缓冲，每个物体要换 `dynamicOffset`。

### texId 怎么进去

```
:464  texId = item.texture ? item.texture->GetBindlessIndex() : 0u
        ← 这个数就是构造期 RegisterTexture 在 :119 发的号
        ← Playground.cpp:62-64 把它存进 m_BatchTexIds
:465  dynamicOffset = frame.PushObjectUbo(transform, color, texId)
        VulkanFrameData.cpp:79-80  memcpy 进环形缓冲
```

### 批那条路不上 UBO

`RecordBatches`：texId 在**顶点属性**里（`BatchVertex::texId`），顶点在 CPU 侧已经烘成
世界坐标（`AppendShapeVertices`）。所以批既不绑 set 0，也不重绑 set 1。

---

## 五、阶段四 · GPU 侧

| 谁 | 读什么 |
|---|---|
| `texture.slang` vertMain | push constant `viewProj` + **set 0 的 `ubo.model`**（`dynamicOffset` 定位） |
| `texture.slang` fragMain | **set 0 的 `ubo.texId`** → `albedo[NonUniformResourceIndex(texId)]` → 采样 → `* ubo.color` |
| `batch.slang` vertMain | push constant `viewProj` + 顶点里的世界坐标（**没有 UBO**） |
| `batch.slang` fragMain | **顶点插值来的 `fragTexId`**（`nointerpolation`） → **同一张表** |

**两条路的唯一区别是"texId 从哪来"**：per-item 从 UBO（每物体一份），批从顶点属性（每顶点一份）。
表是同一张，binding 也是同一格（set 1 / binding 0）。

描述符的 stage 位是**按入口点取并集**的（`VulkanReflection.cpp:93` 用 `entryPoint.shader_stage`）：

- set 0：`texture.slang` 的 vertMain 读 `model`、fragMain 读 `color`/`texId` → **vertex + fragment**
- set 1：只有 fragMain 读 → **fragment**

---

## 六、谁在什么时候建了什么

| 时刻 | 建了什么 |
|---|---|
| `CreateShader` ×3 | **`VkDescriptorSetLayout` ×2** |
| `CreateTexture2D` ×4 | image ×4 + imageView ×4 + sampler ×4。**描述符：0** |
| 第一帧 `EnsureRenderState` | pool ×2、`DescriptorSet` ×3、`vkUpdateDescriptorSets` ×3、`vkCreatePipelineLayout` ×1 |
| 之后 | **什么都不再建** |

---

## 七、顺序被什么钉住

1. **layout 要等 shader** —— 它是反射出来的。所以 `EnsureRenderState` 开头那条
   `hasLayout()` 早退（`:197-198`）不是优化，是**必须的**
2. **池和 set 要等 layout** —— `VkDescriptorSetLayout` 是建池和分配 set 的入参
3. **`m_layoutFrozen` 要等到真有人引用 layout** —— 三条置位路径：
   `AddShaderBindings:26-29`（检查）、`layoutHandles():129`、`allocateSet:221`。
   **句柄一交出去，重建就是悬空**

> 另外一条不是约束但值得记住：**`CreateShader` 是唯一一个"应用层调用会改后端 layout 结构"
> 的地方。** `CreateTexture2D` 改不了（表至少 4096 格），`CreateVertexBuffer` 完全不碰描述符。

---

## 八、容易搞错的地方

| # | 事 | 为什么容易错 |
|---|---|---|
| 1 | **UBO 和贴图分在两个 set** | `vkCmdBindDescriptorSets` 对同一个 set 号连绑两次，第二次会顶掉第一次 |
| 2 | **texId 有两个来源** | per-item 走 UBO、批走顶点属性。串了只在一条路上看得出来 |
| 3 | **`ObjectUbo` 三份必须一致** | `AddShaderBindings` **只比 set/binding/type，不比 block 大小** —— 不一致不报错 |
| 4 | **`NonUniformResourceIndex` 不能漏** | 漏了不报错，驱动按整个 subgroup 算一次描述符，采到隔壁的纹理 |
| 5 | **`nointerpolation` 不能漏** | 漏了每个像素拿到三个顶点 texId 的插值，一个无意义的数 |
| 6 | **`descriptorCount` 不能读反射的 `count`** | 运行时数组反射出来是 0。见第四节 |
| 7 | **`[[vk::binding(X, Y)]]` 的顺序是 `(binding, set)`** | 不是 `(set, binding)`。实测：`(0, 1)` → `Binding 0` + `DescriptorSet 1` |
| 8 | **`StructureChain` 的初始化列表必须按声明顺序** | 乱序 MSVC 报 C5337（非标准扩展） |

---

## 九、怎么自己查

**看 shader 编成了什么**（Vulkan SDK 里就有，不用改代码重编）：

```bash
D:\Vulkan_SDK\Bin\spirv-reflect.exe <name>.spv   # set/binding/count/array + UBO 块逐成员 offset
D:\Vulkan_SDK\Bin\spirv-dis.exe     <name>.spv   # 装饰:NonUniform / Flat / RowMajor / OpTypeRuntimeArray
```

**临时加打印**（拿到确切的异常文案）：这个引擎未捕获异常只弹一句 `abort() has been called`，
没有 message。要拿文案得临时在 `EntryPoint.h` 的 `main` 外面套 try/catch 打印 `what()`。

**验证一轮的标准流程**（`CLAUDE.md` 第三节）：构建 0 错 0 警告 → 跑十几秒看 stderr →
证明在干活（CPU 时间 / 帧率）→ 边界事件单独跑（`WM_CLOSE` 退出、resize **要断言前置条件**）。

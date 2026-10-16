# Bindless 描述符 —— 完成记录

> 2026-10-16。`PLAN.md` 第六节的 **W14**。起点是 `BATCH_REFACTOR_TODO.md` 那轮建的纹理库。
>
> **状态:跑通。** 构建 0 错误 0 警告,验证层 0 输出(20 秒 + resize + WM_CLOSE 全程)。
> **画面 2026-10-16 真人开窗口确认过。**
>
> 遗留项见第六节 —— **下次接着做先看那一节。**

---

## 一、做了什么

改前:`batch.slang` 里 `Sampler2D albedo[64]`(set 2),而且**开画前必须把贴图全部登记完**
(`VulkanRendererAPI.cpp:122-125` 那条 throw)。per-item 是另一条路 —— set 1 一贴图一个 set
(`VulkanTextureDescriptorCache`)。

改后:**一张纹理表,两条路共用。**

| | 改前 | 改后 |
|---|---|---|
| 纹理 set | set 1(per-item)+ set 2(批),两套 | **set 1,一张表** |
| 数组长度 | 编译期 `[64]`,和 C++ 侧手工对齐 | 运行时数组 `[]`,格数由 layout 定 |
| 槽位 | 应用层指定 | **后端发号** |
| 加贴图的时机 | 第一次画批之前 | 任何时候 |
| `VulkanTextureDescriptorCache` | 一贴图一个 `DescriptorSet` | **删了** |

实测(400 精灵):

| | 改前 | 改后 |
|---|---|---|
| ms/帧 | 1.25 | 1.15–1.20 |
| 表格数 | 64 | `min(设备上限, 4096)` = 4096 |

`bindDescriptorSets` 次数:set 1 从"贴图变了就绑"变成**固定 1 次/帧**;set 0 仍然每项一次。

---

## 二、形状(2026-10-16 拍板的四条)

| # | 决定 | 备选是什么 |
|---|---|---|
| 1 | per-item 的 set 1 和批的 set 2 **合并成一张表** | 只动 set 2,set 1 原样保留 |
| 2 | shader 写**运行时数组** `albedo[]` | 定长,把 64 调大 |
| 3 | 槽位**后端发号** | 沿用应用层指定 |
| 4 | 表**单开第二个描述符池** | 一个池、所有 layout 都加 `eUpdateAfterBindPool` |

其余已定的:

| # | 决定 |
|---|---|
| 5 | 表放 set 1(不是 set 2) —— 合并后只剩一个纹理 set,沿用 `k_TextureSet` |
| 6 | 格数 = `min(设备 maxDescriptorSetUpdateAfterBindSampledImages, k_MaxBindlessTextures = 4096)` |
| 7 | 表那条 binding 加 `eUpdateAfterBind \| ePartiallyBound` |
| 8 | per-item 的 texId 走 set 0 的 `ObjectUbo`,不走顶点属性 |
| 9 | `Texture` 基类加非纯虚 `GetBindlessIndex()`(默认 0),`VulkanTexture2D` override |
| 10 | 删 `k_MaxTextures` / `k_BatchTextureSet` / `k_BatchTextureBinding` / `k_NoBatchTextureSlot` |

---

## 三、机制

### 纹理表(`VulkanRendererAPI` 的成员)

```cpp
vk::raii::DescriptorSet              m_TextureTableSet = nullptr;
std::vector<vk::DescriptorImageInfo> m_TextureInfos;   // 下标就是槽位
uint32_t                             m_NextTextureSlot = 0;
```

- set 1、binding 0,`Sampler2D albedo[]`(combined image sampler)
- 槽位由 `RegisterTexture` 发,`m_NextTextureSlot` **只增不减**
- 表懒建,第一次 `DrawFrame` 的 `EnsureRenderState` 里:
  - 建之前 —— 只往 `m_TextureInfos` 里攒
  - 建之后 —— **每加一张立刻写那一格**(`writeCombinedImageSamplers(..., slot, {info})`)

### 两条路的 texId 来源不同

| 路 | texId 从哪来 | 谁填 |
|---|---|---|
| per-item(`texture.slang`) | `ObjectUbo.texId`(每物体一份,带 dynamic offset) | `DrawFrame` 读 `item.texture->GetBindlessIndex()` |
| 批(`batch.slang`) | 顶点属性 `BatchVertex::texId` | `AppendShapeVertices` |

**表是同一张,binding 也是同一格。** 代价见第四节第 5 条。

### 描述符池:两个

| 池 | 装谁 | maxSets | flags |
|---|---|---|---|
| 0 | set 0(UBO) | `k_DescriptorSetBudget = 128` | `eFreeDescriptorSet` |
| 1 | set 1(纹理表) | `k_TextureTableSetCount = 1` | `eFreeDescriptorSet \| eUpdateAfterBind` |

---

## 四、关键设计决定(不要重新推导)

### 1. 运行时数组反射出来的 `count` 是 0

`Fish/vendor/SPIRV-Reflect/spirv_reflect.c`:

```
:2198   p_descriptor->count = 1;
:1920   dims[dim_index] = SPV_REFLECT_ARRAY_DIM_RUNTIME    (spirv_reflect.h:368 = 0)
:2263   p_descriptor->count *= dim_value;                  → 1 × 0 = 0
```

实测(`spirv-reflect.exe texture.spv`):`count : 0` / `array : [0]`。

**后果一**:`descriptorCount` 绝不能读 `binding.count`。`BuildLayout` 走 `DescriptorCount()`,
对 `runtimeArray` 的 binding 用 `textureTableSize()`。

**后果二**:`count == 0` **不能当哨兵** —— `AddShaderBindings` 那条"数组长度必须一致"的检查
会变成 `0 == 0` **静默通过**(比抛异常更糟)。所以 `Binding` 加了独立的 `bool runtimeArray`,
判据是 `array.dims` 的最后一维,不是 `count`。

顺带:**「这是运行时数组」反射是告诉你的** —— 在 `array.dims[]` 里。定长数组 `dims[0]` 是真长度,
运行时数组是哨兵。大小则**反射拿不到,也不是"拿不到"而是 SPIR-V 里就没有**
(`OpTypeRuntimeArray` 不带长度操作数,定长的 `OpTypeArray` 才带)。

### 2. 「两个池」不是规范强制的,但这样池才装得下

`VUID-VkDescriptorSetAllocateInfo-pSetLayouts-03044` **只约束一个方向**:带 `eUpdateAfterBindPool`
的 layout 必须从带 `eUpdateAfterBind` 的池里分。反向没有 VUID —— 一个 UAB 池同时装 set 0 和表
**是合法的**。

选两个池的实际理由是池大小公式:`BuildPool` 按 `count × maxSets` 预留,而
`k_DescriptorSetBudget = 128`。表单独一个池就能把 `maxSets` 压到 1,池里只留 4096 格;
合池会被按 128 倍预留。

配套:`VUID-VkDescriptorSetLayoutCreateInfo-flags-03000` —— binding 上带了 `eUpdateAfterBind`,
layout 的 flags 就必须带 `eUpdateAfterBindPool`。

### 3. `descriptorCount` 必须有具体整数

`VkDescriptorSetLayoutBinding::descriptorCount` 没有"运行时"这个取值。规范对运行时数组的表述是
"shader 侧不封顶,layout 侧由 `descriptorCount` 封顶"。

**不需要 `eVariableDescriptorCount`** —— 那是"给单个 set 分配少于 layout 声明的格数"用的,我们一次分满。

### 4. 设备特性要五条,加在 `PhysicalDeviceDescriptorIndexingFeatures` 上

`VulkanContext.cpp` 的 `isDeviceSuitable` 检查和 `StructureChain` **必须逐字段一一对应** ——
查的和开的对不上,`getFeatures2` 那片内存是别的东西填的。

```
shaderSampledImageArrayNonUniformIndexing   ← 上一轮就有
runtimeDescriptorArray
descriptorBindingSampledImageUpdateAfterBind
descriptorBindingPartiallyBound
descriptorBindingUpdateUnusedWhilePending
```

**`StructureChain` 的 designated initializer 必须按结构体声明顺序写**,乱序 MSVC 报 C5337。

`descriptorBindingUpdateUnusedWhilePending` 单独看**不是** updateAfterBind 的硬要求(它是弱化替代),
但绑成一组是通行做法。

### 5. texId 走 UBO 不走顶点属性

per-item 的顶点格式是 pos3+uv2(stride 20),塞不下第五个属性;而 UBO 本来就是每物体一份带 dynamic offset。

**代价:texId 有两个来源**(见第三节那张表)。串了的话只在一条路上看得出来 —— 这是这轮
验证时最该盯的东西,也是画面确认的价值所在。

### 6. `Texture` 基类多了一个虚函数

`GetBindlessIndex()` —— **非纯虚、默认返回 0**。纯虚会把 `Platform/OpenGL/` 下那批遗留类打断,
而那个目录在红线表里("删的时机要挑")。

### 7. `flat_color.slang` 的 `ObjectUbo` 必须跟着改

两个 shader 占同一个 set 0 binding 0,`ObjectUbo` 必须逐字段一致。而 `AddShaderBindings`
**只比 set/binding/type,不比 block 大小** —— 不一致不报错。

---

## 五、踩到的坑

### 1. 验证层 0 输出 ≠ 程序在跑

测越界那条时把 `k_MaxBindlessTextures` 调到 2。跑出来:进程活着、窗口标题正常、
`Responding=True`、**但没有一帧输出**。差点当成"跑通了"。

查窗口列表才发现是个 `#32770` 对话框,标题 "Microsoft Visual C++ Runtime Library",
内容 `abort() has been called` —— **未捕获异常弹了模态框**,进程就一直挂在那儿。

**这个引擎现在任何未捕获异常都只给这一句,没有 message。** 拿到确切文案要临时在
`EntryPoint.h` 的 `main` 外面套 try/catch 打印 `what()`。(要不要让它常驻是个待定项:
常驻的代价是异常从 abort 变成 exit 1。)

**教训**:断言要看**过程有没有在推进**(帧输出),不能只看"进程没退、窗口还在"。

### 2. `spirv-reflect.exe` 在 Vulkan SDK 里有

```
D:\Vulkan_SDK\Bin\spirv-reflect.exe <shader>.spv   # descriptor binding 的 set/binding/count/array + UBO 块逐成员 offset
D:\Vulkan_SDK\Bin\spirv-dis.exe <shader>.spv       # 装饰:NonUniform / Flat / RowMajor / OpTypeRuntimeArray
```

比"改代码 + 编译 + 跑"快得多。这轮 `count = 0` 和 `texId` 的 offset 都是这么确认的。

### 3. `flat_color.spv` 里 `texId` 被标 `UNUSED`,但结构没被删

```
uint texId; // abs offset = 80, ..., UNUSED
```

**结构成员不会被删,没用到的描述符声明会被删** —— 后者见 `flat_color.spv` 反射出来只有 1 条 binding。
所以"Slang 会把用不到的东西优化掉"这句话要分这两种情况说。

### 4. `[[vk::binding(X, Y)]]` 的顺序是 `(binding, set)`

不是 `(set, binding)`。实测:`[[vk::binding(0, 1)]]` 反汇编出来是 `Binding 0` + `DescriptorSet 1`。

### 5. 改完 shader 必须重跑 `compile.bat`

`BATCH_REFACTOR_TODO.md` 第五节第 5 条有前科。这轮改了三份 `.slang`。

---

## 六、遗留项

> **下次接着做,先看这一节。**

### 没验证的

| 项 | 说明 |
|---|---|
| **`ePartiallyBound` 到底挡没挡 08114** | **归因没验。** 删 filler 和加 flag 是同一次改动,**没做 A/B**(把 flag 去掉再跑一遍)。"0 报错"这个结果不能单独归给它 |
| **帧中写描述符要不要 barrier** | 探针跑过那个场景、验证层没报,但**同步校验没开**(`--sync-validation`)。要真验得单开一次 |
| **4000 精灵的帧时** | 要按键,没测。400 精灵 1.15 ms/帧对上了上一轮的 1.25 |
| **真人拖窗口** | 用的是程序化 `MoveWindow`,断言过客户区确实变了(1600×900 → 984×661),不是手拖 |
| **非 NVIDIA 设备** | 四条新特性是硬门槛。780M / 软渲染那几个没试过能不能选出来 |

### 缺口 A:真缺功能

| 缺口 | 坐标 | 说明 |
|---|---|---|
| **槽位不回收** | `VulkanRendererAPI.cpp:119`;`VulkanTexture2D` 析构不还槽位 | 发出去就回不来。运行时反复加载/卸载贴图,4096 格填满之后只剩抛异常 |
| **sampler 和 image 没分开** | `texture.slang:37` / `batch.slang:37` | 现在是 combined image sampler,采样参数烘在每一格里。分开成两个数组(`SamplerState[]` + `Texture2D[]`)之后,换采样参数不用占新槽位 |
| **只有"采样图"这一类是 bindless** | 表里只有 `eCombinedImageSampler` | storage buffer / storage image / uniform texel buffer 都还没有 |

**第一条和 updateAfterBind 是配对的。** `VulkanRendererAPI.cpp:131-133` 那条增量写分支
**当前没有调用点** —— 全部 4 张贴图都在 layer 构造期创建,那时表还没建。就算有了调用点,
没有回收也撑不了几轮。**这两件事实际上是一件事的两半。**

### 缺口 B:代价类(能跑,只是不划算)

| 项 | 坐标 | 现在 |
|---|---|---|
| 表池按上界预留 | `VulkanDescriptorAllocator.cpp:174-204` | 池里固定 4096 格,实际用了 4 格。要按实际用量得引 `eVariableDescriptorCount` |
| 逐物体数据还是一次绑定 | `VulkanRendererAPI.cpp:466-467` | 每个绘制项一次 `bindDescriptorSets`(换 dynamic offset)。改成 SSBO 数组 + push constant 索引能消掉 |
| 池 `maxSets` 写死 | `VulkanDescriptorAllocator.cpp:217` `k_TextureTableSetCount = 1` | 表只有一份,这个 1 是硬编码的 |

### 缺口 C:另一量级(不是"完善",是换掉)

| 项 | 说明 |
|---|---|
| `VK_EXT_descriptor_buffer` | 描述符本身放进缓冲、GPU 可写,`vkUpdateDescriptorSets` 整个不要 |
| set 0 也 bindless | 见上面"逐物体数据"那条 |

### 还想做的

- **让 `updateAfterBind` 那条路真被踩到。** 需要一个"第一帧之后创建贴图"的场景
  (流式加载 / 编辑器里换图)。`PLAN.md` 的路线图上现在没有一件事会触发它
- `Playground` 里那个探针(第 200/400/600 帧各加一张贴图)**验完删了**,现在没有回归面

### 一个悬着的问题

三个 shader 的矩阵都带 `OpMemberDecorate ... RowMajor` + `MatrixStride 16`,而 C++ 侧传的是
**GLM 的列主序 `mat4`**。按我的理解这俩应该差一个转置,但**位置一直是渲染对的**
(W5–W12 逐项确认过,这轮画面也确认过)。**"为什么会对"我没推导过** ——
是被保证的还是碰巧的,不知道。

---

## 七、验证是怎么做的

1. 构建 0 错误 0 警告
2. 跑 20 秒 → **stderr 0 字节**
3. 400 精灵 1.15–1.20 ms/帧
4. resize:客户区 1600×900 → 984×661,**前置条件断言过**
5. `WM_CLOSE` → `exitCode=0`,stderr 0 字节,窗口标题断言过
6. **临时探针**:第一帧之后继续加贴图(第 200/400/600/800 帧),槽位 4/5/6/7,
   **stderr 0 字节** —— 这是"绑后还能改"的直接证据
7. 越界:`k_MaxBindlessTextures` 调到 2 → 抛 `纹理表满了:已经发了 2 个槽位,上限是 2`,exit=1;
   调到 4 → 正常跑(正好 4 张贴图)
8. **画面 2026-10-16 真人开窗口确认过**

---

## 八、一句话

**shader 定形状 → 反射把形状读出来(含"这是运行时数组")→ 设备答应那套 flag →
`DescriptorAllocator` 把形状变成 layout 和两个池 → `VulkanRendererAPI` 拿这个 layout
建一张能边走边填的表 → per-item 那条路并进来 → 应用层只剩 `CreateTexture2D(path)`。**

链条上除了第 4 步(设备特性,当场抛 VUID),**前面任何一层错了都是静默的**。

# 设计漏洞题库 + 预测日志

> 建立于 2026-09-24。规矩见 `CLAUDE.md` 第二节「协作模式」。
>
> **两样东西放一个文件**:都是动手前/动手后要各看一眼的,分两个文件会漏掉其中一个。
>
> **题库只写坐标和线索,不写诊断。** 我先自己想,想不出来再问;问过之后答案才补进那一条。
> 直接读你写好的诊断,和自己在代码里找出来,是两种收获。

---

## 一、预测日志(最新在上)

每轮动手**之前**写,写完才动手。收尾时回来补一行"实际是什么、差在哪"。

**第三行空着是最危险的信号** —— 2026-09-24 复核出的五处错里,至少两处就是因为当时没写这行,
把"猜的"当成了"查过的"。

```
### 2026-MM-DD · <这轮做什么>

我打算怎么做:

我预期会发生什么:

我不确定的地方:

<!-- 收尾时补 -->
实际是什么:
差在哪:
```

### 2026-09-26 · 回退后重做:先给应用层提供资源创建的接口

我打算怎么做:先给应用层提供资源创建的接口

我预期会发生什么:应用层可以开始创建资源

我不确定的地方:会牵扯到其他组件的初始化时机

<!-- 收尾时补 -->
实际是什么:四个 `Renderer::Create*` 立起来只是个开头 —— 它把整条渲染路径一路带了出来。
按顺序:shader 换成 flat_color + texture(`ObjectUbo{model,color}` 进 UBO、
`PushConstants{viewProj}` 走推送常量)、描述符 layout 改成从 shader 反射的并集、
管线换成按 PipelineDesc 缓存的池、贴图挪去 set 1 并加 TextureDescriptorCache、
帧槽加环形缓冲 + dynamic offset、最后在 DrawFrame 里写录制循环。
现在 404 个绘制项能画出来,验证层 0 输出,退出干净。

差在哪:第三行说中了方向,但**范围估小了一档** —— "初始化时机"只是其中一条,还有:

① `layout 从 shader 来` 一落地,layout / pool / pipeline layout / 管线**全部**得
   延迟到第一次 DrawFrame,于是有了 `EnsureRenderState`;
② `贴图和 UBO 分两个 set` 不是可选项 —— 挤在 set 0 里"贴图变了才绑"就没有落点;
③ **退出路径(目标 7)是被这一步逼出来的**:在这之前引擎不持有任何应用层资源,
   析构顺序错了也看不出来。第一次有 layer 握着 GPU 资源,`~Application` 只调
   `Shutdown()` 的问题就暴露了(23 个对象泄漏 + `vulkan_raii.hpp:13703` 断言 + 挂住)。

另外两件没预料到的:

- 用户那版"帧开头检查 + 单缓冲"**不需要双缓冲** —— `DrawFrame` 收到 items 时就知道
  这一帧要画几项,检查点放在 `waitForFences` 之后就是安全的。
- `VulkanCommandPool` 里挂着原型时代的 `recordCommandBuffer`(0 调用者),是删
  `Vertex` 才把它带出来的。
差在哪:

---

## 二、题库

**未解。** 自己找,找不出来再问 Claude。

| # | 坐标 | 线索 | 难度 | 状态 |
|---|---|---|---|---|
| 1 | `Fish/src/Renderer/Renderer.h:56` | 一个成员函数,没有任何调用者 | 热身 | ⬜ |
| 2 | `Fish/src/Core/Application.cpp:84` 和 `:113-116` | 一个标志,恒为某个值 | 容易 | ⬜ |
| 3 | `Fish/src/Platform/Vulkan/VulkanPipelineCache.h:46-47` + `Application.cpp:46-47` | 有个字段的寿命比缓存它的人短 | 中等 | ⬜ |
| 4 | `Fish/src/Platform/Vulkan/VulkanFrameData.h:41` | 一个数字,和实际对不上 | 中等 | ⬜ |
| 5 | `Fish/src/Platform/Vulkan/VulkanReflection.cpp:30-47` | 这段代码有一条路径从没被执行过 | 难 | ⬜ |
| 6 | `Fish/src/Platform/Vulkan/VulkanSwapChain.cpp:127-133` + `Application.cpp:117` | 窗口状态有两条独立路径,其中一条是死的 | 难 | ⬜ |

**做题的方法**:先只看坐标,想清楚"会出什么事、什么条件下会出",再看下一行。

**自己找到的也应该往里加。** 这个题库如果只有 Claude 出的题,练的就只是"读他的报告"。

---

## 三、已解归档

2026-09-24 复核 `RENDERER_ARCHITECTURE.md` 时翻出来的,当场讨论过,记在这儿备查。

| 坐标 | 结论 |
|---|---|
| `RENDERER_ARCHITECTURE.md` 第一节 / 第五节 | 写的是 `Renderer::CreateShader(path)`,实际签名是 `CreateShader(name, spvPath)`(`Renderer.h:39`) |
| `Fish/src/Platform/Vulkan/VulkanFrameData.h:41` | 注释写"Playground 现在 402 个(400 方块 + 2 贴图)",实际 **404**(漏了三角形和 `playground2D`)。正文的 404 是对的 |
| `Fish/src/Renderer/Renderer.h:56` | `GetDeviceContext()` 零调用者。**HEAD 里就有**,不是那轮新增 |
| `Fish/src/Core/Application.cpp:113-116` | `m_minimized = true` 之后紧跟一句无条件的 `m_minimized = false`,标志恒为 false。最小化实际是被 `VulkanSwapChain.cpp:130-133` 的 `while(width==0) glfwWaitEvents()` 自旋吸收的 |
| `PIPELINE_REFACTOR_TODO.md` S1 | "查过 `D:\Vulkan_SDK\cmake\` 不存在" —— **目录存在**,里面全是 SDL2/SDL3 的 config。`find_package(Vulkan)` 走的是 CMake 自带的 `FindVulkan.cmake`,`grep reflect` 0 命中。**结论对、证据错,而且写着"查过"** |
| `PIPELINE_REFACTOR_TODO.md` S2 | 那张"反射 vs 反汇编"对照表,对象 `slang.spv` 已在 S6 删除 → **这份验证最强的表已无法复现** |

---

## 四、坑的分类

`CLAUDE.md` 第二节提到"值得亲手撞的坑就那么几个",分类在这。

| 类 | 特征 | 例子 | 怎么办 |
|---|---|---|---|
| **值得踩** | 便宜 + 以后会反复遇到 | `cullMode`(Y 轴朝向)、同一 set 号连绑会顶掉、stride/offset 前缀和 | 我自己写,Claude 只报错 |
| **不值得踩** | 贵 + 教训只有一句话 | bug 9(异常吊着):会花几小时误判成死锁,而教训是"main 里包个 catch" | Claude 直接给 |
| **不能踩** | 不可逆 / 污染仓库 | 见 `CLAUDE.md` 的「红线」表 | Claude 动手前打断 |

2026-09-23 那轮 14 个 bug 里,**值得亲手撞的只有 4 个**:7、8、10、14。
最便宜的三个都只要改一行:

| 改什么 | 应该看到 | 复现哪个 bug |
|---|---|---|
| 删掉 `VulkanRendererAPI.cpp:235` 的 `frame.ResetObjects()` | 第 5 帧吊住、无输出、CPU 极低 | 8 + 9 |
| `VulkanPipelineCache.h:57` 的 `cullMode` 改成 `eBack` | 窗口纯色,验证层一条不报,404 个 draw 照发 | 14 |
| `VulkanFrameData.cpp:53` 的 `range` 改成 `VK_WHOLE_SIZE` | 验证层报 `VUID-vkCmdBindDescriptorSets-pDescriptorSets-06715` | 10 |

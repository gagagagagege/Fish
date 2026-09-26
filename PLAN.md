# Fish 引擎 · 实习冲刺计划

> 制定于 2026-09-14。后面几个月会反复回来看，有变动直接改这个文件。

## 一、目标与定位

| 项 | 内容 |
|---|---|
| 身份 | 双非，大三，2028 届（2028.06 毕业） |
| 目标 | 大厂图形 / 引擎方向实习 |
| 时间投入 | 每天 2-3 小时 |
| 项目定位 | 自己的小引擎，**不是 Hazel 复刻**，简历上的核心项目 |
| 成功标准 | 对 Fish 了如指掌，能解释每一行 |

双非的学历筛选确实存在。但图形 / 引擎方向候选人少、能力可验证，是少数靠项目能突破筛选的岗位。
**内推是最大的杠杆**，要提前铺，不能等投递前才找。

## 二、技术方向

### 已定：放弃 OpenGL，后端全换 Vulkan

扔掉 OpenGL 之后，"为什么用 Vulkan" 才有答案——bindless 描述符、GPU 侧决策这些事 OpenGL 做不了。
两个后端并存反而会让 Vulkan 看起来像简历粉饰。

#### OpenGL 遗留代码怎么处理（2026-09-21 定）

OpenGL **不一次改完**——工作量太大。**走到哪改到哪。**

1. 改动牵连到别的 OpenGL 逻辑时，**把被牵连的那部分注释掉**，不要为了它绕路
2. 注释用 `// OPENGL 遗留:` 开头，写明**原来是什么** + **归哪一周**
3. 同时在第六节的**登记表**里加一行：文件、行号、原逻辑、归哪周
4. 做那一周的时候，把登记项和代码注释一起清掉

**没有使用者但还没删的文件**同理（`GraphicsContext.h`、`OpenGLContext.{h,cpp}` 现在就是这种）
——留着，等彻底删 OpenGL 那一步一起清。

找所有遗留点：

```bash
grep -rn "OPENGL 遗留" Fish/src Playground/src
```

### 差异化：bindless → GPU-driven

> **封装是及格线，技术是得分点。**

封装（RAII 正确、不泄漏、接口干净、README 能看）是必做项，但**不产生差异化**。
做到及格就停，不要当成项目做。

得分点走这条阶梯：

| 顺序 | 内容 | 为什么 |
|---|---|---|
| 1 | **Bindless 描述符** | 解掉 Renderer2D 的 32 纹理槽瓶颈，是下一步的前提 |
| 2 | **Indirect draw + compute culling** | "draw 的决策从 CPU 移到 GPU"，这才是 GPU-driven 的本体 |
| 3 | （可选）Compute 粒子 | 视觉冲击强，深度一般 |

设备条件已满足：Vulkan 1.3、`dynamicRendering`、`synchronization2` 都已开启，
描述符索引（bindless 需要的 `runtimeDescriptorArray` / `descriptorBindingPartiallyBound` / `updateAfterBind`）
是 Vulkan 1.3 核心特性，不需要额外扩展。

### 明确不做

- **PBR / 延迟渲染 / 阴影贴图** —— 教程遍地，做完只能证明跟着教程走过一遍
- **MSDF 文字渲染** —— Cherno 刚讲过，属于教程复刻
- **Render graph** —— 价值高但四个月内大概率做不完，风险收益比不合适
- **编辑器** —— 演示效果最好，但技术深度最低，且吃时间

## 三、时间线

| 时间 | 阶段 | 关键动作 |
|---|---|---|
| **2026.09 – 2027.01** | 大三上 | **项目冲刺期**（黄金窗口，课业压力最小） |
| **2027.02 – 03** | 大三下开学 | **暑期实习提前批** ← 第一个真正的目标 |
| 2027.03 – 06 | 大三下 | 暑期实习正式批、面试 |
| 2027.07 – 09 | 暑假 | 暑期实习（争取转正） |
| 2027.09 – 11 | 大四上 | 秋招 |
| 2028.06 | — | 毕业 |

4 个月计划结束后，距离提前批只差一个月——节奏刚好卡在点上。
而且有暑期实习 + 秋招两次机会，缓冲比想象中厚。

### 关于 3D

**现在不加。** 先用手里已有的牌打一次，看反馈再决定。

大厂图形岗确实以 3D 为主，2D 项目能投的岗位范围更窄。但：
- 加 3D 会撑爆这 4 个月，而提前批的机会比多一个 3D 路径更值钱
- 半成品 3D < 成品 2D
- 如果 2027 暑期实习结果不理想，大三下到大四上再加 3D 冲秋招，那时还有一整年

## 四、四条并行线

**必须并行，不能串行。**

| 线 | 内容 | 开始时间 |
|---|---|---|
| **项目** | 见第六节逐周计划 | 现在 |
| **算法** | 大厂笔试 / 一面手撕。**考的是通用题，没有"图形专用算法"** | **现在，每天固定时间** |
| **C++ 八股** | 虚函数表、智能指针、STL 容器原理、内存对齐、链接过程 | 现在，可稍后 |
| **图形八股** | 渲染管线、光栅化、PBR 数学、阴影算法、TAA/MSAA、Z-fighting | 现在，可稍后 |

> 只做项目是最常见的失败模式。
> 项目经历会让你对 Vulkan 细节很熟，但面试官问的是**渲染理论**——这两者要专门补。
> **强项目 + 算法挂 = 一面淘汰**，双非尤其如此，因为学历扣的分要靠面试表现补回来。

**C++ 八股这条线是 2026-09-21 补的**：查面经发现它独立成块，量不比图形八股小，
原来的三条里没有它。

> **从项目里长八股答案，比背有用。** 比如 `GraphicsContext` 没有虚析构而
> `WindowsWindow.cpp:150` 是 `delete m_Context`
> （那是 UB，现在没出事只因为 `OpenGLContext` 平凡析构）；
> `PhysicalDevice` / `DebugUtilsMessengerEXT` 存的是指向 `Instance` 内部 dispatcher
> 的**裸指针**。这些撞过一遍就能讲"我实际踩过"，背下来的答不出追问。

### 算法：去哪刷、刷哪些

**两个平台两个用途，别混。**

| | 用途 | 模式 |
|---|---|---|
| 力扣 `leetcode.cn` | 练题、建立题感 | 核心代码模式（只写函数体，平台管 IO） |
| 牛客 `nowcoder.com` | 适应**笔试** | ACM 模式（自己读输入、自己打印输出） |

**很多人刷了几百道 LeetCode，笔试第一题卡在读不懂输入格式。**
牛客的「历年笔试真题」那块才是要练的，不是「在线编程」。

| 笔试平台 | 谁在用 | 读输入 | 提交 |
|---|---|---|---|
| 牛客 | 主流通道 | `readline()` | 可以先自测再交 |
| 赛码 | 字节、小红书、美团技术岗、360、顺丰 | `read_line()` | **「运行」跑的就是最终评测点，错了直接扣分** |
| 企业自研 OJ | BAT、华为 | 各自为政 | 监考松紧不一 |

Java 两边都要求类名是 `Main`、文件里不能有 `package`。
**考前一定去目标平台做 1-2 道练习题，把 IO 跑通。**

**题单：**

| 顺序 | 题单 | 量 |
|---|---|---|
| 1 | 力扣 **热题 100** | 100 |
| 2 | **面试经典 150**（Top Interview 150） | 150，和热题 100 有重叠，跳过已会的 |
| 3 | **剑指 Offer / LCR** | 约 75。**LCR 就是原剑指 Offer 的新前缀**，不是另一套题单 |

**换算**：每天 30-45 min ≈ 1-2 道中等题。按 1.5 题/天，热题 100 约 67 天 → **11 月底刷完**；
面试经典 150 的增量到 2027-01 前后。**会撞上 W15–W16 期末季**，那两周按第六节写的放低。

### 图形岗实际考什么 → 见 `INTERVIEW_NOTES.md`

**考点清单、各家差异、真题实录全部移到了 `INTERVIEW_NOTES.md`**（2026-09-21）。
不在这里留第二份 —— 两份同样内容必然分叉，这个仓库吃过一次亏（`CLAUDE.md` 开头那段）。

一句话版本：**手撕算法常见于一面，难度因公司而异；项目拷打 + 图形学基础才是核心。**
腾讯 IEG 一面会上 1 easy + 1 medium + 1 hard；网易的手撕是 ACM 模式，自己写测试用例。
当前主攻腾讯和网易，岗位是游戏引擎开发。

### ⚠️ 一个未核实的变数：AI Coding 笔试

搜索里反复出现：2026 校招这一轮笔试在往 **AI Coding** 转——不考手写代码，
考需求理解 / 任务拆解 / Prompt 设计 / 与 AI 协作的交付质量，评分会看聊天记录。
提到的公司：蚂蚁 27 届、阿里淘天、美团、京东。

**没能核实。** 来源里有一个是卖「笔试 AI 辅助」的厂商博客（利益相关），其余是二手文章。
**如果它成立，上面「算法」那条线的假设要重做。**
去牛客讨论区搜目标公司今年的笔试形式确认。

## 五、里程碑

| | 内容 |
|---|---|
| **M1** | Vulkan 侧重构：`Buffer` / `Image` / `FrameData`，不碰 Fish |
| **M2** | `VulkanRendererAPI` + Vulkan 初始化，能开窗 + clear |
| **M3** | `VulkanRendererAPI` + 三角形走通 Fish 的 `Renderer::Submit` |
| **M4** | `VulkanShader` / `VulkanTexture2D` |
| **M5** | `Renderer2D` 接进来 ← **核心交付** |
| **M6** | ImGui |

> **M2 不另起类名。**（2026-09-21 改。原先写的是「`VulkanContext` 名字被占了，得换一个」）
> `VulkanContext` 保持原名和现状 —— 它仍然是设备/队列/物理设备那个包装类，
> **不继承 `GraphicsContext`**。M2 改为新建 `VulkanRendererAPI`，由它负责 instance、
> debugMessenger，以及创建 `VulkanContext`。
> `TriangleApp` / `VulkanMain.cpp` 2026-09-21 已删（原型的壳，逻辑收进 Fish）。

**时间不够时的砍的顺序**：ECS → compute culling → indirect
**底线**：bindless + Renderer2D

## 六、逐周计划

起点 **2026-09-14（周一）**，共 18 周。

**假设**：每天 2-3h 是**总可用时间** → 项目约 2h + 算法/八股约 0.5h。
**节奏**：每周留一天不排计划，用来补欠。
**每周日**：花 15 分钟对一次计划，落后了就调整后面几周，不要硬扛。

### 计划和顺序（2026-09-21 定）

**计划不是固定的，挡路了就调顺序。** 但"挡路"和"进度快"是两回事：

| 理由 | 允许吗 | 怎么做 |
|---|---|---|
| **挡路** | ✅ | 把那周的活提前过来做。**但要在表里标明它原本排在哪一周、被什么挡住** |
| **进度快** | ❌ | 只把当周标成完成，后面几周的定义**一个字都不改** |
| **临时改动引起的新任务** | ✅ | 直接加（比如「OpenGL 遗留登记」机制） |
| **改验收标准**（主观排序，不是被挡） | ✅ | **当周「验收标准」那一栏一个字都不改**，差的那部分单独记进下面「画面验收欠账登记」 |

**判据**：挪完之后，表里还能回答"这个本来是 W_n 的活"。
**改验收同理**：原标准留在表里别动，实际做到什么记在别处 —— 否则半年后读起来
像是"这周本来就只要求接上线"。

进度快于计划**本身是产出**，要让它显形——计划和现实永远贴在一起的话，
"我一直跑在计划前面"这件事就看不见了。

> 2026-09-21 犯过：W3 完成后，我把 W4 那格改成「最小帧循环 + 关掉 ImGui 层 +
> Playground 换 layer」，理由是"它们挡路"。**挡路这个理由成立，问题出在我没标明
> 它们原本排在哪周**——读起来像是 W4 本来就这么定义的。已改回原样，
> 只把 `VulkanContext` 这个名字更正成 `VulkanRendererAPI`（那是改名，不是挪活）。

| 周 | 日期 | 项目主线 | 验收标准 | 状态 |
|---|---|---|---|---|
| W1 | 09/14 – 09/20 | git 整理（两个仓库）、简历初稿、M1 开工 | 两仓库都有首次提交；`Buffer` 骨架能编译 | ✅ 完成 |
| W2 | 09/21 – 09/27 | **M1 完成** | vulkan_project 跑起来，旋转贴图四边形正常；6 个调用点全改成类构造；**开始投日常实习** | ✅ 完成 |
| W3 | 09/28 – 10/04 | 源码搬进 `Fish/src/Platform/Vulkan/` + 加 `Vulkan` 前缀<br>`VulkanRendererAPI` 骨架 + 接进 `Renderer` | 空实现能编译链接 | ✅ 完成 |
| W4 | 10/05 – 10/11 | `VulkanRendererAPI` 完整（**国庆，可冲刺**） | Fish 能开窗 + clear 成纯色 | ✅ 完成 |
| W5 | 10/12 – 10/18 | `VulkanRendererAPI : RendererAPI`；帧模型 | `BeginFrame/EndFrame` 跑通，每帧 clear | ✅ 完成 · 形状不同，见下注 |
| W6 | 10/19 – 10/25 | 顶点/索引缓冲抽象；pipeline 创建 | 静态图形能画出来 | ✅ 完成 |
| W7 | 10/26 – 11/01 | **M3**；swapchain 重建接入 `onWindowResize` | **三角形走通 `Renderer::Submit`**；resize 不崩 | ✅ 完成 |
| W8 | 11/02 – 11/08 | `VulkanShader : Shader`；uniform / descriptor 抽象 | shader 和 uniform 走 Fish 的抽象 | ✅ 完成 |
| W9 | 11/09 – 11/15 | **M4**：`VulkanTexture2D : Texture2D` | 贴图四边形走 `Renderer::Submit` | ✅ 完成 |
| W10 | 11/16 – 11/22 | Renderer2D 设计（看 Hazel，**不抄**） | 设计草稿：顶点组织、批处理策略、flush 条件 | 🔵 进行中 |
| W11 | 11/23 – 11/29 | Renderer2D 实现：攒 quad、flush、per-frame 顶点缓冲 | 单个 quad 能画出来 | ⬜ 未开始 |
| W12 | 11/30 – 12/06 | **M5：Renderer2D 完成** | 1000+ 精灵批处理、帧率稳定；**commit + push + 录 demo 截图** | ⬜ 未开始 |
| W13 | 12/07 – 12/13 | **弹性周**（补欠 / 提前调研 bindless） | 前面落后的在这里补齐 | ⬜ 未开始 |
| W14 | 12/14 – 12/20 | Bindless 描述符 | Renderer2D 突破 32 纹理槽限制 | ⬜ 未开始 |
| W15 | 12/21 – 12/27 | GPU-driven：indirect draw（**期末季，预期放低**） | | ⬜ 未开始 |
| W16 | 12/28 – 01/03 | GPU-driven：compute culling（**期末季，预期放低**） | draw 决策在 GPU 侧 | ⬜ 未开始 |
| W17 | 01/04 – 01/10 | 收尾：demo 视频、README、过程文档 | 三件套齐 | ⬜ 未开始 |
| W18 | 01/11 – 01/17 | 缓冲 + 提前批准备 | 简历定稿、内推已铺、项目讲解稿（3 分钟版 + 10 分钟版） | ⬜ 未开始 |

> **W1–W9 是 2026-09-26 一次结清的**，当时日期还在 W2 里（提前约 7 周）。
> 那天从 `39a1f0d` 起把 2026-09-23 那轮按复演敲定的形状重做了一遍，
> 之后 404 个绘制项一路画到屏幕上。
>
> **W5 的形状和计划不一样**：计划写的是 `BeginFrame/EndFrame` 拆开，
> 实际做的是 `DrawFrame(items)` 单函数版 —— 提交队列和遍历录制都收在一个函数里。
> 验收标准那一栏一个字没改，偏离记在这儿。
>
> **W5 还欠一件事**：见下面「OpenGL 遗留登记」，那三条还没清。

### 画面验收欠账登记

> 由 **2026-09-23 的决定**产生：**按后端建立顺序和 Fish 链接，暂时不做画面上的验收。**
> 触发理由不是"挡路"，是主观排序（宁可一段时间看不见输出，也要先把后端接完）。
>
> **上面那栏「验收标准」一个字都没改** —— 照旧写着"能画出来"。实际交付时只做到
> "链路通 + 验证层干净"，差的部分记在这里。**结账时把对应行删掉。**

**W4 / W5 / W6 / W7 / W9 这五行 2026-09-26 结清、已删。** 那天跑通之后逐项确认过：

| 被降级的周 | 实际做到 |
|---|---|
| W4 | 开窗 ✅ / 后端对象全建出来 ✅ / 干净退出 ✅ / **clear ✅**（画面上底色是设定的 `0.1,0.1,0.1`） |
| W5 | `DrawFrame(items)` 单函数版跑通 ✅ / 逐帧 clear + present ✅ / **画面 ✅** |
| W6 | **静态图形画出来了 ✅** —— 404 个绘制项,网格 + 三角形 + 两张贴图 + 第二个 layer 的方块 |
| W7 | **三角形 ✅**（白色、不透明,说明 stride 28 那条管线的顶点属性前缀和是对的）；**resize ✅ 真人拖过**；最小化 / 还原 ✅ |
| W9 | **两张不同的贴图 ✅**（棋盘格 + Cherno logo,说明每张贴图各一个 set 1） |

还剩两行没做：

| 归哪周 | 被降级的周 | 原验收标准 | 实际做到 |
|---|---|---|---|
| | W11 | 单个 quad 能画出来 | |
| | W12 | 1000+ 精灵批处理、帧率稳定；commit + push + 录 demo 截图 | |

**「归哪周」这一列是空的 —— 画面欠账集中到哪一周结，那一格要你自己定。**

**验证的边界**（2026-09-26）：上面这些是"看四样东西都在、位置和颜色对"级别的确认，
**不是逐像素比对**。帧率、批处理效率、大场景下的表现都没测。W11/W12 的验收要另说。

### OpenGL 遗留登记

> 由第二节「OpenGL 遗留代码怎么处理」产生。**做对应那一周时清掉——代码注释和这里的行一起删。**
> 代码里的位置：`grep -rn "OPENGL 遗留" Fish/src Playground/src`

| 归哪周 | 位置 | 原逻辑 | 现状 |
|---|---|---|---|
| **W5** | `WindowsWindow.cpp` `OnUpdate()` | `m_Context->SwapBuffers();` —— OpenGL 的 present | 已注释 |
| **W5** | `WindowsWindow.cpp` `Init()` | `SetVSync(true);` | 已注释 |
| **W5** | `WindowsWindow.cpp` `SetVSync()` | `glfwSwapInterval(1/0)` | 已注释。Vulkan 侧对应交换链的 `VulkanSwapChain.cpp:81` `choosePresentMode` |
| **M6** | `Application.cpp` 构造函数 | `m_ImGuiLayer = new ImGuiLayer(); PushOverlay(...)` | 已注释。`ImGuiLayer::OnAttach` 走 `InitForOpenGL` + `OpenGL3_Init`（`ImGuiLayer.cpp:48-49`），窗口现在是 `GLFW_NO_API` 没 GL context |
| **M6** | `Application.cpp` `run()` | `m_ImGuiLayer->Begin() / End()` | 已改成判空跳过 |
| **M6** | `Application.h:33` | `ImGuiLayer* m_ImGuiLayer;`（**原来没初始化**） | 改成 `= nullptr`，判空要靠它 |
| ~~**W8/W9**~~ | `Playground/src/Playground.cpp` `Playground()` | `PushLayer(new ExampleLayer())` / `PushLayer(new playground2D())` | **2026-09-26 已清**：两个 layer 改成走 `Renderer::Create*` + `Submit(DrawItem{...})`，放回来了 |
| 彻底删 OpenGL | `Renderer/GraphicsContext.h`、`Platform/OpenGL/OpenGLContext.{h,cpp}` | 现在没有使用者，还没删 | 留着 |
| 彻底删 OpenGL | `Fish/src/Platform/OpenGL/` 其余 12 个文件、`CMakeLists.txt` 里的 `glad` | | 留着 |

> **归到 W5 的理由**：present 和 VSync 都属"帧模型"，W5 那格写的就是它。
>
> **2026-09-26：W5 已经标完成，但上面那三条还没清。** 现在 `WindowsWindow.cpp`
> 里那三处仍是注释掉的原实现（`:3` / `:53` / `:162` / `:172`）。
> 表里说"做对应那一周时清掉"，所以这是**欠着的**——归到 W5 的账没结完。
> W4 的验收标准是「开窗 + clear 成纯色」，跟 present 有交叉 —— 但那是 W4 到时候的事，
> **不提前改 W4 的定义**（见下面的「不要提前」）。

> **进度更新 2026-09-19**（本文件是唯一一份；`D:\vulkan_project` 里那份已删）
>
> **W1 / W2 完成。W3 的「源码搬进 Fish」提前做了 —— 现在停在 W3 的「`VulkanContext` 骨架」之前。**
>
> W2 的「vulkan_project 跑起来」按 CLAUDE.md 的验证标准复核过：构建 0 错误 0 警告、
> 12 秒墙钟烧 18.5 秒 CPU（渲染循环全速跑）、`WM_CLOSE` 退出 exit=0、stderr 0 字节。
> **画面（旋转贴图四边形）我没有亲眼看过。**
>
> W3 的搬运：31 个文件进 `Fish/src/Platform/Vulkan/`（27 个源码 + `shaders/` `textures/`），
> 除 `TriangleApp` 外都加了 `Vulkan` 前缀。构建 **0 错误 3 警告**，但那 3 条都在 Fish
> 原有文件里（`OpenGLVertexArray.cpp:66`、`Renderer/Buffer.h:24`），**不是这次引入的** ——
> 也就是说「0 警告」这条标准本来就达不到。`Playground` 10 秒墙钟烧 9.95 秒 CPU、
> `WM_CLOSE` 干净退出、stderr 0 字节。
> **搬进来的 Vulkan 代码目前全是死代码** —— 没有任何东西 new `TriangleApp`。
>
> 未确认的两项：W1 的「简历初稿」、W2 的「开始投日常实习」—— 这两件我这边没有任何依据。

> **进度更新 2026-09-21**
>
> **W3 ✅ 完成，做过头了。** 原本的边界是「`VulkanRendererAPI` 骨架」，实际做到「Vulkan
> 初始化真的跑起来」：
>
> - 新建 `Platform/Vulkan/VulkanRendererAPI.{h,cpp}`：持 `Context` / `Instance` /
>   `DebugUtilsMessenger` / `Surface` / `unique_ptr<VulkanContext>`
> - `Renderer` 改成**运行时创建后端**（`Renderer.cpp` 的 `Init` 按 `RendererAPI::GetAPI()` 分派），
>   `RendererAPI::Init()` 加 `void* nativeWindow` 参数
> - 加 `Renderer::GetDeviceContext()`（`Renderer.h` 里只前置声明 `class VulkanContext;`，
>   公共头不出现 Vulkan 类型）
> - `WindowsWindow` 按 Vulkan 建窗（`GLFW_CLIENT_API = GLFW_NO_API`），`m_Context` 成员删掉
> - **删**：`TriangleApp.{h,cpp}`、`VulkanMain.cpp`、`RenderCommand.{h,cpp}`
> - 新增「OpenGL 遗留登记」机制（第二节规则 + 本节登记表）
>
> **验证**：构建 0 错误；**Vulkan 初始化跑通了** —— stderr 只有 37 字节（glad 的
> `Failed to initialize OpenGL loader!`），**零验证层输出**，说明 `createInstance` /
> `setupDebugMessenger` / `createWin32Surface` / `VulkanContext` 全部干净通过。
>
> ⚠️ **进程现在停在 `Shader.cpp:14` 的 `FS_CORE_ASSERT(false, "Unknown RendererAPI!")`** ——
> `Playground` 的 layer 还在走 OpenGL。要往前走，先处理 ImGui 层（`Application.cpp:26-27`）
> 和 `Playground` 的两个 layer。
>
> ⚠️ **一条旧数据对不上**：09-19 记的「`Playground` 10 秒墙钟烧 9.95 秒 CPU」今天不复现，
> 三次采样 4.02/8、3.83/6、3.83/6，约 50–64% 单核。**原因未查明**（怀疑那次的窗口被遮挡，
> vsync 不生效变成全速空转）。要判的话得用 `git worktree` 做对照实验。

> **进度更新 2026-09-23**
>
> **改验收标准，不排序。** 决定：**按后端建立顺序和 Fish 链接，暂时不做画面上的验收。**
>
> 先纠正一点：W4→W9 的**做事顺序**本来就接近这个；真正改的是**验收标准** ——
> W6「静态图形能画出来」、W7「三角形走通 `Renderer::Submit`」、W9「贴图四边形走通
> `Renderer::Submit`」这三条里含"看得见"，现在降级成"链路通 + 验证层干净"。
> 新增了上面那张「画面验收欠账登记」表来记这笔欠账。
>
> **链接的工作顺序 —— 照抄 `TriangleApp::initVulkan()`（原文件在 `git show 6258eca^` 里）：**
>
> | 步 | 做什么 | 依赖从哪来 | 状态 |
> |---|---|---|---|
> | 1 | `SwapChain` | `VulkanSwapChain.h:17` 收 `(VulkanContext*, GLFWwindow*)` | ✅ |
> | 2 | `DescriptorAllocator` | `VulkanDescriptorAllocator.h:33` 收 `(VulkanContext*, maxSets)` | ✅ |
> | 3 | `Pipeline` | `VulkanPipeline.h:18` 收 `(…, swapChainImageFormat, descriptorSetLayout)` ← 依赖 1 和 2 | ✅ |
> | 4 | `CommandPool` ×2（主 / 一次性传输） | `VulkanCommandPool.h:18` 收 `(VulkanContext*)` | ✅ |
> | 5 | `texture` | `VulkanTexture.h:16` 收 `(VulkanContext*, CommandPool&, path)` ← 依赖 4 | ✅ |
> | 6 | `Frames` | `VulkanFrameData.h:70` 收 `(…, textureView, textureSampler)` ← **依赖 5** | ✅ |
> | 7 | 5 个 adapter 类 + 6 处工厂分支 | 见下 | ⬜ |
>
> > 2026-09-23 更正：这张表原先是我**从构造函数签名推的**，把 `CommandPool` 排在第 2、
> > `Pipeline` 排在第 6。原型里 `Pipeline` 在 `CommandPool` 之前（它只要 swapchain format
> > 和描述符布局，不需要命令池）。**按原型来**——那是真跑过的顺序。
>
> **第 6 步是这次改序最直接撞上的地方**：`Frames` 的构造函数要贴图的 view/sampler
> （原型的形状 —— 一个旋转贴图）。所以贴图（W9）会被提到帧模型前面，不是可选。
>
> **要新写 5 个 adapter 类**（那批 Vulkan 类不实现 Fish 的任何接口）：
> `VulkanVertexBuffer : VertexBuffer`、`VulkanIndexBuffer : IndexBuffer`、
> `VulkanVertexArray : VertexArray`、`VulkanShader : Shader`、`VulkanTexture2D : Texture2D`。
> 这原本是 W6/W8/W9 的活。
>
> **两个接口形状问题，碰到时再定**（都是"抽象切在哪层"，属于你要想的）：
> - `Renderer.cpp:39-40` 的 `Submit` 硬转 `OpenGLShader` 再 `UploadUniformMat4`
> - `RendererAPI::DrawIndexed(vertexArray)` 只收一个 VertexArray —— 这是 OpenGL 的形状
>   （绑 VAO → `glDrawElements`），Vulkan 侧还要 pipeline + 描述符 + 命令缓冲
>
> **已知的硬约束**：这批 vk::raii 成员的**声明顺序**会被析构顺序约束 —— 命令缓冲必须早于
> `CommandPool`、描述符集必须早于 `DescriptorAllocator`（`CLAUDE.md` 第六节第 2、3 条）。
> 往 `VulkanRendererAPI` 里加成员时按这个排，加错了不报编译错误，是退出时踩空。
> 现在的声明顺序是从 `TriangleApp.h` 抄的。
>
> ### 析构：2026-09-23 定并验过
>
> 原来 `Renderer.cpp:19` 是 `s_RendererAPI = new VulkanRendererAPI;`，**全仓库没有一处
> `delete`**，`Application::~Application` 是空的。所以整套 vk::raii（instance / device /
> swapchain / pipeline / frame 的缓冲和描述符集）**一次都没销毁过**，靠驱动兜底。
>
> **决定：在 `Application` 的析构函数里销毁。** 落成 `Renderer::Shutdown()`
> （`Renderer.cpp`）由 `Application::~Application` 调用 —— 必须在**函数体**里，
> 不能等成员析构：`m_Window` 是后于这里销毁的，而 surface / instance 的销毁要用到它。
>
> ⚠️ **`RendererAPI` 必须有 `virtual ~RendererAPI()`。** 这条不是照本宣科，实测过 A/B：
>
> | `~RendererAPI` | 派生类析构 | `exit` | 验证层输出 |
> |---|---|---|---|
> | 非虚 | **不执行**（临时打印没出现） | **0** | **0 字节** |
> | virtual | 执行 | 0 | 0 字节 |
>
> **两种写法在退出码和验证层输出上完全一样。** 只有加一行临时打印才分得出
> —— 非虚那一版 `delete s_RendererAPI` 只跑基类那层，`vkDestroyDevice` /
> `vkDestroyInstance` 全没发生，而进程照样干净退出。
> （和 `PLAN.md:110-113` 记的 `GraphicsContext` 那件事同一类，但这次验证层也不会报。）
>
> **验过的**：`WM_CLOSE` 退出、`exit=0`、stderr 0 字节（临时打印拆掉后），
> 窗口前置条件断言过（`Get-Process` 拿到 "Hazel Engine"、`CloseMainWindow()` 返回 `True`）。
> 整套拆解顺序无误 —— 验证层不报"池里还有未释放的 set"这类错，就是顺序对了的证据。
>
> `Renderer::s_SceneData`（`Renderer.cpp:11` 静态初始化时就 `new`，同样只 new 不 delete）
> 一并在 `Shutdown()` 里清掉了。它是纯 POD，**"那 64 字节被释放"没有可观测的副作用**，
> 所以这条只做到了"编译过、跑起来退出干净"——没有独立证据，和上面那批 vk::raii 不一样。
> 多出来的一层风险：`LayerStack` 是 `~Application` **之后**才析构的，哪天有 layer
> 在析构里碰 `Renderer`，这里的置空会变成空指针解引用（现在没有 layer，不会发生）。

> **进度更新 2026-09-23（下半段）：W5 的帧模型接上了**
>
> **`Renderer::DrawFrame()` 落地**，`Application::run()` 每帧调一次：各 layer 的 `OnUpdate`
> 先跑完，再画一帧。**没有拆成 Begin/End** —— 拆开会让调用方背一个"必须成对"的契约，
> 而 acquire 拿不到图时想跳帧就会破坏它。合成一个函数后，原型那句 `return` 就够用了。
>
> **这里定了一条后续要遵守的规矩：`Submit` 只入队，不碰命令缓冲。**
> 命令缓冲只在 `DrawFrame` 里开着。这正是 Renderer2D（W10–W12）的形状
> —— Submit 攒批、Flush 时录。所以 `OnUpdate` 放前面不是绕路，是提前对齐了终点。
> `DrawFrame` 里已经留了位置注释。
>
> **改了一个接口名：`setViewport(x,y,w,h)` → `NotifyWindowResized()`（无参）。**
> Vulkan 侧 viewport/scissor 是录制时按交换链 extent 设的，那四个参数一个都用不上。
> 连带 `Renderer::onWindowResize(w,h)` → `Renderer::NotifyWindowResized()`，
> `m_Viewport` 成员删掉（已经没人读）。`OpenGLRendererAPI` 的 override 按遗留机制注释掉。
>
> **删了一个接口：`RendererAPI::Clear()`。** 动态渲染下没有 render pass，
> 清屏是 `beginRendering` 的 `loadOp + clearValue`，不是一个能单独发的命令。
> `SetClearColor` 的值现在由 `BeginFrame` 消费。连带改的：
> `Renderer::Clear()`、`VulkanRendererAPI::Clear()` 删掉；
> `OpenGLRendererAPI::Clear()` 和两个 Playground layer 里的调用按遗留机制注释掉。
>
> **撞到一个真 bug（已修）**：第一版退出时**193 行验证层报错**，全是
> `vkDestroyFence / vkDestroySemaphore / vkFreeCommandBuffers / vkDestroySwapchainKHR` 的
> "currently in use by VkQueue" —— GPU 还在跑就把资源销毁了。
> 原型的 `TriangleApp::cleanUp()` 里本来有 `device.waitIdle()`，注释都写明了原因，
> **搬进 Fish 时漏了**。补在 `~VulkanRendererAPI()` 函数体里（先于成员析构执行）。
> 修完 stderr 归零。
>
> **两个之前欠着的坑这一格填了：**
>
> - `SwapChain::recreate()` 原来没有调用者。现在 `setViewport()` 置 `m_FramebufferResized`，
>   `BeginFrame` 开头消费它重建（放在 acquire **之前** —— 之后重建会把刚拿到的图像丢掉）。
> - **resize 验过了**：GLFW 回调收到的尺寸（1478x1570）= 重建后交换链的 extent，
>   一模一样，验证层零输出。探针脚本里 Windows **没有采纳请求的高度**（被卡在工作区高度），
>   所以能说的是"尺寸变了、事件到了、按新尺寸重建了"，**不是"精确改成了某个尺寸"**。
>   另外这台机器是 150% DPI 缩放，探针看到的虚拟尺寸要 ×1.5 才是物理尺寸。
>
> **还没做的**：`Renderer::Submit` / `DrawIndexed` 仍是抛异常；没有画任何东西。
> 帧循环现在每帧只做 clear + present。
>
> **一个附带发现**：进程是**全速跑**的（12 秒 5400 帧 ≈ 450fps，8 秒烧 8.8 秒 CPU）。
> `chooseSwapPresentMode`（`VulkanSwapChain.cpp:42`）优先选 Mailbox，Mailbox 不节流。
> 这正是 `PLAN.md` 上面那条「09-19 的 CPU 数据今天不复现」待查的原因 ——
> 不是窗口被遮挡，是 present mode 决定的。

### 关于 W15–W16（期末）

大三上期末通常在 12 月底到 1 月初。这两周**只保证不断线，不保证进度**。
如果 GPU-driven 没做完，挪到 W17–W18——**它的优先级低于"三件套齐"**，因为
一个能演示的引擎比一个没讲清楚的高深技术有用。

### 每日节奏（建议）

| 时段 | 内容 |
|---|---|
| 固定 30-45 min | 算法题（不可挪用）。**练在力扣，每周抽 1-2 道去牛客按 ACM 模式重写** |
| 主时段 1.5-2h | 项目 |
| 零碎时间 | 图形八股 / C++ 八股（通勤、排队，看文档/记笔记） |

## 七、教程节奏

**停掉线性跟更 Hazel。** Fish 的骨架（`Application` / `Layer` / `Event` / `Window` / `Renderer` /
`RenderCommand`）已经和 Hazel 对齐，那是教程最值钱的部分，已经拿到了。

剩下值得看的只有两块：

1. **Renderer2D 的设计**（W10） —— 看，理解，然后自己按 Vulkan 实现
2. **ECS 的概念** —— 看，理解，然后自己写（Cherno 那个 ECS 很朴素，自己写个 archetype 版的深度更高）

其余（文字渲染、批处理优化、编辑器）按需回查，不要线性跟。

### 核心判断

> **Hazel 的 Renderer2D 代码你必然会重写。**

那一版建立在"CPU 填 VBO → `glDrawElements`"上，而 Vulkan 上形状完全不同
（多帧缓冲、command buffer 重录、描述符绑定）。跟抄一遍再改，等于写两遍。

**规则：看设计，不抄代码。** 值钱的是设计意图（为什么按纹理切换 flush、顶点数据怎么组织、
`BeginScene` / `EndScene` 的语义），这些是 API 无关的。

### 一个纪律

**不要把"跟教程"和"写集成"混在同一次会话里。** 每天 2-3 小时不算多，
上下文切换的成本占比会很高。要跟教程就一次看一批（比如周末），工作日专心写集成。

## 八、立刻要做的

- [x] **Fish 项目提交**（✅ 已完成。**不开分支，全部提交到 `main`** —— 见下方说明）
      ```bash
      cd /d/Fish
      git add -A && git commit -m "..."
      git push origin main
      ```
- [x] **`D:\vulkan_project` 建仓 + 首次提交**（✅ 已完成）

> **2026-09-19 改：不开分支。**
> 这条原来写的是 `git checkout -b vulkan-backend`（实际建出来叫 `vulkanAPI`）。
> 理由是「main 保持随时能跑」——但对你**不成立**：单人项目、没协作者、没 release、没 CI，
> 而且第二节已经定了放弃 OpenGL，「保住能跑的 main」保的是个要扔的东西。
> **反过来的坏处是实的**：GitHub 默认分支就是 main，继续在分支上做，投简历时对方
> 打开看到的是九月的空骨架。
> `vulkanAPI` 分支已 fast-forward 合进 main（`4d148a7..74eb967`，零冲突）。
> **以后所有工作直接提交到 main。** 想要「随时能回到能跑的节点」，用 tag 打里程碑
> （`M1`…`M6`），不用分支。
- [ ] **开始刷算法**，每天固定时间，不要等
- [ ] **写简历初稿**（W1 内）
- [ ] **投日常实习**（W2 开始），不用等大厂，小厂 / 创业公司 / 远程都行
- [ ] **开始铺内推人脉**（牛客、GitHub、学长学姐）—— 2027 年 2 月才找就来不及了
- [ ] **做 M1**

### 为什么要现在投日常实习

1. **简历上有实习经历，暑期实习筛选通过率明显不一样。** 双非需要它对冲学历
2. **面试经验是消耗品。** 大三上投，挂了不影响什么，还能把"第一次技术面试"提前消化掉
3. **内推人脉要提前建**

## 九、风险与纪律

### 最大的风险

**大三下的课业 + 等"完美"再投。**

提前批 2027.02 开始时，如果项目还没成形，一拖就到 4 月，很多名额没了。
**1 月能跑出 demo 视频 + 一份讲得清楚的项目，就够投了。** 项目永远不会有"做完"的那天。

其次是 **W15–W16 的期末季**，已经在第六节留了后手。

### 简历项目的真正风险

不是功能少，是**不可辩护**。

面试官不会问"你实现了什么"，会问"最难的一个 bug 是什么""如果重来你会怎么改""为什么这里用 X 不用 Y"。
教程复刻在这三个问题上会立刻露底。

- **我给的代码，你必须能解释每一行。** 哪次没法当场讲清楚，直接问为什么
- 别为了赶进度跳过"这里为什么这么写"的思考。这个项目没有 deadline，唯一的产出是你自己

### 协作约定

| | 问 |
|---|---|
| Vulkan API 事实、验证层报错含义、某个 flag 的作用、GL/VK 语义差异 | ✅ 随便问（这是查资料，不构成学习） |
| 抽象切在哪层、接口什么形状、为什么这样取舍、先做哪个 | ❌ 自己想（这是判断，是你的收获） |

判据：**答案是可查证的事实就问，是取舍就自己想。**

#### 现有接口不是约束（2026-09-23 定）

**该加的函数就加，该删的就删。老函数只是架构设计上的参考，不是要保住的东西。**

Fish 的渲染接口是从 Hazel 抄来的，那是 OpenGL 的全局状态机形状
（绑 shader → 传 uniform → 绑 VAO → draw）。Vulkan 没有全局状态：要么在建时烘进
pipeline，要么在录制时烘进命令缓冲。硬保形状 = 把 OpenGL 的假设带进 Vulkan。

已知对不上的三处（截至 2026-09-23）：

| 接口 | 现状 | 问题 |
|---|---|---|
| `RendererAPI::DrawIndexed(vertexArray)` | 只收一个 VertexArray | OpenGL 形状。Vulkan 还要 pipeline + 描述符集 + 命令缓冲 |
| `Renderer::Submit` | `Renderer.cpp:52` 硬转 `OpenGLShader` 再 `UploadUniformMat4` | `Shader` 抽象里根本没有 uniform 接口 |
| `Texture2D::Bind(slot)` | `Frames` 构造时就把一张贴图绑进描述符集（`VulkanFrameData.h:33-34`） | 描述符模型下"绑定"不是这个意思 |
| `VertexBuffer::Bind()` / `VertexArray::Bind()` | GL 的状态切换 | Vulkan 里是录制命令（`vkCmdBindVertexBuffers`），不是状态 |

**边界**：这条说的是"现有形状不神圣"，**不是"抽象切在哪层可以自己定"**——
上面那张表照旧生效。我不该拿"现在的接口长这样"当不动的理由，但取舍还是要你想。

### 产出定义

四个月后交付的是 **demo 视频 + README + 你自己写的过程文档**。
不是"一个完成的引擎"——那做不完，也不该是目标。

`diary.txt` 是好资产，建议纳入版本管理，但**写你自己的理解，不要贴 AI 对话**。

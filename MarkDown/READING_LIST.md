# 学习资料清单 + 每篇的掌握标准

> 建立于 2026-10-27。配合 `PLAN.md` 的执行顺序：
> **3D 最小实现 → 最小仪表 → 整套 GPU-driven → 工具学习 → 窄优化**。
>
> 每篇后面写的不是章节摘要，是**读完必须能回答 / 能做的事**。
> 读不懂时：先用每篇末尾的「中文检索词」搜视频或文章补概念，再回来读原文。
> 读法：每篇读完写 5 行 —— 问题 / 机制 / 代价 / 我的场景能不能用 / 待验证。
>
> **本机是 N 卡（2026-10-27）**：主线读 NVIDIA / 通用资料；AMD 的 RGP、RDNA Guide、
> RDNA 4 ISA 留作对照，不占首轮时间。

## N 卡首轮顺序

1. 0.1 → 0.3 → 0.4（0.2 选读）
2. 1.3 Ada Tuning Guide
3. 4.1 RenderDoc + Nsight Graphics 入门
4. 1.4 RTX 4090 微基准（照着测一次）
5. 2.1 AC Unity → 2.2 Nanite → 2.6 meshoptimizer
6. 其余按需

AMD 线（1.1 RGP、1.2 RDNA Guide、1.6 RDNA 4 ISA）不进首轮；跨平台面试时再补。

## 掌握标准

一篇算“读过”，要满足四条：

1. 能用自己的话讲清楚它解决的**问题**；
2. 能说出它的**机制**和**代价**（不背广告词）；
3. 能在 `Fish` 的代码里指出对应位置，或者指出“我们现在没有这一层”；
4. 能设计一个**验证实验**：在他的硬件上测什么、看哪个计数器、预期看到什么。

做不到第 3、4 条，就还没掌握，只是看过。

## 读不懂时怎么补

1. 先用中文视频/文章建立概念（关键词在每篇末尾）；
2. 回到原文，只读 abstract / introduction / 算法章节 / 结论，跳过数学推导；
3. 用“自检问题”检验；答不出就回到对应章节；
4. 在 `diary.txt` 用自己的话写 5 行，不要贴原文；
5. 在项目里找一个对应位置，做一个小实验或测量。

---

# 阶段 0：硬件词汇

## 0.1 A Trip Through the Graphics Pipeline（2011，经典）

- 链接：https://fgiesen.wordpress.com/2011/07/09/a-trip-through-the-graphics-pipeline-2011-index/
- 难度：中；时间：6–8 小时，分 3 次
- 为什么读：建立“一个 draw 在硬件里经过哪些阶段”的词汇。后面所有瓶颈分析都靠这套词。

**必须掌握**

- [ ] 命令缓冲 / 命令处理器，state 设置和 draw 的关系
- [ ] input assembly → vertex shader → primitive assembly / clipping → rasterizer → early-Z → pixel shader → ROP 的顺序
- [ ] immediate mode 和 tile-based 的区别
- [ ] 为什么小 draw 多时 CPU / GPU 前端成本高
- [ ] 带宽、cache、overdraw 分别发生在哪个阶段

**自检问题**

- 能画出一帧从提交到上屏的框图吗？
- “draw call 多为什么慢”能说出三个不同的原因吗？（CPU 录制、前端状态、缓存局部性）

**中文检索词**：GPU 图形管线、光栅化流程、early-z、ROP、tile-based rendering

**Fish 对应**：`VulkanRendererAPI::DrawFrame` 的录制循环；per-item 与 batch 的成本差异。

---

## 0.2 What Every Programmer Should Know About Memory（2007，经典，选读）

- 链接：https://www.akkadia.org/drepper/cpumemory.pdf
- 难度：中；时间：4–6 小时（只读缓存层级、预取、NUMA 几章）
- 为什么读：CPU 侧瓶颈的底子。主机性能有大量时间卡在 CPU 和内存，不是 GPU。

**必须掌握**

- [ ] cache line、层级、命中/未命中延迟量级
- [ ] 顺序访问 vs 随机访问的差异
- [ ] prefetch 能做什么、不能做什么
- [ ] 为什么数据结构布局比“算法复杂度”更能决定实际速度

**自检问题**

- 一个指针追逐实验能测出 L1/L2/内存的延迟差吗？
- 你的 UBO / 顶点数据遍历是顺序的还是跳跃的？

**中文检索词**：CPU 缓存层级、缓存行、内存墙、预取、NUMA

**Fish 对应**：`PushObjectUbo` 的顺序写、批顶点的整块 `memcpy`、per-item 的队列遍历。

---

## 0.3 Gallery of Processor Cache Effects（经典，1 小时）

- 链接：http://igoro.com/archive/gallery-of-processor-cache-effects/
- 为什么读：把 0.2 的概念变成几个马上能复现的小实验。

**必须掌握**

- [ ] cache line 大小对数组遍历的影响
- [ ] 行优先 / 列优先的差异
- [ ] 伪共享是什么、怎么产生
- [ ] 关联度（associativity）导致的冲突

**自检问题**

- 能写出一个“改一行代码，速度差 2–5 倍”的缓存实验吗？

**中文检索词**：缓存行 实验、伪共享、行优先 列优先、CPU cache benchmark

---

## 0.4 Data-Oriented Design（免费在线书，经典/现状）

- 链接：https://www.dataorienteddesign.com/dodbook/
- 难度：中；时间：选读前几章
- 为什么读：主机 CPU 性能的核心是数据布局和批处理，不是继承和虚函数。

**必须掌握**

- [ ] SoA vs AoS 的取舍
- [ ] 把“对象”拆成“数据表 + 索引”的思路
- [ ] 批处理为什么能减少 CPU 工作和缓存未命中
- [ ] 什么时候 OOP 布局会拖慢遍历

**自检问题**

- 能把 `DrawItem` / `DrawBatch` 的字段按“每帧变 / 不变”拆成两张表吗？

**中文检索词**：面向数据设计 DOD、SoA AoS、数据布局 优化

**Fish 对应**：`DrawItem` 和 `BatchVertex` 的字段组织；实例记录的雏形。

---

# 阶段 1：GPU 架构与计数器

## 1.1 AMD GPUOpen 开发/性能入门 + RGP 手册（对照，N 卡可跳过）

- 链接：https://gpuopen.com/learn/getting-started-development-performance/
- 链接：https://gpuopen.com/manuals/rgp_manual/
- 难度：中；时间：3–4 小时
- 为什么读：RGP 是 A 卡上把“架构词汇”变成“计数器证据”的工具。

**必须掌握**

- [ ] wave / wavefront、workgroup、occupancy 的关系
- [ ] VGPR / SGPR、LDS、L1/L2 在 RGP 里怎么看
- [ ] barrier 的代价，以及为什么 compute 会等
- [ ] 如何判断一个 pass 是带宽 bound、占用率 bound 还是延迟 bound

**自检问题**

- 给你一个 capture，能指出“这个 pass 的瓶颈在哪”吗？
- `barrier` 出现在哪些阶段，RGP 里怎么显示？

**中文检索词**：RGP 使用教程、RDNA wave、GPU occupancy 占用率、VGPR

**Fish 对应**：最小仪表、GPU-driven 的 compute pass、窄优化。

---

## 1.2 AMD RDNA Performance Guide（对照，N 卡可跳过）

- 链接：https://gpuopen.com/performance/
- 为什么读：RDNA 上的常见瓶颈和优化建议，和 RGP 计数器配套看。

**必须掌握**

- [ ] occupancy 和延迟隐藏的关系
- [ ] 合并访问 / 非合并访问的代价
- [ ] LDS（共享内存）的使用场景和 bank conflict
- [ ] atomic 和 barrier 对性能的影响

**自检问题**

- 一个 compute culling kernel，怎么判断它是不是被 atomic 或内存访问拖住？

**中文检索词**：RDNA 性能优化、显存带宽、延迟隐藏、LDS bank conflict

---

## 1.3 NVIDIA Ada GPU Architecture Tuning Guide（N 卡主读）

- 链接：https://docs.nvidia.com/cuda/ada-tuning-guide/index.html
- 难度：中；时间：3–4 小时（架构 + best practices 两章）
- 为什么读：N 卡的 SM、warp、寄存器、L1/共享内存、L2 的公开说明。虽然是 CUDA 文档，图形 compute 同样适用。
- 说明：Blackwell 我没核到同等完整的官方调优文档；Ada 这份仍是最好的公开入口。

**必须掌握**

- [ ] SM 里 warp 的调度和延迟隐藏
- [ ] 寄存器压力如何限制 occupancy
- [ ] L1 / shared memory 的 carveout
- [ ] 全局内存合并访问
- [ ] L2 容量和持久化对带宽的影响

**自检问题**

- 为什么“多开线程”不一定更快？
- 你的 culling kernel 用多少寄存器、occupancy 是多少？

**中文检索词**：NVIDIA GPU 架构 SM、warp 调度、occupancy 占用率、L2 cache

---

## 1.4 Microbenchmarking Nvidia's RTX 4090（Chips and Cheese，现状，N 卡主读）

- 链接：https://old.chipsandcheese.com/2022/11/02/microbenchmarking-nvidias-rtx-4090/
- 站点：https://chipsandcheese.com/
- 为什么读：学**测量方法**，不是学 4090 的数字。

**必须掌握**

- [ ] pointer chasing 测延迟
- [ ] 用不同 workgroup 数量扫带宽/占用率
- [ ] 如何设计一个能排除编译器优化的微基准
- [ ] 测出来的数字怎么对应到架构（L1/L2/显存）

**自检问题**

- 能在自己的 GPU 上复现一个“延迟 vs 占用率”曲线吗？

**中文检索词**：GPU 微基准测试、显存延迟 带宽、pointer chasing、GPU benchmark

---

## 1.5 Dissecting the NVIDIA Hopper Architecture through Microbenchmarking（2025，方法论，可选）

- 链接：https://arxiv.org/abs/2501.12084
- 类型：前沿 / 方法论；硬件是数据中心卡，不是游戏卡
- 为什么读：内存层级和 L2 分区的测量方法；知道“架构文档说的”和“实测的”可能不一样。

**必须掌握**

- [ ] 如何测 L2 容量、延迟、分区
- [ ] TMA / DSM 是什么（知道概念即可）
- [ ] 微基准结果如何受测量方法影响
- [ ] 哪些结论不能直接搬到游戏 GPU

**自检问题**

- 你能说出一条“文档没写、只能实测”的架构行为吗？

**中文检索词**：Hopper 架构 微基准、GPU L2 cache 分区、TMA、DSM

---

## 1.6 AMD RDNA 4 ISA Reference Guide（2025，查阅型，N 卡可跳过）

- 获取：GPUOpen 上搜 “RDNA 4 ISA”，2025-04 版
- 用法：**不要通读**。查指令、寄存器、wave 细节时翻。
- 必须掌握：至少知道有哪些执行单元、寄存器类型、wave 大小；其余按需查。
- 中文检索词：RDNA 4 指令集、GPU ISA、wave32 wave64

---

# 阶段 2：引擎级 GPU-driven 与剔除

## 2.1 GPU-Driven Rendering Pipelines（AC Unity，SIGGRAPH 2015，经典）

- 链接：https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf
- 难度：高；时间：6–8 小时
- 为什么读：整套 GPU-driven 的原型。你接下来要做的实例数据、分组、间接绘制、async compute，都能在这里找到对应。

**必须掌握**

- [ ] CPU 粗剔 → GPU 实例剔除 → cluster 剔除 → 索引压缩 → ExecuteIndirect 的完整链条
- [ ] 可见列表 / 命令缓冲 / count 分别在哪个阶段产生
- [ ] 为什么 CPU 不再逐物体发 draw
- [ ] async compute 掩盖了什么等待
- [ ] 阴影剔除为什么比主视图剔除收益更大
- [ ] 代价：复杂度、调试难度、顺序问题

**自检问题**

- 能画出它每一帧的数据流吗？
- 如果只做“GPU 实例剔除 + 每组 indirect”，哪些步骤可以省？

**中文检索词**：GPU Driven 渲染、刺客信条 Unity 渲染、ExecuteIndirect、GPU 剔除、间接绘制

**Fish 对应**：整套 GPU-driven 的骨架；实例记录、mesh 表、分组命令。

---

## 2.2 Nanite: A Deep Dive（SIGGRAPH 2021，现状）

- 链接：https://www.advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf
- 难度：高；时间：8–10 小时
- 为什么读：当前工业界最完整的 GPU-driven 几何管线。你不做虚拟几何，但 culling / HZB / visibility buffer 的思想可以直接用。

**必须掌握**

- [ ] cluster（约 128 三角形）+ DAG 层级 + LOD cut
- [ ] InstanceCull → PersistentCull / ClusterCull 的分工
- [ ] HZB 两阶段遮挡：上一帧可见 → 建 HZB → 再测本帧
- [ ] visibility buffer：存 instance/cluster/triangle ID，后处理材质
- [ ] 小三角形为什么走 compute 软光栅
- [ ] 常驻 GPU 场景 + 流式加载

**自检问题**

- 为什么不能只做一遍遮挡剔除？
- 在你的 2D/3D 场景里，HZB 两阶段能测出什么数字？

**中文检索词**：Nanite 原理、虚拟几何体、cluster culling、HZB 遮挡剔除、visibility buffer

**Fish 对应**：视锥剔除之后的遮挡剔除候选；窄优化的中档/大档。

---

## 2.3 Alan Wake 2: GPU-driven Rendering with Mesh Shaders（Digital Dragons 2024，现状）

- 链接：https://www.alanwake.info/2024/05/18th-may-2024-digital-dragons-2024-erik.html
- 类型：讲座报道（非官方幻灯片）；面向 PS5 / Xbox Series
- 为什么读：当前主机世代的实际做法；mesh shader 和 compute culling 的分工。

**必须掌握**

- [ ] meshlet 是什么，和 cluster 的关系
- [ ] mesh shader 替代了传统管线的哪些阶段
- [ ] meshlet 剔除在 mesh shader 里做，还是在 compute 里做
- [ ] 主机上和 PC 上的差异（如果有）
- [ ] 代价：预处理、内存、调试

**自检问题**

- 你的引擎暂时没有 mesh shader，哪些思想可以用 compute 模拟？

**中文检索词**：mesh shader 原理、meshlet 剔除、Alan Wake 2 渲染、网格着色器

**Fish 对应**：3D 阶段的剔除粒度；未来 mesh shader 方向。

---

## 2.4 Decima Engine: Visibility in Horizon Zero Dawn（Guerrilla，2017，经典）

- 链接：https://www.guerrilla-games.com/read/decima-engine-visibility-in-horizon-zero-dawn
- 为什么读：主机上“百万级对象可见性查询 + 异步计算”的具体做法。

**必须掌握**

- [ ] 可见性查询的输入和输出
- [ ] 异步计算如何和渲染重叠
- [ ] 如何把可见对象收集成批，减少后续管线负担
- [ ] PS4 硬件约束怎么影响设计

**自检问题**

- 如果不用异步计算，这套可见性系统还能不能成立？代价是什么？

**中文检索词**：地平线 零之曙光 可见性、异步计算 剔除、GPU 可见性查询

**Fish 对应**：窄优化的遮挡/可见性方向；主机思路。

---

## 2.5 Wicked Engine（开源引擎，现状）

- 链接：https://github.com/turanszkij/WickedEngine
- 为什么读：能跑、能改的参考实现；比只读论文更容易对照代码。

**必须掌握**

- [ ] 它的可见性列表怎么组织
- [ ] CPU 视锥剔除和 GPU 遮挡查询怎么配合
- [ ] meshlet 遮挡剔除的开关和实现位置
- [ ] 实例批处理和 indirect draw 的代码路径

**自检问题**

- 能把它的 culling 路径画成和你的 `DrawFrame` 类似的图吗？

**中文检索词**：Wicked Engine 渲染、GPU 可见性 剔除 开源、C++ 渲染引擎

**Fish 对应**：实现阶段的参照；对照自己的数据流。

---

## 2.6 meshoptimizer + Billions of triangles in minutes（2025，现状）

- 链接：https://github.com/zeux/meshoptimizer
- 链接：https://zeux.io/2025/09/30/billions-of-triangles-in-minutes/
- 为什么读：cluster LOD 的工程实现和构建期性能分析；比 Nanite 更容易上手。

**必须掌握**

- [ ] meshlet / cluster 生成
- [ ] 合并 + 简化 + DAG 构建
- [ ] 误差指标和 LOD cut
- [ ] 构建期性能优化：稀疏初始化、负载均衡、分配器
- [ ] 内存占用和构建时间的取舍

**自检问题**

- 一个 100 万三角形的 mesh，构建 cluster 需要多少内存和时间？

**中文检索词**：meshoptimizer 使用、meshlet 生成、Nanite 简化、cluster LOD

**Fish 对应**：窄优化的大档；如果做 cluster，先跑它的 demo。

---

## 2.7 The Visibility Buffer（JCGT 2013，经典，选读）

- 链接：https://jcgt.org/published/0002/02/04/
- 为什么读：理解“带宽换计算”的经典取舍；3D 渲染和窄优化都可能用到。

**必须掌握**

- [ ] visibility buffer 存什么（三角形 ID / 实例 ID）
- [ ] 和 G-Buffer 的带宽对比
- [ ] 后处理阶段如何重建重心坐标和属性
- [ ] 代价：重复计算、材质 resolve

**自检问题**

- 你的场景是带宽 bound 还是计算 bound？visibility buffer 适合哪种？

**中文检索词**：Visibility Buffer 可见性缓冲、延迟着色 带宽、G-Buffer 优化

---

# 阶段 3：前沿

## 3.1 SIGGRAPH 2025 Advances in Real-Time Rendering in Games（前沿）

- 链接：https://advances.realtimerendering.com/s2025/index.html
- 用法：**不要全读**。按你的窄优化方向选 1–2 个：
  - idTech8 Global Illumination（DOOM: The Dark Ages）
  - MegaLights（UE5）
  - AVBOIT（Call of Duty）
  - AC Shadows 光追 GI
- 必须掌握（每选一个）：
  - [ ] 它解决的瓶颈是什么
  - [ ] 核心机制
  - [ ] 在什么硬件上跑、代价是什么
  - [ ] 哪一部分能用到你的项目
- 中文检索词：SIGGRAPH 2025 实时渲染、idTech8 GI、MegaLights、AVBOIT

---

## 3.2 Microsoft Work Graphs 文档（2024/2025，前沿）

- 链接：https://learn.microsoft.com/zh-cn/windows-hardware/drivers/display/work-graphs
- 为什么读：GPU 自己生成 work，减少 CPU 往返；知道方向即可。
- 必须掌握：
  - [ ] work graph 的节点、边、内存 backing
  - [ ] 它解决的是哪类 CPU 瓶颈
  - [ ] 平台支持情况（主机落地不明，不要当现状）
- 中文检索词：Work Graphs 原理、D3D12 Work Graphs、GPU 自主调度
- Fish 对应：未来方向，不是现在。

---

## 3.3 UE5.7 Work Graph 工作室博客（前沿，非官方数据）

- 链接：https://www.strayspark.studio/blog/work-graph-rendering-ue5-7-beyond-draw-calls
- 用法：看方向和用例，**数字不要当基准**（单机测试、非 Epic 官方）。
- 必须掌握：PCG / Niagara / 布料这些系统为什么适合 Work Graphs。

---

# 阶段 4：工具

## 4.1 抓帧与计数器

| 工具 | 平台 | 必须掌握 |
|---|---|---|
| [RenderDoc](https://renderdoc.org/docs/) | Vulkan / D3D | 抓帧、看 pipeline state、buffer 内容、barrier、事件列表 |
| [Nsight Graphics](https://docs.nvidia.com/nsight-graphics/) | N 卡 | 帧调试、GPU 计数器、shader 调试 |
| [Nsight Compute](https://docs.nvidia.com/nsight-compute/) | N 卡（CUDA 为主；Vulkan compute 用 Nsight Graphics 看） | kernel 级计数器、occupancy、内存访问 |
| [RGP](https://gpuopen.com/manuals/rgp_manual/) | A 卡 | wave / occupancy / barrier / cache 计数器 |
| [PIX](https://devblogs.microsoft.com/pix/documentation/) | D3D12 | 帧捕获、GPU 时间线 |

**N 卡主用 Nsight Graphics + RenderDoc；RGP 只作对照。**

**自检问题**

- 能在一个 capture 里找到“这一帧有多少 draw、每个 pass 多少时间、barrier 在哪”吗？
- 能看出一个 buffer 被谁写、被谁读吗？

**中文检索词**：RenderDoc 教程、Nsight Graphics 教程、RGP 使用、PIX 教程

## 4.2 帧时与 CPU timeline

| 工具 | 用途 | 必须掌握 |
|---|---|---|
| [PresentMon](https://github.com/GameTechDev/PresentMon) | 帧时、present 模式 | 帧时分布、1% low、present 模式 |
| [Nsight Systems](https://docs.nvidia.com/nsight-systems/) | N 卡 CPU/GPU 时间线 | 帧内各阶段、CPU/GPU 重叠、NVTX 标注 |
| [Tracy](https://github.com/wolfpld/tracy) | CPU timeline | 每帧各阶段耗时、锁、job |

**自检问题**

- 能区分“CPU 卡住”和“GPU 卡住”吗？
- 能说出一帧的时间花在哪几个阶段吗？

**中文检索词**：PresentMon 使用、Tracy Profiler 教程、帧生成 分析

---

# 路线对照

| 项目阶段 | 先读 | 产出 |
|---|---|---|
| 3D 最小实现 | 0.1–0.4 + RenderDoc 入门 | 能画出 3D 一帧的框图 |
| 最小仪表 | 1.3、1.4 + PresentMon/Tracy | 一份 baseline 记录 |
| 整套 GPU-driven | 2.1、2.2、2.5、2.6 | 实例/mesh/剔除/indirect 的数据流 |
| 工具学习 | 1.3、4.1（Nsight / RenderDoc）、4.2 | 能独立分析一个 capture |
| 窄优化 | 2.2、2.4、2.6、2.7、3.1 | baseline vs after 的 A/B |

# 备注

- 经典文献标“经典”是因为概念仍成立，不是“过时”；现状/前沿用来知道今天工业界做到哪。
- 论文里的数字来自别人的硬件和场景，不能直接搬到你的项目；你要带走的是机制和测量方法。
- 每篇的掌握清单是**最低标准**，不是全部内容；读不懂的数学可以跳过，但机制和代价不能跳。

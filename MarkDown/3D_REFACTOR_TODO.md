# 3D 迁移 TODO

> 建立于 2026-10-28,2026-10-30 按依赖重排。
> 顺序按"一条渲染路径从 2D 到 3D 依次要解决的问题"排,原来的 L0–L6 层号不再作为顺序(对照表在最后)。
> 状态只有三种:**已完成** / **可做**(前置都在) / **阻塞**(等某一阶段)。

---

## 已完成

### 阶段 0 · 深度资源(2026-10-28)

| 文件 | 改动 |
|---|---|
| `VulkanImageDesc.h` | `ImageExtent`、`TextureFormat`、`ShadowFormat`、`TextureDesc`、`ShadowMapDesc` |
| `VulkanImage.h/.cpp` | 两个 desc 构造 + 私有 `create`;`createView` 支持 level/layer,aspect 按格式推(`:31-43`) |
| `VulkanTexture.cpp` | 改用 `TextureDesc`,`RGBA8_SRGB`,mip 传 1 |
| `VulkanSwapChain.h/.cpp` | 每张 image 一份深度图,由 `SwapChain` 持有(不混进 `m_images`)、声明在 views 之后所以先析构;`create()` 末尾重建(`:176-190`);`GetDepthFormat()`、`GetDepthImage()` |

设备事实:本机 `R8G8B8A8_SRGB` 没有 `STORAGE_IMAGE`。compute 生成 mip 要么换 UNORM、要么用 mutable view、要么走 blit。

环境坑:改了 `Fish.lib` 之后 MSBuild 可能只更新 exe 时间戳、不更新内容。判据是在 exe 里搜一段只有新代码才有的字符串;搜不到就是没链上,删 exe 重新构建。

### 阶段 1 · 管线 3D 状态(2026-10-30)

| 改动 | 结果 |
|---|---|
| `PipelineDesc` 去掉 cull / frontFace / polygonMode / lineWidth | 四项改成动态状态(`VulkanPipelineCache.cpp:151` 的列表) |
| 开 EDS3 的 `extendedDynamicState3PolygonMode` | `VulkanContext.cpp` 特性链和 `isDeviceSuitable` 两处对应 |
| 混合默认改不透明 | `DefaultOpaqueBlend()`(`VulkanPipelineCache.h:32`);`DefaultAlphaBlend()` 保留 |
| 录制期设值 | `setCullMode(eBack)`、`setFrontFace(eCounterClockwise)`、`setPolygonModeEXT(eFill)`、`setLineWidth(1.0f)`(`VulkanRendererAPI.cpp:507-510`) |

绕序的实测结论:`eClockwise` 配 `eBack` 截图全空。ortho 的 y 调换是在抵消 Vulkan NDC 的 y 向下,两次翻转抵消,屏幕上的绕序仍是逆时针,所以 `frontFace` 用 `eCounterClockwise`。

### 阶段 2 · 深度附件(2026-10-30)

| 改动 | 结果 |
|---|---|
| `PipelineDesc` 加 `colorFormat` / `depthFormat` | `==` 和 hash 同步(`VulkanPipelineCache.h:56-57`);`PipelineCache` 构造降到 3 参数,`Invalidate` 删除 |
| `RenderingInfo` 挂深度 | Clear / Store / 1.0(`VulkanRendererAPI.cpp:475-488`) |
| 深度布局转换 | `transitionSwapImage` 加 aspect 参数;组合格式必须 `eDepth | eStencil`(`03320`) |
| 深度状态 | 测试/写动态(`:511-512`),比较静态 `eLessOrEqual`;`pDepthStencilState` 非空 |

验证:构建 0 错误 0 警告;12 秒约 850 fps;验证层 0;resize 1600×900 → 1268×861、最小化/还原、`WM_CLOSE` exit=0。画面和改之前一致(现有几何全在 z=0,`eLessOrEqual` 保持画家顺序)。

### 阶段 3 · #3 · y 翻转落点(2026-10-31)

| 改动 | 结果 |
|---|---|
| `OrthographicCamera.cpp` 加 `k_FlipY = glm::scale(mat4(1), vec3(1,-1,1))` | 投影和 view 回到标准 GL 约定 |
| 构造 / `SetProjection` / `RecalculateViewMatrix` 三处 VP 都改成 `k_FlipY * P * V` | 三条路径一致:构造、resize/滚轮、相机移动 |
| `glm::ortho` 的 y 参数从 `(pos_y, neg_y)` 改回 `(neg_y, pos_y)` | 参数顺序不再承担翻转 |

验证:构建 0 错误(该 TU 0 警告);12 秒 400 精灵约 460 fps、stderr 0、验证层 0;程序化 resize 1066×600 → 1226×720(**断言过尺寸确实变了**)、最小化/还原、`WM_CLOSE` exit=0、stderr 0。

没做:`eBack` vs `eNone` 的 A/B —— `frontFace` 和剔除模式都没动,链路里 y 取反次数没变。

顺带:MSBuild 在 `Fish.lib` 更新后**没有重新链接 `Playground.exe`,连时间戳都没更新**;删掉 exe 再构建才链上。是阶段 0 记的那个坑,程度更重。

### 阶段 3 · #1 相机抽象 + #2 深度约定(2026-10-31)

| 改动 | 结果 |
|---|---|
| 新建 `Renderer/Camera.h` | 基类只存三个矩阵 + position;VP 由每台相机自己算 |
| `OrthographicCamera` 改成派生 | 行为不变,`k_FlipY` 留在自己的 .cpp |
| 新建 `PerspectiveCamera` | 两个构造:yaw/pitch、target/up;`SetYawPitch` / `SetProjection` / `SetPosition` |
| 新建 `ShadowCamera` | `lookAt` + 正交,**不乘 `k_FlipY`**(不经过屏幕) |
| `Renderer::BeginScene` → `AddCamera` | `SceneData` 存 `unordered_map<string, Camera*>`;整张表替换,不是累加 |
| `Submit` / `BatchingSubmit` 加相机键 | 每次提交按名字查表 |
| `CMakeLists.txt` 加 `GLM_FORCE_DEPTH_ZERO_TO_ONE`(PUBLIC) | `glm::perspective` 的 z 从 [-1,1] 变 [0,1];`glm::ortho` 的 `z=0` 从 ndc 0 变 0.5 |

验证:构建 0 错误;12 秒约 400 fps、stderr 0、验证层 0;resize 1066×600 → 1226×720(**断言过尺寸确实变了**)、最小化/还原、`WM_CLOSE` exit=0。**画面未目视。**

---

## 已定设计决定(不再讨论)

| 决定 | 内容 |
|---|---|
| 类型分派 | 两个 desc 构造函数重载,desc 类型就是标记 |
| extent | 引擎自己的 `ImageExtent`,不暴露 `vk::Extent2D` |
| 默认值 | `mipLevels = 1`、`layerCount = 1`,format 必须显式写 |
| 格式枚举 | 两个枚举,不暴露 `vk::Format` |
| 上传 | `Image` 只建空图,像素上传留在 loader |
| mip | desc 填层数;静态导入和运行期生成都未实现 |
| storage | 贴图 usage 按格式特性加 |
| 阴影采样器 | shadowmap 自带,不改 `texture::createSampler` |
| 管线键 | 附件格式进 `PipelineDesc`;离屏目标和交换链会同时存在 |
| 深度状态 | 测试/写动态;比较静态 `eLessOrEqual` |
| 深度存储 | `storeOp = Store`,给以后的 Hi-Z 留 |
| y 翻转 | 相机侧,乘进 VP 合成(`k_FlipY`);投影和 view 保持标准 GL 约定 |
| 相机抽象 | 一个 `Camera` 基类,只存矩阵;y 取反不进基类,屏幕相机各自乘 |
| 相机存储 | `SceneData` 存 `unordered_map<string, Camera*>`;`AddCamera` 是整表替换,不是累加 |
| 阴影相机 | `ShadowCamera` 不乘 y 取反;绕序和屏幕相机相反,阴影 pass 要单独定 frontFace / cull |
| 深度约定 | `GLM_FORCE_DEPTH_ZERO_TO_ONE`,PUBLIC |
| 相机控制 | 照搬 UE 编辑器飞行相机的非物理分支:WASD + 右键锁光标,Space/Ctrl 上下,C/Z 改 FOV;速度写死 |

---

## 阶段 3 · 相机 —— 代码完成,遗留见「欠账」

| # | 事项 | 现状坐标 |
|---|---|---|
| 1 | ~~相机抽象~~ ✅ **已完成 2026-10-31** | `Camera` 基类 + 两个屏幕相机 + `ShadowCamera`;`AddCamera` + 提交带相机键 |
| 2 | ~~透视投影的深度约定~~ ✅ **已完成 2026-10-31** | `GLM_FORCE_DEPTH_ZERO_TO_ONE`(PUBLIC);ortho near/far 仍是 -1/1 |
| 3 | ~~y 翻转落点~~ ✅ **已完成 2026-10-31** | 从 ortho 参数挪到 VP 合成(`k_FlipY`);`frontFace` 不变 |
| 4 | ~~相机控制~~ ⚠️ **代码完成 2026-10-31,未接线** | 新增 `PerspectiveCameraController`(照搬 UE 编辑器飞行相机的非物理分支)+ `Window::SetCursorLocked`;没有 layer 用它,运行期没跑过 |

## 阶段 4 · 几何 —— 阻塞(等阶段 3)

| # | 事项 | 现状 |
|---|---|---|
| 1 | 网格数据来源 | 无加载器;vendor 只有 spdlog/GLFW/glm/imgui/stb_image |
| 2 | 顶点加法线 | per-item 顶点 pos3 + uv2,stride 20;`texture.slang:26-29` 只有两个输入;offset 由前缀和推(`VulkanReflection.cpp:152-161`),shader 声明顺序必须等于内存顺序 |
| 3 | 索引位宽 | 只收 `uint32_t*`(`Renderer.h:27`),`drawIndexed` 写死 `eUint32`(`VulkanRendererAPI.cpp:585`) |
| 4 | 批的顶点格式 | `BatchVertex` 48 字节、7 条 `static_assert`(`DrawBatch.h:21-41`);批是 2D 精灵,法线可以先不加 |

## 阶段 5 · 光照 —— 阻塞(等阶段 4)

| # | 事项 | 现状 |
|---|---|---|
| 1 | 法线矩阵 | `ObjectUbo` 只有 model/color/texId/samplerId(`VulkanFrameData.h:18-24`);加字段要同步 `texture.slang:18-22`、`flat_color.slang` 和三处 `offsetof`(`VulkanFrameData.h:26-29`) |
| 2 | 光照 shader | 现在所有 shader 都直接输出颜色/贴图;`flat_color` 零调用者 |

## 阶段 6 · 多相机与阴影 —— 阻塞(等阶段 3/4)

| # | 事项 | 现状 |
|---|---|---|
| 1 | 批多相机 | 每个 shape 一个 `BatchState`,每次提交覆盖 `viewProjection`(`VulkanRendererAPI.cpp:225`),整批一次 pushConstants(`:399`);`DrawBatch.h:43` 注释说"会报",实际没有断言。**2026-10-31:`Submit` / `BatchingSubmit` 已加相机键,但批的 VP 仍是每 shape 一份 —— 加键没解决这条,等加 pass 那轮一起改。** |
| 2 | 阴影 pass | 深度图和 `ShadowMapDesc` 已有,缺光源相机、阴影 shader、比较采样器(`compareEnable` 现在是 false,`VulkanTexture.cpp:70`) |
| 3 | 新 descriptor binding | layout 一旦交出去就冻结(`VulkanDescriptorAllocator.cpp:26-29`),阴影 shader 必须在首帧前建 |
| 4 | 阴影管线 | color format `Undefined`、depth format 是阴影图格式,`PipelineDesc` 的格式字段会自然分出另一条管线,不用额外改 |

## 阶段 7 · 画质与性能 —— 可延后

| # | 事项 | 现状 |
|---|---|---|
| 1 | mip 导入 | 需要 DDS/KTX2;`copyFrom` 只写第 0 层(`VulkanImage.h:31`) |
| 2 | mip 生成 | blit 或 compute;本机 sRGB 无 storage |
| 3 | 批的顶点上传 | 每帧全量 memcpy(`VulkanRendererAPI.cpp:384`) |
| 4 | 深度图按层指定 | `VulkanImage.h:27` 的 TODO,等阴影 pass |

## 收尾

- 每轮画面必须目视;程序化 resize 不等于真人拖拽。
- 退出路径用 `WM_CLOSE`,不能用 `taskkill /F`。

## 欠账(每轮收尾更新)

- 深度图已经挂到每帧渲染,但没有任何 shader 采样它(阴影 / Hi-Z 未做)。
- `ShadowMapDesc` 的 `layerCount > 1` 没跑过。
- `mipLevels > 1` 没跑过,上传只写第 0 层。
- `Image::transitionLayout` 只支持贴图上传那两条转换。
- 画面只靠单帧截图确认过,动画和交互没测。
- 阶段 3 #3 改完之后,画面没有真人目视(只验到稳态和边界事件)。
- 阶段 3 #3 的 `eBack` vs `eNone` A/B 没跑。
- `PerspectiveCamera` / `ShadowCamera` 没有被任何代码实例化,运行期没出过帧。
- 相机键未命中在 Release 下没有兜底(`FS_CORE_ASSERT` 被编译掉)。
- 阶段 3 #1 改完之后的画面同样没有真人目视。
- `PerspectiveCameraController` 没有接线:光标锁定、鼠标看、移动、FOV 缩放全都没有在运行期跑过。
- 阶段 3 的所有改动只在正交那条老路径上验过(它仍走 `OrthographicCamera`)。

## 旧层号对照

| 原层 | 现在 |
|---|---|
| L0 | 已完成 · 阶段 0 |
| L1 | 已完成 · 阶段 1 + 阶段 2 的管线部分 |
| L2 | 已完成 · 阶段 2 |
| L3 | 拆到阶段 3(相机接口)、阶段 5(法线)、阶段 6(多相机、阴影) |
| L4 | 阶段 4(网格、顶点、索引)+ 阶段 7(mip、上传) |
| L5 | 阶段 3 |
| L6 | 收尾 |

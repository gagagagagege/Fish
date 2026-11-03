# 3D 迁移 TODO

> 建立于 2026-10-28,2026-10-30 按依赖重排,2026-11-01 阶段 4 设计定稿,2026-11-02 阶段 5 完成,2026-11-03 阶段 6 完成。
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
| 网格加载器 | Assimp 走 git submodule + `add_subdirectory`,只编 GLTF/OBJ;loader 放 `Renderer/MeshImporter`,`.h` 不暴露 `ai*` 类型 |
| 后处理开关 | `Triangulate`、`JoinIdenticalVertices`、`GenSmoothNormals`、`SortByPType`(移除点线)、`ImproveCacheLocality`;glTF/GLB 另加 `FlipUVs`,OBJ 不加 |
| 顶点格式 | `MeshVertex` = pos3 + uv2 + normal3,32 字节,offset 0/12/20;阶段 5 起 `lit.slang` 读 offset 20,2D 和批仍不读 |
| 几何存储 | 所有 mesh 进后端大 VB/IB;`Mesh` 只存 `firstVertex`/`vertexCount`/`firstIndex`/`indexCount`/局部 AABB;不存材质、名字、哈希 |
| 去重 | 在 loader 内完成:先按 `aiMesh` 下标复用,再内容哈希 + 逐字节比较;只覆盖一次导入;hash 不存进 Mesh |
| Mesh/Instance | Mesh 是几何;Instance = meshId + transform + color + textureSlot + samplerSlot + shader;tiling 暂不加 |
| 材质 | 归 Instance;Mesh 不存 `mMaterialIndex` |
| 提交 | `SceneInstance` 跨帧持久,每帧循环 `Submit(instance, camera)`;loader 返回 `SceneInstance`,收一个 shader 填进每个实例 |
| 遍历规则 | 节点矩阵连乘;每个 `aiMesh` 一个 Instance;蒙皮 mesh 跳过;缺 UV 填 (0,0);多套 UV 用第 0 套 |
| 扩容 | Mesh 表 64 条、VB 65536 顶点、IB 131072 索引起步,均 ×1.5;旧大 buffer 延迟 `k_MaxFramesInFlight` 帧销毁 |
| 光照空间 | 世界空间;N 由 normalMatrix 变换,L = -光传播方向 |
| 法线矩阵 | `ObjectUbo` 末尾 `alignas(16) mat4 normalMatrix`,offset 96,总 160B;`PushObjectUbo` 内算 inverse-transpose,支持任意缩放 |
| 光照参数 | push constant:viewProj 0–63、方向 64、颜色 80、环境光 96,共 112B;方向存传播方向,强度折进颜色 |
| Light 类型 | `Renderer/Light.h`,三个 `vec3`;`SceneData::CurrentLight` 跨帧保留;`Renderer::SetLight`;`RendererAPI::DrawFrame(items, light)` |
| 光照 shader | 新建 `lit.slang`(采样 + Lambert + 环境光);`texture.slang` / `flat_color.slang` / `batch.slang` 不改 |

---

## 阶段 3 · 相机 —— 代码完成,遗留见「欠账」

| # | 事项 | 现状坐标 |
|---|---|---|
| 1 | ~~相机抽象~~ ✅ **已完成 2026-10-31** | `Camera` 基类 + 两个屏幕相机 + `ShadowCamera`;`AddCamera` + `SetPassCamera` |
| 2 | ~~透视投影的深度约定~~ ✅ **已完成 2026-10-31** | `GLM_FORCE_DEPTH_ZERO_TO_ONE`(PUBLIC);ortho near/far 仍是 -1/1 |
| 3 | ~~y 翻转落点~~ ✅ **已完成 2026-10-31** | 从 ortho 参数挪到 VP 合成(`k_FlipY`);`frontFace` 不变 |
| 4 | ~~相机控制~~ ✅ **已完成并接线 2026-11-03** | `PerspectiveCameraController` 由 `Layer3D` 实例化;右键锁光标、WASD、Space/Ctrl、C/Z 改 FOV 跑过。第一次运行撞到两个 bug,见下 |

## 阶段 4 · 几何 —— 可做(第一版跑通,2026-11-01)

前置:阶段 3 #1–#3 已完成;阶段 3 #4(相机控制器接线)和几何无关,不阻塞。

| # | 事项 | 状态 |
|---|---|---|
| 1 | 网格数据来源 | Assimp v6.0.5(子模块)+ `Renderer/MeshImporter`;`cube.obj` 已跑通 |
| 2 | 顶点加法线 | `MeshVertex` pos3 + uv2 + normal3 = 32B;阶段 5 起 `lit.slang` 读取 |
| 3 | 索引位宽 | 保持 uint32 |
| 4 | 批的顶点格式 | 保持 `BatchVertex` 48 字节,不动 |

环境备注:

- Assimp 子模块锁在 `v6.0.5`;它会把 `CMAKE_DEBUG_POSTFIX=d` 写进全局 cache,`CMakeLists.txt` 在 `add_subdirectory` 之后强制清空,否则 `Fish.lib` 会改名 `Fishd.lib`。
- Assimp 6 的 `aiString` 取 C 字符串是 `C_Str()`,不是 `CStr()`。

## 阶段 5 · 光照 —— 已完成(2026-11-02)

| # | 事项 | 结果 |
|---|---|---|
| 1 | 法线矩阵 | `ObjectUbo` 追加 `alignas(16) mat4 normalMatrix`,offset 96,总 160B;`PushObjectUbo` 内 `inverseTranspose(mat3(model))`,每物体每帧一次。C++ 端必须写 `alignas`,否则默认对齐落在 88 |
| 2 | 光照 shader | 新建 `lit.slang`:pos+uv+normal,Lambert + 环境光 + 光色;立方体改用它 + 白贴图;2D 和批仍无光照 |
| 3 | 光源数据 | `PushConstants` 112B:viewProj 0–63、方向 64、颜色 80、环境光 96;光每帧推一次,viewProj 每项推一次 |
| 4 | 接口 | `Light.h`、`SceneData::CurrentLight`(跨帧保留)、`Renderer::SetLight`、`RendererAPI::DrawFrame(items, light)` |
| 5 | 验证 | 构建 0 错误 0 警告;15 秒 stderr 0;resize 1600×900 → 1384×761(断言过尺寸变化)、最小化/还原、`WM_CLOSE` exit=0;CPU 14.89s / 墙钟 15s |

已知错误与修正:

- `vkCmdPushConstants` 推光源时只给 `fragment`,layout 的 range 是 `vertex|fragment` → `VUID-vkCmdPushConstants-offset-01796`。改成两个 stage 后消失。
- 非均匀缩放的 A/B:同样 `scale(0.45, 0.225, 0.9)`,`mat3(model)` 与 inverse-transpose 两张截图立方体区域有 27,761 个像素不同,最大通道差 728。**轴对齐的面法线区分不了两种实现**;能区分的是 `GenSmoothNormals` 在共享顶点上生成的对角法线。

## 阶段 6 · 多相机与阴影 —— 已完成(2026-11-03)

| # | 事项 | 结果 |
|---|---|---|
| 1 | 相机提交改成 pass 级 | `AddCamera(Camera&)` 返回 `CameraId`,返回的是 `SceneData::Cameras` 的下标;`SetPassCamera(pass, id)`;`Submit` / `BatchingSubmit` 不再带相机。`RenderInstance`、`DrawBatch::viewProjection` 删除。`Renderer::DrawFrame` 每帧把 `Camera*` 数组解析成 `cameraVPs`,和 `passCameras` 一起交给后端 |
| 2 | 批多相机 | 批的 VP 改成 pass 级,每批 push 一次。同一 shape 用两个相机提交互相覆盖的问题消失 |
| 3 | 阴影 pass | 2048×2048 `D32_SFLOAT` 阴影图 + 比较采样器(`compareEnable=true`、`eLessOrEqual`、clamp-to-border)。`DrawFrame` 先录深度-only 的 light pass,barrier 后录主 pass |
| 4 | 新 descriptor binding | set 1 加 binding 2 `Texture2D<float> shadowMap`、binding 3 `SamplerComparisonState`;`lit.slang` 在构造期登记,`EnsureTextureTable` 写一次 |
| 5 | 阴影管线 | `PipelineCache` 支持 `colorFormat = eUndefined`:`colorAttachmentCount = 0`、`colorBlending.attachmentCount = 0`。新增 `shadow.slang`(顶点 + 无输出片元) |
| 6 | 主 pass 采样 | `lit.slang` 加 `lightViewProj`(offset 112)、世界坐标、`SampleCmpLevelZero`;`PushConstants` 112→176B |
| 7 | 测试场景 | `Playground` 构造期注册主相机和 `ShadowCamera`;加一块接收面;2D 精灵、三角形、接收面 `castsShadow=false`;光方向 `SetLight` 和阴影相机同向 |

已知错误与修正:

- `VulkanPipelineCache.cpp` 里 `hasColorAttachment` 一开始声明在 `colorBlending` 之后 → C2065;挪到 `GetOrCreate` 开头。
- `PushConstants` 的 `static_assert(sizeof <= 128)` 挡住 176B;改成 ≤256,并记录本机原生 4060 报 256、D3D12 翻译层报 128。
- 深度-only 管线不能传 `eUndefined` 作为唯一颜色附件格式 —— `colorAttachmentCount` 必须是 0。

验证:

- 构建 0 错误(该 TU 没有看到警告);8–10 秒运行 stderr 0 字节,验证层 0。
- resize 1600×900 → 1384×761(断言过客户区确实变了)、最小化/还原;`WM_CLOSE` exit=0、stderr 0。
- 光方向 A/B:`k_LightDirection.x` 从 +0.4 改成 -0.5 后,接收面上的阴影跟着移到左侧,再改回 +0.4。
- 截图确认阴影出现在接收面上;400 精灵约 1.30 ms/帧。
- **画面没有真人开窗口确认过,只有截图。**

## 3D layer(2026-11-03)

`Playground` 现在只推 `Layer3D`,`ExampleLayer` 和 `playground2D` 删除。类名不能以数字开头,所以 C++ 类叫 `Layer3D`,`Layer("3dLayer")` 是显示名。

场景:3 个 `cube.obj` 实例 + 一张 20×20 地面;透视相机默认在 `(0,0,3)` 看向 -z;地面只接收阴影,方块投影。

第一次运行 `PerspectiveCameraController` 撞到两个 bug:

- **成员初始化顺序**:`m_Camera` 声明在 `m_Position` 之前,构造函数用 `m_Position` 初始化 `m_Camera`,而成员按声明顺序初始化 —— 相机拿到未初始化的位置,VP 是 NaN,整屏只剩清屏色。把 `m_Position` / `m_Yaw` / `m_Pitch` 挪到 `m_Camera` 之前。
- **最小化 0×0**:`OnWindowResized` 用 0 宽/高算 aspect,`glm::perspective` 里 `assert(aspect != 0)` 直接终止进程。宽或高为 0 时直接返回。

验证:构建 0 错误;8–10 秒 stderr 0;resize 1600×900 → 1384×761、最小化/还原、`WM_CLOSE` exit=0;截图确认 3 个方块、地面和阴影。**真人开窗口和鼠标飞行没做。**

## 阶段 7 · 画质与性能 —— 可延后

| # | 事项 | 现状 |
|---|---|---|
| 1 | mip 导入 | 需要 DDS/KTX2;`copyFrom` 只写第 0 层(`VulkanImage.h:31`) |
| 2 | mip 生成 | blit 或 compute;本机 sRGB 无 storage |
| 3 | 批的顶点上传 | 每帧全量 memcpy(`VulkanRendererAPI.cpp:384`) |
| 4 | 深度图按层指定 | `VulkanImage.h:27` 的 TODO。阴影 pass 只用 layer 0,分层阴影(CSM)未做 |

## 收尾

- 每轮画面必须目视;程序化 resize 不等于真人拖拽。
- 退出路径用 `WM_CLOSE`,不能用 `taskkill /F`。

## 欠账(每轮收尾更新)

- 交换链深度图仍只做深度测试,没有 shader 采样它(Hi-Z 未做);阴影图已经由 `lit.slang` 采样。
- `ShadowMapDesc` 的 `layerCount > 1` 没跑过(阴影只用 1 层)。
- `mipLevels > 1` 没跑过,上传只写第 0 层。
- `Image::transitionLayout` 只支持贴图上传那两条转换。
- 画面只靠单帧截图确认过,动画和交互没测。
- 阶段 3 #3 改完之后,画面没有真人目视(只验到稳态和边界事件)。
- 阶段 3 #3 的 `eBack` vs `eNone` A/B 没跑。
- `ShadowCamera` / `PerspectiveCamera` 都已由 `Layer3D` 实例化并出帧;`PerspectiveCamera` 只在这一个场景上跑过。
- 相机按 pass 查表的兜底:`ResolvePassVP` 在 Release 下断言被编译掉,返回单位矩阵,画面会塌成 NDC;没有“跳过这个 pass”的路径。
- 阶段 3 #1 改完之后的画面同样没有真人目视。
- `PerspectiveCameraController` 已接线并出帧,但只跑了默认位姿:右键锁光标、鼠标看、WASD、Space/Ctrl、C/Z 改 FOV 都没有真人操作过。
- 阶段 3 的改动现在在正交(旧场景)和透视(`Layer3D`)两条路径上都出过帧。
- 阶段 4 第一版已落地(2026-11-01):`MeshVertex`/`Mesh`/`Instance`/`SceneInstance`、后端大 VB/IB + mesh 表、`CreateMesh`/`Submit`、Assimp v6.0.5 + `ImportScene`。原有 2D 几何改走 meshId,新增一个 `cube.obj` 测试导入。
- 导入只跑过一个没有 UV、没有法线、无骨骼的 OBJ;glTF 路径、缺 UV 分支、蒙皮跳过分支、跨 mesh 内容去重都没在运行期触发过。
- 大 buffer 的 1.5 倍扩容 + 旧 buffer 延迟销毁分支没在运行期触发过:当前只有 3 个 mesh,远低于初始容量。
- `DrawItem.h` 已无调用者但还没删;`CreateVertexBuffer`/`CreateIndexBuffer` 也没有调用者了。
- 法线 offset 20 只由 `lit.slang` 读取;`texture.slang` 和 `batch.slang` 仍不读。
- 光照只有一个方向光:Lambert + 环境光 + 一张阴影图,没有高光、多光源、衰减;批路径的 `BatchVertex` 48B 没有法线,仍无光照、不投影。
- `Renderer::SetLight` 现在由 `Playground` 构造期调一次;没有 ImGui 调方向的入口,运行期改 Light 不会同步 `ShadowCamera`(`ShadowCamera` 没有 `SetTarget`)。
- 法线矩阵每物体每帧在 CPU 求逆,没有静态物体缓存;per-item 循环里的 2D 物体也算了。
- 光照现在在透视相机(`Layer3D`)下出帧;旧的正交场景已删,不再复测。
- 画面只由截图确认(立方体受光、A/B 有差异),真人目视和交互未做。
- 立方体的明暗渐变来自 `GenSmoothNormals` 在 8 个共享顶点上平均法线,不是逐面法线;要逐面平直需要导入时不合并顶点或拆法线。
- `flat_color.slang` 现在真的零调用者了,仍保留;`lit.spv` 是手工跑 `slangc` 生成的。
- A/B 只证明两条法线路径不同,没有和逐像素参考值比对。
- 导入只支持静态几何:蒙皮 mesh 跳过,不导入材质/贴图,`textureSlot` 留 0。
- 大 buffer 的扩容 + 旧 buffer 延迟销毁只有设计,没跑过。
- 跨导入去重不做;相同几何在不同文件里各占一份。
- 阴影图固定 2048×2048、只有一张;`ShadowCamera` 的位置和 halfSize 写死在 `Playground` 构造期,没有按场景 AABB 拟合。
- 阴影比较采样器用最近邻、单 tap,没有 PCF;深度偏移是 shader 里固定的 0.002,没有 slope-scaled bias。
- 阴影 pass 剔正面 + `frontFace = eClockwise`;开放网格(单面)不会投影,接收面 `castsShadow=false` 是靠 flag 排除的。
- `PushConstants` 现在是 176B,要求 `maxPushConstantsSize >= 176`。本机原生 4060 报 256,D3D12 翻译层设备只报 128,那类设备上这个结构用不了。
- `AddCamera` 只追加、不回收 id;每帧调用会让数组和 id 无限增长。`Playground` 已改成构造期调一次,`playground2D` 不再注册自己的相机(主 pass 用 `ExampleLayer` 的相机,两个控制器仍同步更新)。
- 阴影 UV 和绕序只通过“改光方向 → 阴影跟着移动”间接验证,没有和参考图逐像素比对。
- 阴影 pass 和主 pass 各 `PushObjectUbo` 一遍,同一物体的 `normalMatrix` 每帧被算两次;UBO 容量按 `items.size() * 2` 预留。
- 帧时约 1.30 ms(400 精灵),上一轮记录是 1.15–1.20;场景多了阴影 pass 和接收面,不是干净 A/B。
- `Layer3D` 替换掉两个旧 layer 之后,`BatchingSubmit` / 批处理那条路在应用层零调用者;`batch.slang`、`CreateVertexBuffer`、`CreateIndexBuffer`、`flat_color.slang` 都保留但运行期不再触发。
- `Layer3D` 的地面是 20×20 的单 quad,场景内容写死(3 个 cube);透视相机默认 `(0,0,3)`、俯仰 0,地面在视野下半部。

## 旧层号对照

| 原层 | 现在 |
|---|---|
| L0 | 已完成 · 阶段 0 |
| L1 | 已完成 · 阶段 1 + 阶段 2 的管线部分 |
| L2 | 已完成 · 阶段 2 |
| L3 | 阶段 3(相机接口)、阶段 5(法线)、阶段 6(多相机、阴影)已完成 |
| L4 | 阶段 4(网格、顶点、索引)+ 阶段 7(mip、上传) |
| L5 | 阶段 3 |
| L6 | 收尾 |

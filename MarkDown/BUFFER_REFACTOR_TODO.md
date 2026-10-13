# Buffer 重构 —— 完成记录

> 更新于 2026-09-15。对应 `PLAN.md` 的 M1（Vulkan 侧重构：Buffer / Image / FrameData）。
>
> **状态：全部完成。** 编译 0 错误 0 警告，程序渲染正常，机器环境问题也已解决。
> 剩下的只有第五节的既有遗留项（不阻塞）。

---

## 一、已完成

| 项 | 说明 |
|---|---|
| `Device` → `vkContext` | 类名全量改名。文件名仍是 `PhysicalDevice.h`，暂未改 |
| `buffer` → `Buffer` | 类名首字母大写。文件仍是 `buffer.h` / `buffer.cpp`，暂未改 |
| `Vertex` 迁入 `buffer.h` | 及其两个布局函数（带 TODO，见下） |
| 旧 `vertexBuffer` 类下线 | `src/vertexBuffer.h`、`src/vertexBuffer.cpp` 已删除 |
| `uniformBuffer` 拆出继承 | 不再是 `: public Buffer`，六个函数保持全 `static` |
| `texture` 迁移 | `createTextureImage` 改收 `vkContext*`，去掉 device/physicalDevice/graphicsQueue |
| include 修正 | `commandPool.h`、`pipeline.h` → `"buffer.h"`；`TriangleApp.h` 里的删掉 |

**最终的 `Buffer` 接口：**

```cpp
class Buffer
{
public:
    Buffer() = default;                       // 空状态:句柄 null,析构安全
    Buffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
           const vk::MemoryPropertyFlags& properties, vkContext* context);
    ~Buffer() { unmap(); }

    void copyBuffer(const vk::raii::Buffer& srcBuffer, vk::raii::CommandPool& transientPool);

    static Buffer createVertexBuffer(const std::vector<Vertex>& vertices,
                                     vkContext* context, vk::raii::CommandPool& transientPool);
    static Buffer createIndexBuffer(const std::vector<uint16_t>& indices,
                                    vkContext* context, vk::raii::CommandPool& transientPool);

    static uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties,
                                   vk::raii::PhysicalDevice& physicalDevice);

    vk::raii::Buffer&       getHandle() { return m_buffer; }
    vk::raii::DeviceMemory& getMemory() { return m_memory; }
    vk::DeviceSize          getSize()   { return m_size; }

    void* map(vk::DeviceSize offset, vk::DeviceSize size);
    void  unmap();
    // 移动语义:拷贝已删,仅可移动
};

// TriangleApp 成员
Buffer vertexBuffer;
Buffer indexBuffer;
std::vector<Buffer> uniformBuffers;   // MAX_FRAMES_IN_FLIGHT 个
```

---

## 二、关键设计决定（不要重新推导）

1. **构造函数收 `vkContext*`（指针），不是引用。**
   引用成员会隐式删除拷贝/移动赋值运算符，而 `TriangleApp` 需要
   `vertexBuffer = Buffer::createVertexBuffer(...)` 这种赋值。

2. **`Buffer() = default;` 是故意加的。**
   提供「空状态」（句柄 null、`m_context` null、析构安全），
   好让 `TriangleApp.h` 能写 `Buffer vertexBuffer;` 这种「先声明、后在 `initVulkan()` 里赋值」的写法。
   和这个文件里原本 `vk::raii::Buffer x = nullptr;` 的风格一致。

3. **工厂签名砍掉的参数是有意的。**
   `size` 由 `数据条数 × sizeof(元素)` 算出；`usage` 由函数语义固定
   （顶点 = `eVertexBuffer`，索引 = `eIndexBuffer`）；`properties` 固定；
   `device` / `physicalDevice` / `graphicsQueue` 全从 `context` 取。
   **只留 `transientPool`** —— 命令池还在 `TriangleApp` 手里，没进 `vkContext`。

4. **`findMemoryType` 保持裸 `vk::raii::PhysicalDevice&` 入参。**
   它只依赖物理设备、不依赖 context，且 `texture.cpp` 也在用。

5. **`uniformBuffer` 不继承 `Buffer`。**
   六个函数全是 `static`，基类状态一个没用上，继承是装饰性的。

6. **`uniformBuffersMapped`（`std::vector<void*>`）已删除。**
   `Buffer::map()` 内部缓存 `m_mapPtr`，重复调用返回同一地址，
   不需要外部再存一份。每帧直接 `uniformBuffers[i].map(0, sizeof(ubo))`。

7. **`getBindingDescription` / `getAttributeDescriptions` 是临时寄放在 `Vertex` 上的。**
   它们描述的是渲染侧的顶点布局，不属于通用 Buffer 的职责。
   已在 `buffer.h:18-19` 和 `buffer.cpp:10` 标注 TODO，
   等 `Vertex` / mesh 独立成头文件后再迁走。

---

## 三、✅ 已解决：机器 Vulkan 环境故障（**与代码无关**）

> **已于 2026-09-15 修复**：更新 AMD 核显驱动后，`vulkaninfoSDK.exe` 12/12 稳定，
> 本工程 8/8 稳定，不再需要任何环境变量。下面是当时的排查过程，留作记录。

### 现象

程序约 **50% 概率**在 `vkCreateInstance` 里段错误。

用 `std::cerr` 探针定位到确切位置（探针已拆除）：

```
[TRACE] initVulkan begin
[TRACE]   about to create Instance     ← 之后无输出
```

### 定性：不是你的代码

**SDK 自带的 `vulkaninfoSDK.exe` 同样崩溃**，10 次里崩 7 次：

```
0 139 139 0 139 139 139 139 0 139
```

一个和本工程毫无关系的官方工具也会崩，说明问题在系统层面。

### 根因：AMD 的 Vulkan 驱动 ICD 损坏

逐个加载 ICD 测试（每组 10 次）：

| 配置 | 正常率 |
|---|---|
| 只加载 NVIDIA ICD | **10/10** ✅ |
| 只加载 AMD ICD | **0/10** ❌ |
| 两个都加载（默认） | 8/10 |

**强制走 NVIDIA 之后，你的程序 6/6 全部正常运行。**

> 顺带排除：三个隐式层（`VK_LAYER_NV_present` / `VK_LAYER_AMD_switchable_graphics` /
> `VK_LAYER_NV_optimus`）逐个禁用后成功率无变化，**不是层的问题**，是 ICD 本身。

### 处理方式（已执行）

从 AMD 官网下载并安装核显驱动，**分类要选「处理器搭载 Radeon 显卡」而不是「Radeon 显卡」**
（780M 是核显，长在 CPU 里）。

| | 修复前 | 修复后 |
|---|---|---|
| 版本 | `31.0.22032.1` | `32.0.31041.1004` |
| 日期 | 2023-12-06 | 2026-08-18 |

**验证结果：** `vulkaninfoSDK.exe` 12/12 正常，本工程 8/8 正常，无需环境变量。

> 备选方案（已不需要）：设 `VK_ICD_FILENAMES` 指向 NVIDIA 的 json 强制走独显。
> 缺点是路径带驱动哈希，N 卡驱动一更新就失效。

### 为什么 N 卡程序会被 A 卡驱动搞崩（供回顾）

`vkCreateInstance` 的语义是**枚举系统上所有 Vulkan 驱动**，不是选一个。
loader（`vulkan-1.dll`）会把**所有已注册 ICD 的 DLL 全部加载进来**并逐个调用。

```
你的程序 → vulkan-1.dll (loader) ┬→ NVIDIA ICD (nvoglv64.dll)
                                 └→ AMD ICD    (amdvlk64.dll)
```

所以哪怕全程只用 N 卡，AMD 驱动的 DLL 照样被加载。它崩在这一步，程序就崩 ——
崩溃发生在 loader 枚举阶段，那时还没轮到应用自己的代码。

这台机器是 8845H（含 Radeon 780M 核显）+ RTX 4060 的混合显卡配置，
所以机器上确实有一块 A 卡，只是不叫「显卡」，藏在 CPU 里。

---

## 四、验证结果

编译：**0 错误 0 警告**，链接通过，产出 `build/Debug/VulkanProject.exe`。

运行（强制 NVIDIA ICD）：10 秒渲染 **35519 帧**（≈3550 FPS，无垂直同步），
`frameIndex` 在 0/1 之间正确轮转，各资源数量正确
（`fence=2 cmd=2 ubo=2 ds=2`）。

**渲染这条线是通的。**

---

## 五、遗留的既有问题（非本次引入，不阻塞）—— 全部已解决

1. **present 信号量复用警告** —— ✅ 已修。改成每个 swapchain image 一份
   （`SwapChain::createRenderFinishedSemaphores`，基数 = `imageCount` 而不是 `MAX_FRAMES_IN_FLIGHT`）。

2. **`createDescriptorSet` 顺带绑了贴图** —— ✅ 已修。原函数是
   `uniformBuffer::createDescriptorSet`，现在整体变成 `DescriptorAllocator`，
   "交付写完的 set" 就是这个类的本职，贴图出现在签名里不再越界。

3. **`uniformBuffer.h` 里 `UniformBufferObject ubo;` 成员是死的** —— ✅ 已消失，该类现在没有成员变量。

4. **`const int& MAX_FRAMES_IN_FLIGHT` 按 const 引用传 int** —— ✅ 已消失，
   现在是 `TriangleApp.h` 的成员常量，没有按引用传的地方。

---

## 六、可选的收尾清理

- `TriangleApp.h` 的注释「缓冲与内存：内存须晚于缓冲销毁，故先声明内存」已过时 ——
  内存现在归 `Buffer` 自己持有
- 文件名 `buffer.h` / `buffer.cpp` → `Buffer.h` / `Buffer.cpp`（牵动 4 处 include）
- 文件名 `PhysicalDevice.h` → `vkContext.h`（牵动 3 处 include）

---

## 七、验证命令

```bash
cd /d/vulkan_project
cmake -S . -B build          # 增删源文件后必须重跑:CMakeLists 用的是 src/*.cpp glob
cmake --build build --config Debug
```

运行（驱动已修好，直接跑）：

```bash
cd build/Debug
./VulkanProject.exe
```

> 注意工作目录必须是 `build/Debug` —— 程序会去读 `textures/texture.jpg` 和 `shaders/slang.spv`，
> CMake 在构建时会把它们拷到这个目录。

> `CMakeLists.txt` 用的是 `file(GLOB ...)`。删掉 `vertexBuffer.cpp` 之后不重跑 configure，
> 项目文件里还会引用它，会出现 `C1083: 无法打开源文件` 这种误导性报错。

---

## 八、两个坑（避免重蹈）

1. **编辑器没保存时，读到的文件是磁盘上的旧版本。** 曾因此基于过期代码做了一轮错误的 review。改完记得 Ctrl+S。

2. **改名 `buffer` → `Buffer` 时，`\bbuffer\b` 这种词边界搜索会漏掉 `buffer::Xxx` 的限定用法。**
   实际漏了 `texture.cpp:64` 的 `buffer::findMemoryType`。类名改完后跑一次全量复查：
   ```bash
   grep -rnE "\bbuffer\b" --include=*.h --include=*.cpp src/
   ```

3. **排查这条崩溃时的教训：单次采样会骗人。**
   一开始用 1 次采样得出「AMD 隐式层是元凶」的结论，8 次采样后就被推翻了 ——
   真实原因是 AMD 的 ICD，两者完全无关。**间歇性问题必须多采样，至少 10 次再看比例。**

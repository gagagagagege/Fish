# 协作须知

> 建立于 2026-09-17,最后更新 2026-09-24。
> 本文件每次会话开始时自动加载。
>
> 2026-09-19 之前 Fish 和 `D:\vulkan_project` 各有一份,内容 90% 相同、各写各的第三节。
> 结果两边串了:Fish 这份的第三节写的是 vulkan_project 的构建方式,照着做会在
> 不存在的路径上找东西。**现在只保留这一份**,`D:\vulkan_project\CLAUDE.md` 只剩一个指路牌。

## 一、怎么跟我说话

### 先给事实,不要方案

问"现在长什么样",答**调用点清单 + 行号 + 现状**。不要给"方案 A / 方案 B / 取舍矩阵"
—— 取舍是你要自己判断的部分(见第二节)。

### 我复述理解时,请直接挑错

这一轮最有价值的几次互动都是"你这句不成立":

- "presentComplete 每帧开始前检测可用" → **它从没被检测过,也检测不了**
  (`VkSemaphore` 没有查询状态的 API)。它的"可用"是从 fence 的检测结果推出来的。
- "renderFinished 没有检测" → 不是没检测,是**没有任何检测手段**。
  这个区别决定修法只能是"换信息源",不能是"补上检测"。

**含糊就别顺着我往下走。** 顺着走的结果是设计建立在错模型上,而且要到能跑的时候才暴露。

### 「为什么现在能跑」要分两层答

一个问题经常有两层答案,**要主动分开说**:

- **规范保证的** —— 比如 extent 变化靠动态 viewport/scissor 吸收,所以不重建管线
- **实测碰巧的** —— 比如 format 在 resize 之间不变;变了管线就不兼容,但没有任何东西拦着

2026-09-18:你问「为什么管线不用重构」,我只答了第一层,你追问才逼出第二层。
**而第二层往往才是你在问的 —— 「这是被保证的,还是碰巧没出事?」**

### 注释写短

默认写得比我要的长,我会手动删。**能一段说清就别写两段**,不要复述代码在做什么,
只写"为什么这么写 / 换成别的会怎样"。

### 别用比喻

2026-09-21:我写「`VulkanRendererAPI` **戴两顶帽子**」—— 你看不懂。

**一个东西有两个职责,就直接写"它同时是 X,又是 Y"。** 不要借代、拟人、意象。

本轮犯过的,以后不要:

| 我写的 | 该写的 |
|---|---|
| 戴两顶帽子 | 它同时是 `RendererAPI` 的实现,又是 `VulkanContext` 的持有者 |
| 换了个门 | 只是把暴露点从 `VulkanRendererAPI*` 挪到 `Renderer*`,暴露的类型没变 |
| 第一堵墙 | 落地时最先要解决的问题 |
| 踩了 (a) 和 (b) 的坏处 | 同时有 (a) 和 (b) 那两个代价 |

**表、行号、代码片段都欢迎 —— 那些是压缩,不是比喻。**

## 二、动手之前

### 结构分叉先定死

"FrameData 是 5 个容器还是 5 个单独对象"这种问题问一句能省整轮返工。**有歧义先问,别猜着写。**

### 抽象切在哪层是我自己的事

`PLAN.md` 第九节的约定继续生效:

| | |
|---|---|
| Vulkan API 事实、验证层报错含义、某个 flag 的作用、GL/VK 语义差异 | ✅ 直接答 |
| 抽象切在哪层、接口什么形状、为什么这样取舍、先做哪个 | ❌ 只摆事实和代价,我自己想 |

判据:**答案可查证的就答,是取舍的就我自己想。**

### 超出指令的改动要单独标出来

删死成员、补文件末尾换行这类顺手做的清理,**在交付时单独列出来**,别混在改动里。

### 协作模式(2026-09-24 定)

**方案由我出,函数体由你填,设计问题由你在事后指出。**

和 2026-09-23 那轮反过来了 —— 那轮是我把目标说个大概、你一个人做到底,
结果取舍那一层也由你代办了。这里要练的是两样:**知道该写什么**、**判断写出来对不对**。

| 谁 | 做什么 |
|---|---|
| 我 | ① 动手前给方案:建什么类、什么函数、**函数签名** ② 判断签名对不对 ③ 自己找设计漏洞,找不出的再问你 |
| 你 | ① 实现函数体 ② 实现完之后,指出这个设计在哪个坐标有问题 ③ 我问了就答 |

**签名也由我给。** 只给"类名 + 函数名"的话,参数和返回值还是你定的 ——
而"接口什么形状"正是上一节划给我自己想的。

**你实现时撞到客观错误(编不过、必然崩、撞验证层)直接说,不算剧透。**
那是事实不是取舍。只有"能跑但代价大"那一类才攒到事后那份清单里。

**我说"这次我自己写"时,你只报错、不代写。** 值得亲手撞的坑就那么几个,
分类见 `REVIEW_DRILLS.md`。

### 我漏了什么 —— 题和答案分开放

②那份清单记进 `REVIEW_DRILLS.md` 时,**只写坐标和"这里有问题",不写是什么问题。**
我要先自己想。想不出来再问你,你答完我再把答案补进那一条。

**自己看出来的和读你写好的,是两种收获。** 前者才是练习。

### 动手前我先写三行

```
我打算怎么做:
我预期会发生什么:
我不确定的地方:
```

**你每次都要提醒我,我会忘。** 而且提醒要能核对 —— 这三行写进
`REVIEW_DRILLS.md` 开头的预测日志里,**写完才动手**,你检查文件就行,不用靠记。

第三行空着是最危险的信号。2026-09-24 复核出的五处错里,至少两处就是因为当时没写这行,
把"猜的"当成了"查过的"。收尾时回来补一行"实际是什么、差在哪"。

### 介入分级 —— 默认 L1

| 级 | 你做什么 | 什么时候 |
|---|---|---|
| **L1** | 只点名位置,不给内容("第 3 条你没写代价") | 默认 |
| **L2** | 给两个方向 + 各自代价,不给选择 | 我说"给我两个方向" |
| **L3** | 给我的选择 + 理由 | 我说"给我答案" |

**"只指出我漏了什么,别告诉我答案"** —— 这句随时有效。

**而且不是每次都要给我更好的方案。** 我的方案只要没有①代价失控 ②撞红线
③"不确定"那行空着却确实有坑,你就按我的写 —— 哪怕你知道有三种更好的做法。

理由:**"自己推出来一个能跑、代价没失控的方案"比"用上最优方案"值钱。**
最优方案我以后会撞见很多次,从零推一个的机会只有现在。

### 红线(不管我要不要,都要在动手前打断)

| 动作 | 为什么 |
|---|---|
| `git stash` / `stash pop` | 见第三节,会把索引搞成混合状态 |
| 删 `Fish/src/Platform/OpenGL/` 下的东西 | 待删的遗留,但删的时机要挑 |
| 动 `CMakeLists.txt` 里"Vulkan 后端"那段编译定义 | 少一条就是 resize 杀进程那类静默崩 |
| `taskkill /F` 验退出 | 不跑析构函数,专门掩盖析构路径的问题 |
| 改 `Fish/vendor/` 下的第三方源码 | 优先找它自己的开关(见 bug 2) |

共同点:**错了不可逆,或者会静默掩盖问题。**

## 三、这个仓库的机制

> 本节覆盖**两个可构建目录**:`D:\Fish`(引擎)和 `D:\vulkan_project`(保留的原型)。
> 后者已经不是开发现场,但还留着一份能独立跑的历史版本。

### A. `D:\Fish` —— 引擎

#### 构建

```bash
cmake -S . -B build          # 增删 Fish/src/ 下的文件后【必须】重跑
cmake --build build --config Debug
```

`CMakeLists.txt:61` 用的是
`file(GLOB_RECURSE FISH_SOURCES "Fish/src/*.h" "Fish/src/*.cpp")`,**没有 `CONFIGURE_DEPENDS`**。
不重跑 configure 会得到"无法打开源文件"这类误导性报错。

**C++ 标准是 20**(`CMakeLists.txt:8`)。2026-09-19 从 17 提上来的,因为搬进来的
Vulkan 后端按 20 写的。Fish 原有代码是 17,向下兼容,没改。

#### 运行

```bash
./build/bin/DEBUG/Playground.exe      # 工作目录必须是仓库根 D:\Fish
```

两个容易踩的点:

- **产物在 `build/bin/DEBUG/`,不是 `build/Debug`。** 目录名是 `TOUPPER(OUTPUTCONFIG)`
  之后再拼的(`CMakeLists.txt:15-20`)。
- **可执行文件叫 `Playground` 不叫 `Fish`。** Fish 编成静态库,窗口和主循环在 `Playground/`。

**工作目录必须是仓库根。** `Playground.cpp:107-109` 读的是
`Playground/assets/textures/*.png` 这种仓库根相对路径。CMake 给 VS 调试器也设了
`VS_DEBUGGER_WORKING_DIRECTORY = ${CMAKE_CURRENT_SOURCE_DIR}`(`CMakeLists.txt:111-113`)。

#### `Fish/src/Platform/Vulkan/` —— 从 vulkan_project 搬来的后端

2026-09-19 从 `D:\vulkan_project/src/` **原样拷贝**进来(不是 subtree,历史没跟过来;
历史在 `D:\vulkan_project` 和 GitHub 的 `gagagagagege/vulkanAPI`)。

- **除 `TriangleApp` 外,所有文件都加了 `Vulkan` 前缀**(`Buffer` → `VulkanBuffer`,
  `texture` → `VulkanTexture`)。`main.cpp` → `VulkanMain.cpp`。
  **类名基本没改**,只有 `vkContext` 连类名一起变成了 `VulkanContext`。
- **`VulkanContext` 这个名字不换。**(2026-09-21 改。原先写的是「名字被占了,得另起」)
  它仍然是设备/队列/物理设备那个包装类,**不继承 `GraphicsContext`**。
  M2 是新建 `VulkanRendererAPI`,由它负责 instance、debugMessenger 和创建 `VulkanContext`。
- 它靠 `GLOB_RECURSE` 自动进 `Fish` 静态库,不需要在 CMakeLists 里单独列。
- **`TriangleApp.{h,cpp}` / `VulkanMain.cpp` 2026-09-21 已删**(原型的壳,逻辑收进 Fish)。
  剩下的后端代码**仍是死代码** —— 还没有任何东西 new 它们。接进 Fish 是 W3/W4 的事。
- 资产在 `Platform/Vulkan/{shaders,textures}`。`VulkanPipeline.cpp:9` 按**相对路径**读
  `shaders/slang.spv`,指的是工作目录 —— `CMakeLists.txt` 目前**没有**把它们拷到输出目录。

Vulkan 相关的编译定义集中在 `CMakeLists.txt` 的"Vulkan 后端"一段。其中
`VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS` 不能少,理由见第六节第 5 条。

> **缩进不一致**:`Platform/Vulkan/` 下是 **4 空格**,Fish 原有代码是 **tab**。
> 搬进来时没动,改那批文件时沿用 4 空格,别顺手全转。

### B. `D:\vulkan_project` —— 保留的原型

代码已经拷进 Fish 了,这个目录留着当历史版本,还能独立构建运行。

```bash
cd /d/vulkan_project
cmake -S . -B build          # 增删 src/ 下的文件后【必须】重跑
cmake --build build --config Debug
cd build/Debug && ./VulkanProject.exe     # 工作目录必须是 build/Debug
```

- 它**自带 `GLFW/` 和 `glm/`**(直接提交进 git 的 609 个文件),不走 submodule。
  两份经核对**逐字节一致**,和 Fish 的 `Fish/vendor/{GLFW,glm}` 是同一份。
- 它**自己的 `PLAN.md` 已删**(原先被 `.gitignore:31` 排除、注明"以 Fish 为准",
  留着只会再分叉一次)。要看计划看 `D:\Fish\PLAN.md`。
- 它的 `CLAUDE.md` 已换成指路牌。

### C. 两边差异对照表

| | `D:\Fish` | `D:\vulkan_project` |
|---|---|---|
| C++ 标准 | 20(`CMakeLists.txt:8`) | 20(`CMakeLists.txt:12`) |
| 产物 | `build/bin/DEBUG/Playground.exe` | `build/Debug/VulkanProject.exe` |
| 工作目录 | 仓库根 | 可执行文件所在目录 |
| 资源 | `Playground/assets/` + `Platform/Vulkan/{shaders,textures}` | `src/shaders`、`src/textures`,构建时拷到产物目录 |
| 第三方 | `Fish/vendor/` 走 submodule | `GLFW/`、`glm/` 直接提交 |
| 构建噪音 | `pwsh.exe`(见下) | `pwsh.exe`(见下) |

### 构建时的已知噪音

**两边都有,来源是 vcpkg,不是 Slang。**(2026-09-23 更正。原先写的是「只有 vulkan_project 有,
是它的 post-build 步骤(Slang 编译)调 PowerShell 7」—— 查了,两边的 `CMakeLists.txt` 里
都没有 `pwsh`,也都没有调 PowerShell 的 post-build。)

实际来源是 vcpkg 的 MSBuild 集成,长这样:

```
AppLocalFromInstalled:
  pwsh.exe -ExecutionPolicy Bypass ... -File "C:\vcpkg\scripts\buildsystems\msbuild\applocal.ps1" ...
  'pwsh.exe' 不是内部或外部命令...
  命令"..."已退出，代码为 9009。
  "C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe" -ExecutionPolicy Bypass ... applocal.ps1 ...
```

**它先试 PowerShell 7(这台机器没装),失败后自动退回 `powershell.exe` 重跑一次。**
所以是**无害**的,只在目标真的要链接时才打印(`ZERO_CHECK` 之类的空转不打印)。
要确认每次链接都发生了,看有没有第二行 `powershell.exe`。

### 这个仓库里不要用 `git stash`

(2026-09-18 在 vulkan_project 实测,两个仓库同在 Windows 上、同样会改名重构,所以照样适用。)

文件**改过名**时,`git stash` / `stash pop` 会把工作区搞成混合状态。

- 磁盘文件名回退成旧的大小写(`Buffer.h` → `buffer.h`)
- git 索引里同时留下两套 —— `git ls-files` 里 `Buffer.h` 和 `buffer.h` 各一条
- `git status` 出现 `A src/Buffer.h` + `M src/buffer.h` 这种自相矛盾的条目

**文件内容不会丢**,但索引会乱。根因是 Windows 文件系统大小写不敏感,git 把两条索引
都匹配到了同一个磁盘文件。

修法:换成大小写敏感的视角 `add` 一次,git 才会把"小写那份已经不存在"记成删除。

```bash
git -c core.ignorecase=false add -A src/
```

**要临时对比历史版本,用这些代替:**

```bash
git worktree add /tmp/vp_base HEAD    # 独立检出,不动工作区和索引;用完 git worktree remove
git show HEAD:src/xxx.cpp             # 只看单个文件
```

> 2026-09-19 补:`cp` 在这个环境里偶尔会误报 `'A' and 'B' are the same file` 然后**静默不执行**。
> 用它同步文件后要**复核目标文件确实变了**(比对大小或直接 `diff`),不能只看命令没报错。

### 验证要到什么程度

"没报错"不算验证。至少三条:

1. 构建 0 错误 0 警告
2. 跑十几秒无验证层输出
3. **证明它真在干活** —— 测 CPU 时间(8 秒墙钟烧 14 秒 CPU = 渲染循环全速跑,
   不是卡在 fence 上死等)。`exit=124` 只说明被 timeout 杀了,不说明在渲染

**但上面三条全在测「稳态」。** 2026-09-18 的教训:严格按这三条做完了,resize 一拖就崩,
退出时还有 11 条验证层报错。两条都在**状态转换**上,不在稳态上。

4. **边界事件要单独跑,每一个都是独立路径。**
   - **退出** —— 必须发 `WM_CLOSE` 让它正常退,**不能用 `taskkill /F`**。
     那是 `TerminateProcess`:进程被操作系统直接干掉,**不跑析构函数**。
     凡是只在析构时暴露的问题(资源释放顺序、in-use 销毁)全被它掩盖。
   - resize / 最小化 / 还原
   - 拖窗口 ≠ 程序化改尺寸,真人拖一遍

5. **验证手段本身可能无效。** "脚本跑完了没报错" ≠ "被测的事情真的发生了"。
   2026-09-18:用 PowerShell 调 `MoveWindow` 测 resize,每次都打出 `OK`,
   但那个调用一直抛 `ArgumentException`,窗口从头到尾没变过 —— 测了个寂寞。
   **要断言前置条件真的成立**(客户端尺寸确实变了 / 进程确实是被 `WM_CLOSE` 结束的),
   不能只看"没崩"。
   - 顺带:**PowerShell 5.1 按 ANSI 读 `.ps1`**。脚本里写中文会被拆坏、报
     `字符串缺少终止符`。验证脚本一律纯 ASCII。

**画面本身我确认不了。** 每次改完渲染路径,要明确说"我没亲眼看过"并让我开窗口。
**而且要说清是哪条路径没测** —— 笼统的免责声明没用,欠账要列出来。

## 四、文档体系

| 文件 | 用途 |
|---|---|
| `PLAN.md` | 总计划。里程碑、逐周表、协作约定。有变动**直接改这个文件**。**只有这一份** —— `D:\vulkan_project` 里那份 2026-09-19 已删(它被 `.gitignore:31` 排除、本就注明"以 Fish 为准",留着只会分叉) |
| `BUFFER_REFACTOR_TODO.md` | M1(Buffer / Image / FrameData 重构)的完成记录 + 遗留项,**记录的是在 vulkan_project 做完的那轮**。新的重构**照它的格式另起 `XXX_REFACTOR_TODO.md`** |
| `INTERVIEW_NOTES.md` | 面经笔记(腾讯 / 网易 游戏引擎开发)。**考点清单、各家差异、真题实录只有这一份** —— `PLAN.md` 第四节只留指针 |
| `REVIEW_DRILLS.md` | **设计漏洞题库 + 预测日志**,见第二节「协作模式」。题库只写坐标和"这里有问题",答案等你问过再补 |
| `diary.txt` | 我自己的理解 |
| `README.md` | 对外 |

### diary.txt 的规矩(PLAN 第九节)

> 写你自己的理解,**不要贴 AI 对话**。

让我往里写时,我写**事实**(定义、行号、推导链),并明确说"这是事实记录,不是你的理解"。
**推理那部分你要自己重写一遍** —— 写不顺的地方就是没通的地方。

**2026-09-21 补:把你的原话放在这一段的开头,标 `[我的原话]`。**
理由是原话里有已经整理过一版之后会丢掉的线索 —— 比如"当时在想、后来想不起来了"
那种句子,它标记的是**记录本身的缺口**,整理过的版本反而看不出来。

## 五、代码风格

- 注释、提交信息用中文
- 缩进用 tab(`Fish/src/Platform/Vulkan/` 下是从 vulkan_project 原样搬来的 4 空格,见第三节)
- 提交信息:`type: 中文短标题` + 空行 + 详细正文,**正文讲"为什么"**(参考 `7adbcf2`)
- Vulkan 资源一律 `vk::raii`;裸句柄只在确实只依赖单个对象时用
- 类名首字母大写(`Buffer` / `Image` / `FrameData`),函数名小写开头

## 六、`vk::raii` 的坑

> **每条都要带「怎么验的」**(头文件行号 / 命令 / VUID)。
> 2026-09-18 的教训:第 2 条原来标着「本轮新查证,别重新推导」,实测发现它和 SDK
> 头文件对不上 —— 析构根本不是空操作。
> **「别重新推导」不等于「这条是对的」。** 标了"已验证"反而更没人质疑,错得更久。

1. **复数类型是 `std::vector` 的子类,没有析构函数。**
   `vk::raii::CommandBuffers` / `vk::raii::DescriptorSets` 都是。
   "分配 1 个再 `std::move(...front())`"是安全套路 —— 被搬走的元素在 vector 里变成
   空句柄,临时对象析构时跳过它。
2. **`vk::raii::DescriptorSet` 的析构会真的调 `vkFreeDescriptorSets`。**
   `vulkan_raii.hpp` 里 `~DescriptorSet()` → `clear()`,而 `clear()` 里就是
   `vkFreeDescriptorSets(device, pool, 1, &set)`(头文件 :9702 / :9747 / :9751)。
   **所以 set 必须早于它所属的 `descriptorPool` 释放** —— 池先死,这条路就踩空句柄。
3. **`vk::raii::CommandBuffer` 的析构会 `vkFreeCommandBuffers`。**
   所以**命令缓冲必须早于 `commandPool` 释放** —— 持有它的成员(如 `Frames`)
   在 `TriangleApp` 里的声明位置是被约束的。
4. `vk::raii::CommandBuffer` 没有默认构造函数,但**有 `nullptr_t` 构造**,
   所以 `vk::raii::CommandBuffer x = nullptr;` 可以。
5. **`VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS` 不定义的话,
   `acquireNextImage` / `presentKHR` 遇到 `VK_ERROR_OUT_OF_DATE_KHR` 会抛异常,
   不是返回错误码。** 2026-09-18 前它从没定义过 → `drawFrame` 里那两个
   `if (result == eErrorOutOfDateKHR)` 一直是死代码,resize 直接杀进程。
   现在两边都定义了:Fish 在 `CMakeLists.txt` 的"Vulkan 后端"一段,
   vulkan_project 在 `CMakeLists.txt:82`。
   (出处:`vulkan_raii.hpp` :19218 acquire / :19230 present 的 `#if` 分支)

> `BUFFER_REFACTOR_TODO.md` 第八节还有 3 条更早的教训(编辑器没保存导致读旧文件、
> 改名时词边界搜索漏掉限定用法、间歇性问题必须多采样),继续有效。

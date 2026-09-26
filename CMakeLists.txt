cmake_minimum_required(VERSION 3.16)

project(FishEngine C CXX)

# 设置 C++ 标准
# 20 是 Vulkan 后端的要求（Fish/src/Platform/Vulkan 从 vulkan_project 搬来，
# 那边是 C++20）。Fish 原有代码是 C++17，向下兼容，不用改。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

# 统一输出目录，确保 Playground 的 exe 和 Fish 的 lib 输出到同一文件夹，方便运行
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib/${OUTPUTCONFIG})
endforeach()

# =========================================================
# 1. 引入第三方依赖
# =========================================================
# 对应你的第三方库子模块位置 (确保 Fish/vendor/spdlog 中已经有了完整代码)
# EXCLUDE_FROM_ALL 意味着我们不会主动安装或构建其不需要的额外示例等
add_subdirectory(Fish/vendor/spdlog EXCLUDE_FROM_ALL)
add_subdirectory(Fish/vendor/GLFW EXCLUDE_FROM_ALL)
add_subdirectory(Fish/vendor/glm EXCLUDE_FROM_ALL)

# ImGui — 纯源码头文件库，没有自带的 CMakeLists，手动编译
set(IMGUI_DIR Fish/vendor/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp          # 可选，Demo 窗口
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
# ImGui 需要链接 GLFW 和 OpenGL（通过 glad 提供）
target_link_libraries(imgui PUBLIC glfw glad)


# glad — OpenGL loader (静态库)
add_library(glad STATIC
    Fish/vendor/GLAD/src/glad.c
)
target_include_directories(glad PUBLIC
    Fish/vendor/GLAD/include
)

# ---- Vulkan SDK ----
# Fish/src/Platform/Vulkan 需要它。默认指向本机安装位置，
# 别的机器上用 -DVulkan_ROOT=<路径> 覆盖。
if(NOT DEFINED Vulkan_ROOT)
    set(Vulkan_ROOT "D:/Vulkan_SDK" CACHE PATH "Vulkan SDK 安装目录")
endif()
find_package(Vulkan REQUIRED)

# GLFW 的 vulkan.c 要 Vulkan 头文件
target_include_directories(glfw PRIVATE ${Vulkan_INCLUDE_DIRS})

# =========================================================
# 2. Fish 静态库项目
# =========================================================
file(GLOB_RECURSE FISH_SOURCES
    "Fish/src/*.h"
    "Fish/src/*.cpp"
)

add_library(Fish STATIC ${FISH_SOURCES})

# 将 stb_image 的两个文件直接作为 Fish 的源文件
target_sources(Fish PRIVATE
    Fish/vendor/stb_image/stb_image.h
    Fish/vendor/stb_image/stb_image.cpp
)

# SPIRV-Reflect 同理。Vulkan 后端用它反射顶点布局和描述符布局
# （SPIR-V 里没有 vertex binding 号和 stride，那两样由调用方给）。
# 从 Vulkan SDK 的 Source/SPIRV-Reflect 拷来，Apache 2.0。
# SDK 的 Lib/ 下有预编译的 spirv-reflect-static.lib，但 find_package(Vulkan)
# 没有暴露对应 target，所以走源码。
target_sources(Fish PRIVATE
    Fish/vendor/SPIRV-Reflect/spirv_reflect.h
    Fish/vendor/SPIRV-Reflect/spirv_reflect.c
)

# target_precompile_headers 会给本 target 下所有源文件加 /FI fspch.h，
# 而 fspch.h 里是 STL —— C 编译器吃到 yvals_core.h 会报 STL1003。
# 只给这一个 .c 关掉。
set_source_files_properties(Fish/vendor/SPIRV-Reflect/spirv_reflect.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# spirv_reflect.h 默认按 SDK 自己的目录布局找 "./include/spirv/unified1/spirv.h"，
# 那是 SDK source 树里的相对路径，拷出来就断了。定义这个走系统头，
# 即 SDK 的 Include/spirv/unified1/spirv.h —— 它跟着 Vulkan::Vulkan 的
# include 路径一起进来。不另外拷一份：spirv.h 必须和 vulkan_core.h 同版本。
target_compile_definitions(Fish PRIVATE SPIRV_REFLECT_USE_SYSTEM_SPIRV_H)

# 设置包含目录
target_include_directories(Fish PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Fish/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Fish/vendor/stb_image>
)
# PRIVATE：只有 Vulkan 后端用，不向 Playground 传播
target_include_directories(Fish PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Fish/vendor/SPIRV-Reflect>
)

target_compile_definitions(Fish PRIVATE GLFW_INCLUDE_NONE)

# 当在 Windows 上编译时，定义 FS_PLATFORM_WINDOWS，公开使得链接的 Playground 也继承该宏
if(WIN32)
    target_compile_definitions(Fish PUBLIC FS_PLATFORM_WINDOWS)
endif()

# Debug 模式下启用断言
target_compile_definitions(Fish PUBLIC $<$<CONFIG:Debug>:FS_DEBUG>)

# 设置预编译头 (CMake 3.16+)
target_precompile_headers(Fish PRIVATE "Fish/src/fspch.h")

# 链接。PUBLIC会让链接Fish的Playground也能找到它们。
target_link_libraries(Fish PUBLIC spdlog::spdlog glfw glad imgui glm::glm)

# ---- Vulkan 后端（Fish/src/Platform/Vulkan）----
# 定义与 vulkan_project/CMakeLists.txt:82-91 对齐。其中最后一条关键：
# 不定义的话，vk::raii 的 acquireNextImage / presentKHR 遇到
# VK_ERROR_OUT_OF_DATE_KHR 会直接抛异常，resize 会杀进程。
target_link_libraries(Fish PUBLIC Vulkan::Vulkan)
target_compile_definitions(Fish PRIVATE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    NOMINMAX
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS=1
    $<$<CONFIG:Debug>:DEBUG>        # VulkanValidationLayers.h 靠它开关验证层
)
if(MSVC)
    # Platform/Vulkan 下是中文注释；不设这个，中文 locale 下会报 C4819
    target_compile_options(Fish PRIVATE /utf-8)
endif()


# =========================================================
# 3. Playground 可执行程序项目
# =========================================================
file(GLOB_RECURSE PLAYGROUND_SOURCES 
    "Playground/src/*.cpp"
    "Playground/src/*.h"
)

add_executable(Playground ${PLAYGROUND_SOURCES})

# 链接 Fish 引擎项目
target_link_libraries(Playground PRIVATE Fish)

# 设置 VS 调试器工作目录为项目根目录，使相对路径（如 assets/）正确解析
set_target_properties(Playground PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

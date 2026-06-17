cmake_minimum_required(VERSION 3.16)

project(FishEngine C CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

# 统一输出目录，确保 Playground 的 exe 和 Fish 的 dll 输出到同一文件夹，方便运行
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

# =========================================================
# 2. Fish 动态库项目
# =========================================================
file(GLOB_RECURSE FISH_SOURCES 
    "Fish/src/*.h"
    "Fish/src/*.cpp"
)

add_library(Fish SHARED ${FISH_SOURCES})

# 设置包含目录：指向 Fish/src
target_include_directories(Fish PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Fish/src>
)

# 编译动态库时的宏定义
target_compile_definitions(Fish PRIVATE 
                                  FS_BUILD_DLL
                                  GLFW_INCLUDE_NONE
                                                     )

# 当在 Windows 上编译时，定义 FS_PLATFORM_WINDOWS，公开使得链接的 Playground 也继承该宏
if(WIN32)
    target_compile_definitions(Fish PUBLIC FS_PLATFORM_WINDOWS)
endif()

# 设置预编译头 (CMake 3.16+)
target_precompile_headers(Fish PRIVATE "Fish/src/fspch.h")

# 链接。PUBLIC会让链接Fish的Playground也能找到它们。
target_link_libraries(Fish PUBLIC spdlog::spdlog glfw glad imgui glm::glm)


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

@echo off
chcp 65001
echo ==================================
echo 开始 CMake 构建 + 生成 VS 工程
echo ==================================

:: -B 指定构建目录 build，-S 指定源码目录(当前目录)
rd /s /q "build"
cmake -B build -S .

echo.
echo 构建完成！
echo 工程文件位置: build\*.sln
pause
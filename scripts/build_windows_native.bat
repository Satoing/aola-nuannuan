@echo off
REM build_windows_native.bat - 在 Windows 上使用 vcpkg 构建
REM 
REM 前置条件:
REM   1. 安装 Visual Studio 2019/2022 (含 C++ 桌面开发)
REM   2. 安装 vcpkg: git clone https://github.com/microsoft/vcpkg
REM      cd vcpkg && bootstrap-vcpkg.bat
REM   3. 设置环境变量 VCPKG_ROOT 指向 vcpkg 目录
REM   4. 安装依赖:
REM      vcpkg install sdl2:x64-windows sdl2-image:x64-windows sdl2-ttf:x64-windows
REM      vcpkg install nlohmann-json:x64-windows curl:x64-windows sqlite3:x64-windows

echo =========================================
echo   奥拉暖暖 - Windows 原生构建
echo =========================================

if not defined VCPKG_ROOT (
    echo 错误: 请设置 VCPKG_ROOT 环境变量
    echo 例如: set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

set BUILD_DIR=build-windows
set OUTPUT_DIR=AolaNuanNuan-Windows-Portable

REM 清理
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
mkdir "%BUILD_DIR%"

echo [1/4] CMake 配置...
cmake -B "%BUILD_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DVCPKG_TARGET_TRIPLET=x64-windows

if errorlevel 1 (
    echo CMake 配置失败!
    exit /b 1
)

echo [2/4] 编译...
cmake --build "%BUILD_DIR%" --config Release -j%NUMBER_OF_PROCESSORS%

if errorlevel 1 (
    echo 编译失败!
    exit /b 1
)

echo [3/4] 打包 Portable...
mkdir "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%\fonts"

REM 复制 exe
copy "%BUILD_DIR%\Release\AolaNuanNuan.exe" "%OUTPUT_DIR%\" >nul 2>&1
if not exist "%OUTPUT_DIR%\AolaNuanNuan.exe" (
    copy "%BUILD_DIR%\AolaNuanNuan.exe" "%OUTPUT_DIR%\" >nul 2>&1
)

REM 复制图标
copy icon.png "%OUTPUT_DIR%\" >nul 2>&1

REM 复制 vcpkg 的 DLL
echo   复制依赖 DLL...
for /R "%VCPKG_ROOT%\installed\x64-windows\bin" %%f in (*.dll) do (
    copy "%%f" "%OUTPUT_DIR%\" >nul 2>&1
)

REM 也从构建目录复制 DLL
for /R "%BUILD_DIR%" %%f in (*.dll) do (
    copy "%%f" "%OUTPUT_DIR%\" >nul 2>&1
)

REM 创建说明文件
(
echo 奥拉暖暖 - Windows Portable 版
echo.
echo 【使用方法】
echo 双击 AolaNuanNuan.exe 运行
echo.
echo 【字体说明】
echo 程序会自动使用 Windows 系统中文字体（微软雅黑等）。
echo 如果中文显示异常，请将中文字体文件复制到 fonts 目录。
echo.
echo 【操作说明】
echo - 左侧分类标签切换服装类型
echo - 点击列表中的服装名称穿上/脱下
echo - 鼠标滚轮滚动列表
echo - 右上角切换人模
echo - 保存/加载按钮管理装扮方案
echo - R键重置 ^| F键搜索 ^| ESC键退出
) > "%OUTPUT_DIR%\说明.txt"

echo [4/4] 压缩...
powershell -Command "Compress-Archive -Path '%OUTPUT_DIR%' -DestinationPath '%OUTPUT_DIR%.zip' -Force"

echo.
echo =========================================
echo   构建完成！
echo   输出: %OUTPUT_DIR%.zip
echo =========================================

dir "%OUTPUT_DIR%"
pause

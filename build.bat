@echo off
REM Build script for MouseChargeTray using CMake + Ninja + vcpkg

set BUILD_DIR=build
set CMAKE_GENERATOR=Ninja

REM Set vcpkg root from script location (adjust if needed)
set VCPKG_ROOT=%~dp0vcpkg

REM Check if vcpkg exists
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ERROR: vcpkg not found at %VCPKG_ROOT%\vcpkg.exe
    echo Please install vcpkg or adjust the path in build.bat
    pause
    exit /b 1
)

REM Install vcpkg dependencies if needed
echo Checking vcpkg dependencies...
"%VCPKG_ROOT%\vcpkg.exe" install spdlog lunasvg --recurse 2>&1
if errorlevel 1 (
    echo WARNING: vcpkg install had issues, but continuing...
)

REM Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

cd /d "%BUILD_DIR%"

REM Configure with CMake using vcpkg toolchain
echo Configuring with CMake and vcpkg...
cmake -G "%CMAKE_GENERATOR%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ..
if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

REM Build the project
echo Building...
cmake --build . --config Release
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo Build successful!
echo Executable: %CD%\main.exe
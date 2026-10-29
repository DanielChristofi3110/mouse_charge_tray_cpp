@echo off
REM Interactive Build Script for MouseChargeTray
REM This script guides you through the setup and build process

setlocal EnableDelayedExpansion

echo ============================================
echo  Mouse Charge Tray - Interactive Build
============================================
echo.

REM Check for vcpkg
set VCPKG_ROOT=%~dp0vcpkg

if not exist "%VCPKG_ROOT%" (
    echo [SETUP] vcpkg not found in project directory.
    echo.
    echo Would you like to clone vcpkg? (Y/N)
    set /p choice="Enter choice: "
    
    if /i "!choice!"=="Y" (
        echo [SETUP] Cloning vcpkg...
        git clone https://github.com/microsoft/vcpkg.git vcpkg
        if errorlevel 1 (
            echo [ERROR] Failed to clone vcpkg. Please check your internet connection.
            pause
            exit /b 1
        )
        echo [SETUP] vcpkg cloned successfully.
        echo [SETUP] Bootstrapping vcpkg...
        call vcpkg\bootstrap-vcpkg.bat
        if errorlevel 1 (
            echo [ERROR] vcpkg bootstrap failed.
            pause
            exit /b 1
        )
        echo [SETUP] vcpkg is ready!
    ) else (
        echo [INFO] Skipping vcpkg setup. You can set VCPKG_ROOT manually.
        echo [INFO] Continuing with existing vcpkg or manual setup...
    )
) else (
    echo [SETUP] vcpkg found at: %VCPKG_ROOT%
)

REM Check vcpkg.exe exists
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [SETUP] vcpkg.exe not found. Attempting bootstrap...
    if exist "%VCPKG_ROOT%\bootstrap-vcpkg.bat" (
        call %VCPKG_ROOT%\bootstrap-vcpkg.bat
    ) else (
        echo [ERROR] vcpkg bootstrap script not found.
        echo [INFO] Please ensure vcpkg is properly installed.
        pause
        exit /b 1
    )
)

echo.
echo [SETUP] Checking and installing required packages...
echo [SETUP] Required: spdlog, lunasvg, simpleini

echo.
echo Would you like to install/update dependencies? (Y/N)
set /p install_choice="Enter choice: "

if /i "!install_choice!"=="Y" (
    echo [INSTALL] Installing dependencies via vcpkg...
    "%VCPKG_ROOT%\vcpkg.exe" install spdlog lunasvg simpleini --recurse
    if errorlevel 1 (
        echo [WARNING] Some packages may have failed to install.
        echo [INFO] Check the output above for details.
    ) else (
        echo [INSTALL] Dependencies installed successfully!
    )
) else (
    echo [INFO] Skipping dependency installation.
)

echo.
echo ============================================
echo  Building Mouse Charge Tray
============================================
echo.

REM Create build directory
set BUILD_DIR=build
if not exist "%BUILD_DIR%" (
    echo [BUILD] Creating build directory...
    mkdir "%BUILD_DIR%"
)

REM Configure with CMake
cd /d "%BUILD_DIR%"

echo [BUILD] Configuring with CMake (Ninja + vcpkg)...
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ..
if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    echo [INFO] Check that:
    echo         - vcpkg is properly set up
    echo         - CMake and Ninja are in your PATH
    echo         - The vcpkg toolchain file exists
    pause
    exit /b 1
)

echo [BUILD] Configuration complete.
echo.

REM Build
echo [BUILD] Building project...
cmake --build . --config Release
if errorlevel 1 (
    echo [ERROR] Build failed!
    echo [INFO] Check the error messages above.
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Build Complete!
============================================
echo [SUCCESS] Executable: %CD%\MouseChargeTray.exe
echo [SUCCESS] Resources: %CD%\res\
echo [SUCCESS] Compile commands: %CD%\compile_commands.json
echo.
echo [INFO] You can now run MouseChargeTray.exe from this directory.
echo [INFO] The application will appear in your system tray.
echo.
pause

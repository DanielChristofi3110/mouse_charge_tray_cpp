cmake_minimum_required(VERSION 3.15)
project(MouseChargeTray LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use vcpkg toolchain (set via build.bat or environment)
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Find packages via vcpkg
find_package(spdlog CONFIG REQUIRED)
find_package(lunasvg CONFIG REQUIRED)
find_package(SimpleIni CONFIG REQUIRED)

add_executable(MouseChargeTray
WIN32
src/main.cpp
src/Config/parameters.cpp
src/DeviceScan/DeviceScan.cpp
src/BatteryTimer/BatteryTimer.cpp
)

# Copy res/ folder to build directory
add_custom_command(TARGET MouseChargeTray POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res
        ${CMAKE_BINARY_DIR}/res
)

# Link using imported targets (vcpkg convention)
target_link_libraries(MouseChargeTray PRIVATE spdlog::spdlog lunasvg::lunasvg SimpleIni::SimpleIni)

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

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(MouseChargeTray PRIVATE
        # -Wall
        -Wno-c++98-compat #modern c++
        -Wno-c++98-compat-pedantic
        -Wno-padded #alow padding
        -Wno-newline-eof #alow no new line
        -Wno-exit-time-destructors #global string destructor
        -Wno-shorten-64-to-32 #conversion manualyhandled in helpers
        -Wno-covered-switch-default #switch WILL HAVE a default
        -Wno-global-constructors # global string constructor
        -Wno-suggest-destructor-override
        -Wno-non-virtual-dtor
        -Wno-unsafe-buffer-usage-in-libc-call #memcpy reenabeke if posible
        -Wno-nonportable-system-include-path
        -Wno-implicit-int-float-conversion
        -Wno-missing-prototypes


        -Wno-unsafe-buffer-usage #remove
        -Wno-old-style-cast #thene remove

        -Wno-suggest-override

        -Wextra
        -Wunused-member-function
        -Wpedantic
        -Wshadow
        # -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wformat=2
        # -Werror
    )
endif()

# Copy res/ folder to build directory
add_custom_command(TARGET MouseChargeTray POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res
        ${CMAKE_BINARY_DIR}/res
)

# Link using imported targets (vcpkg convention)
target_link_libraries(MouseChargeTray PRIVATE spdlog::spdlog lunasvg::lunasvg SimpleIni::SimpleIni)

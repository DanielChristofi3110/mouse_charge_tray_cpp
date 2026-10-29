# Mouse Charge Tray

A Windows system tray application that monitors and displays battery/charging status for your mouse.

## Features

- System tray icon showing battery status
- Charging notification
- Battery level display
- Configurable polling intervals

## Prerequisites

- **CMake** 3.15 or higher
- **Ninja** build system
- **vcpkg** - for managing C++ dependencies
- **C++17 compatible compiler** (MSVC, Clang, or GCC)
- **Windows 10/11**

## Dependencies

- [spdlog](https://github.com/gabime/spdlog) - Logging library
- [lunasvg](https://github.com/MeirionEmo/LunaSVG) - SVG rendering
- [SimpleIni](https://github.com/brofield/simpleini) - INI file parsing

All managed through [vcpkg](https://github.com/microsoft/vcpkg).

## Build Instructions

### Using the provided build script:

1. Open a terminal in the project root directory
2. Run `build.bat`
3. The script will:
   - Install vcpkg dependencies
   - Configure CMake with Ninja and vcpkg toolchain
   - Build the project
   - Copy resources to `build/res/`

### Manual build:

```bash
# Install dependencies via vcpkg
vcpkg install spdlog lunasvg simpleini --recurse

# Configure CMake
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" ..

# Build
cmake --build build --config Release
```

## Usage

1. Run `MouseChargeTray.exe` from the `build/` directory
2. The application will appear in the system tray
3. Configure battery monitoring via settings file (see Configuration)

## Configuration

The application uses an INI configuration file with settings managed via the `Parameters` class (see `src/Config/parameters.hpp`).

### Configuration options:

```ini
[Parameters]
device_name = Meetion_MS_BT1
svg_path = mouse.svg
update_ms = 30000
```

- `device_name`: Name of the mouse/device to monitor (default: `Meetion_MS_BT1`)
- `svg_path`: Path to the SVG file for display (default: `mouse.svg`)
- `update_ms`: Update interval in milliseconds (default: `30000`, i.e., 30 seconds)

These values are loaded from the INI configuration file using the SimpleIni library and applied to the application parameters.

## Project Structure

```
mouse_charge_tray_cpp/
├── CMakeLists.txt          # Build configuration
├── build.bat               # Windows build script
├── vcpkg/                  # vcpkg dependency management
├── res/                    # Resource files
│   └── config.ini         # Configuration file
├── src/
│   ├── main.cpp            # Main application entry
│   └── Config/
│       └── parameters.cpp # Configuration handling
└── build/                  # Build output directory
    ├── MouseChargeTray.exe
    └── res/                # Copied resources
```

## Development

- **Compile Commands**: `build/compile_commands.json` is generated for IDE integration
- **Resources**: The `res/` folder is automatically copied to `build/res/` on each build
- **Dependencies**: Managed through vcpkg; update versions in `vcpkg.json`

## License

GPL .

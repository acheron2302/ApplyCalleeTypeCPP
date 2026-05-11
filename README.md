# hello_idax

A minimal, cross-platform IDA Pro plugin template using [idax](https://github.com/19h/idax) and [cmkr](https://cmkr.build/).

This template provides a complete starting point for developing IDA Pro plugins in modern C++23, with automatic dependency management and a streamlined build process.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Requirements](#requirements)
3. [Setup](#setup)
4. [Build Instructions](#build-instructions)

---

## Introduction

**hello_idax** is a minimal "Hello, World!" plugin for IDA Pro that demonstrates:

- Modern C++23 plugin development using the [idax](https://github.com/19h/idax) wrapper library
- Type-safe, idiomatic C++ API instead of raw IDA SDK C calls
- Cross-platform support (Windows, Linux, macOS)
- Automatic dependency fetching (no manual library installation)
- Clean build system using [cmkr](https://cmkr.build/) (CMake with TOML configuration)

When activated in IDA Pro, this plugin prints "Hello, World!" to the output window.

### Project Structure

```
hello_idax/
├── cmake.toml          # cmkr build configuration (TOML format)
├── src/
│   └── hello.cpp       # Plugin source code
├── deps/               # Fetched dependencies (auto-populated)
├── build/              # Build output directory (generated)
└── README.md           # This file
```

---

## Requirements

### Mandatory

| Component | Version | Purpose |
|-----------|---------|---------|
| **CMake** | 3.27+ | Build system generator (bootstraps cmkr) |
| **cmkr** | auto | CMake TOML frontend (auto-bootstrapped) |
| **C++ Compiler** | C++20 capable | Plugin compilation |
| **IDA SDK** | Any recent version | IDA Pro development kit |
| **ida-cmake** | latest | IDA SDK CMake integration |
| **IDASDK** env var | Set to SDK path | SDK location discovery |

### Compiler Requirements

- **Clang**: 17 or later
- **GCC**: 14 or later  
- **MSVC**: Visual Studio 2022 17.10 or later

### Supported Platforms

- Windows (x64)
- Linux (x64)
- macOS (x64, ARM64)

---

## Setup

### Step 1: Bootstrap cmkr

cmkr is automatically bootstrapped when you run CMake. No manual installation needed.

To bootstrap cmkr manually:
```bash
cmake -P cmkr.cmake
```

### Step 2: Install IDA SDK

1. Download the IDA SDK from [Hex-Rays](https://hex-rays.com/ida-pro/)
2. Extract it to a known location (e.g., `~/idasdk` or `C:\idasdk`)

### Step 3: Set IDASDK Environment Variable

**Linux/macOS (bash/zsh):**
```bash
# Add to ~/.bashrc or ~/.zshrc
export IDASDK=/path/to/idasdk

# Apply immediately
source ~/.bashrc  # or ~/.zshrc
```

**Windows (PowerShell):**
```powershell
# Set for current session
$env:IDASDK = "C:\path\to\idasdk"

# Set permanently (requires admin)
[Environment]::SetEnvironmentVariable("IDASDK", "C:\path\to\idasdk", "User")
```

**Windows (CMD):**
```cmd
setx IDASDK C:\path\to\idasdk
```

Verify the variable is set:
```bash
echo $IDASDK        # Linux/macOS
echo %IDASDK%       # Windows CMD
$env:IDASDK         # Windows PowerShell
```

### Step 4: Install ida-cmake

ida-cmake provides CMake integration for the IDA SDK.

```bash
cd $IDASDK                    # Or your IDA SDK directory
git clone https://github.com/0xeb/ida-cmake.git
```

Verify installation:
```bash
ls $IDASDK/ida-cmake/bootstrap.cmake
```

**Linux/macOS (bash/zsh):**
```bash
# Add to ~/.bashrc or ~/.zshrc
export IDASDK=/path/to/idasdk

# Apply immediately
source ~/.bashrc  # or ~/.zshrc
```

**Windows (PowerShell):**
```powershell
# Set for current session
$env:IDASDK = "C:\path\to\idasdk"

# Set permanently (requires admin)
[Environment]::SetEnvironmentVariable("IDASDK", "C:\path\to\idasdk", "User")
```

**Windows (CMD):**
```cmd
setx IDASDK C:\path\to\idasdk
```

Verify the variable is set:
```bash
echo $IDASDK        # Linux/macOS
echo %IDASDK%       # Windows CMD
$env:IDASDK         # Windows PowerShell
```

---

## Build Instructions

### Quick Build

```bash
# 1. Set IDASDK environment variable (if not already set)
export IDASDK=/path/to/idasdk  # Linux/macOS
$env:IDASDK = "C:\path\to\idasdk"  # Windows PowerShell

# 2. Generate build files (cmkr is bootstrapped automatically)
cmake -B build

# 3. Build the plugin
cmake --build build
```

### Detailed Build Steps

#### 1. Bootstrap cmkr

cmkr is automatically bootstrapped when you run `cmake -B build`. The `cmkr.cmake` script will:
- Download and build cmkr if not present
- Generate `CMakeLists.txt` from `cmake.toml`

If you need to regenerate CMakeLists.txt after modifying `cmake.toml`:
```bash
cmake -P cmkr.cmake
cmake -B build
```

#### 2. Configure Build

Generate the build system files:

```bash
cmake -B build
```

On first run, this will:
- Fetch idax from GitHub automatically
- Bootstrap ida-cmake from your IDA SDK
- Configure compiler settings for your platform

#### 3. Compile

Build the plugin:

```bash
cmake --build build
```

For multi-config generators (Visual Studio, Ninja Multi-Config):
```bash
cmake --build build --config Release
```

#### 4. Output Location

The compiled plugin will be at:

| Platform | Output Path |
|----------|-------------|
| Windows | `build/hello.dll` |
| Linux | `build/hello.so` |
| macOS | `build/hello.dylib` |

### Installation

Copy the plugin to your IDA Pro plugins directory:

**Linux/macOS:**
```bash
cp build/hello.so $IDA_DIR/plugins/
# Or create symlink for development:
ln -s $(pwd)/build/hello.so $IDA_DIR/plugins/hello.so
```

**Windows:**
```cmd
copy build\Release\hello.dll %IDA_DIR%\plugins\
```

### Usage in IDA Pro

1. Start IDA Pro
2. Open any database
3. Activate the plugin:
   - **Menu**: Edit → Plugins → Hello
   - **Hotkey**: `Ctrl-Alt-H`
4. Check the Output window for "Hello, World!"

---

## Troubleshooting

### "cmkr: command not found"

cmkr is automatically bootstrapped by `cmkr.cmake`. Just run:
```bash
cmake -P cmkr.cmake
```

### "IDASDK environment variable is not set"

Set the `IDASDK` environment variable pointing to your IDA SDK root (see [Setup - Step 3](#step-3-set-idasdk-environment-variable)).

### "Could not find ida-cmake/bootstrap.cmake"

ida-cmake is not installed in your IDA SDK. Install it:
```bash
cd $IDASDK
git clone https://github.com/0xeb/ida-cmake.git
```

### FetchContent fails to download idax

Check your internet connection. You can also manually clone idax:
```bash
cd deps
git clone https://github.com/19h/idax.git
```

Then re-run `cmake -B build`.

### "No CMAKE_CXX_COMPILER could be found"

Install a C++23 capable compiler:
- **Linux**: `sudo apt install clang-17` or `sudo apt install g++-14`
- **macOS**: Install Xcode Command Line Tools
- **Windows**: Install Visual Studio 2022 with C++ workload

---

## Customizing the Template

To create your own plugin:

1. **Rename the project** in `cmake.toml`:
   ```toml
   [project]
   name = "my_plugin"
   ```

2. **Rename the target** in `cmake.toml`:
   ```toml
   [target.myplugin]
   type = "shared"
   sources = ["src/myplugin.cpp"]
   ```

3. **Update the plugin class** in `src/hello.cpp`:
   ```cpp
   class MyPlugin final : public ida::plugin::Plugin {
       // ... your implementation
   };
   IDAX_PLUGIN(MyPlugin);
   ```

4. **Re-run cmake** to build:
   ```bash
   cmake -B build
   cmake --build build
   ```

---

## References

- [idax Repository](https://github.com/19h/idax)
- [idax Plugin Examples](https://github.com/19h/idax/tree/master/examples/plugin)
- [cmkr Documentation](https://cmkr.build/cmake-toml/)
- [ida-cmake Repository](https://github.com/0xeb/ida-cmake)
- [IDA Pro SDK Documentation](https://hex-rays.com/products/ida/support/idapronotes.shtml)

---

## License

This template is provided as-is for IDA Pro plugin development.

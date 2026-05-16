# ApplyCalleeTypeEx

Apply callee type to indirect CALL instructions in IDA Pro. Press **Shift+A** on a `call` instruction to assign a function prototype from manual input, TIL type libraries, or local database types.

Built with [idax](https://github.com/19h/idax) (C++23 IDA SDK wrapper), Qt 6.8.0 dialogs, and [cmkr](https://cmkr.build/).

---

## Features

- **Three type sources**: manual C-declaration input, TIL (type library) types, and local database types
- **Real-world input tolerant**: strips SAL annotations, `__declspec`, calling-convention macros, bracket annotations, and normalizes whitespace automatically
- **Cross-platform**: Windows (MSVC) and Linux (GCC) via GitHub Actions CI
- **Auto-builds** for latest IDA SDK v9.2 and v9.3 tags

---

## Requirements

| Component | Version | Notes |
|-----------|---------|-------|
| **CMake** | 3.27+ | Build system |
| **C++ Compiler** | C++23 capable | MSVC 2022, GCC 14+, Clang 17+ |
| **IDA SDK** | 9.2 or 9.3 | From [HexRaysSA/ida-sdk](https://github.com/HexRaysSA/ida-sdk) |
| **ida-cmake** | latest | Cloned into IDA SDK root |
| **Qt 6** | 6.8.0 | Core, Gui, Widgets components |
| **IDASDK** env var | — | Points to IDA SDK root directory |

---

## Setup

### 1. Clone and install ida-cmake

```bash
git clone <this-repo>
export IDASDK=/path/to/ida-sdk
cd $IDASDK
git clone https://github.com/allthingsida/ida-cmake.git ida-cmake
```

### 2. Configure Qt6 path in `cmake.toml`

> **Important**: You must update the `Qt6_DIR` path in `cmake.toml` to point to your local Qt 6.8.0 build.

Open `cmake.toml` and find this block near line 64:

```cmake
# Find IDA's Qt6 cmake config
if(EXISTS "E:/tools/qt-build/linux/qt-yusa.tar/qt-yusa/x64win/lib/cmake/Qt6")
    set(Qt6_DIR "E:/tools/qt-build/linux/qt-yusa.tar/qt-yusa/x64win/lib/cmake/Qt6" CACHE PATH "Qt6 CMake directory" FORCE)
    message(STATUS "Found IDA Qt6 at: ${Qt6_DIR}")
endif()
```

Replace the path with your local Qt 6.8.0 cmake directory. Examples:

**Windows (MSVC Qt build):**
```cmake
if(EXISTS "C:/Qt/6.8.0/msvc2022_64/lib/cmake/Qt6")
    set(Qt6_DIR "C:/Qt/6.8.0/msvc2022_64/lib/cmake/Qt6" CACHE PATH "Qt6 CMake directory" FORCE)
```

**Linux (GCC Qt build):**
```cmake
if(EXISTS "/opt/Qt/6.8.0/gcc_64/lib/cmake/Qt6")
    set(Qt6_DIR "/opt/Qt/6.8.0/gcc_64/lib/cmake/Qt6" CACHE PATH "Qt6 CMake directory" FORCE)
```

If `Qt6_DIR` is not set, CMake will try to find Qt automatically via `find_package(Qt6)`. The `jurplel/install-qt-action` in CI handles this automatically.

### 3. Build

```bash
cmake -B build
cmake --build build
```

The plugin output goes to `$IDABIN/plugins/` (set via the `IDABIN` environment variable, defaults to SDK plugin directory).

### 4. Install in IDA Pro

Copy the built plugin to your IDA Pro plugins directory:

```bash
# Linux
cp build/ApplyCalleeTypeEx.so ~/ida-pro/plugins/

# Windows
copy build\Release\ApplyCalleeTypeEx.dll "%IDA_DIR%\plugins\"
```

---

## Usage

1. Open a database in IDA Pro
2. Place the cursor on an indirect `call` instruction
3. Press **Shift+A** (or Edit → Operand type → ApplyCalleeTypeEx)
4. Choose a type source:
   - **Enter Manually** — paste or type a C function prototype
   - **Standard Type (TIL)** — browse types from loaded type libraries
   - **Local Type** — browse types defined in the current database
5. The function pointer type is applied to the call target

Examples of accepted input:

```
UINT WinExec(LPCSTR lpCmdLine, UINT uCmdShow);
typedef UINT (WINAPI *PWINEXEC)(LPCSTR, UINT);
NTSYSAPI NTSTATUS NTAPI LdrGetProcedureAddress(_In_ PVOID DllHandle, ...);
```

---

## CI/CD

GitHub Actions automatically builds the plugin for every push and PR.

- **SDK versions**: latest `v9.3.x` and `v9.2.x` tags from [HexRaysSA/ida-sdk](https://github.com/HexRaysSA/ida-sdk)
- **Platforms**: Windows (MSVC) and Linux (GCC)
- **Qt**: 6.8.0 via `jurplel/install-qt-action`
- **Artifacts**: `.dll` / `.so` files uploaded per build
- **Releases**: auto-published on push to `main`/`master` (not on PRs)

---

## Project Structure

```
ApplyCalleeTypeEx/
├── cmake.toml                 # cmkr build configuration
├── cmkr.cmake                 # cmkr bootstrap (auto)
├── CMakeLists.txt             # Generated — do not edit
├── .github/workflows/
│   └── cmake-cross-platform.yml  # CI/CD
├── src/
│   ├── main.cpp               # Plugin entry, action handler, type applier
│   ├── preprocessing.hpp/cpp  # Prototype cleaning (SAL, declspec, etc.)
│   └── qt_dialogs.hpp/cpp     # Qt dialogs (type source, manual input, TIL/local browser)
└── README.md
```

---

## Troubleshooting

### `IDASDK environment variable is not set`

Set the `IDASDK` variable:

```bash
export IDASDK=/path/to/ida-sdk        # Linux
$env:IDASDK = "C:\path\to\ida-sdk"    # Windows PowerShell
```

### `Could not find ida-cmake/bootstrap.cmake`

Clone ida-cmake into your SDK:

```bash
cd $IDASDK
git clone https://github.com/allthingsida/ida-cmake.git ida-cmake
```

### `find_package(Qt6) fails`

Either set `Qt6_DIR` in `cmake.toml` (see Setup step 2) or pass it to CMake:

```bash
cmake -B build -DQt6_DIR=/path/to/Qt/6.8.0/gcc_64/lib/cmake/Qt6
```

### Type parsing fails

- Ensure the input is a function declaration or function pointer typedef
- Check that SAL annotations and calling conventions are stripped (preprocessing handles most common patterns)
- Very long declarations (>4096 chars) are truncated

### `std::atomic::dont_use_wait` build error

This is caused by an IDA SDK macro that renames `wait` to `dont_use_wait`, corrupting C++23 `<atomic>` headers. The fix is applied automatically during CMake configure — if it persists, verify that `cmake.toml` contains the `sdk_bridge.hpp` patch block in the `[target.ApplyCalleeTypeEx]` section.

---

## License

MIT

## References

- [idax — A beautiful, idiomatic IDA C++ SDK](https://github.com/19h/idax)
- [cmkr — CMake with TOML](https://cmkr.build/)
- [ida-cmake — IDA SDK CMake integration](https://github.com/allthingsida/ida-cmake)
- [IDA Pro SDK](https://hex-rays.com/products/ida/support/idadoc/)

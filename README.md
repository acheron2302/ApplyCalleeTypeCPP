# ApplyCalleTypeCpp

Apply callee type to indirect CALL instructions in IDA Pro. Press **Shift+A** on a `call` instruction to assign a function prototype from manual input, TIL type libraries, or local database types.

Built with [idax](https://github.com/19h/idax) (C++23 IDA SDK wrapper), Qt 6.8.0 dialogs, and [cmkr](https://cmkr.build/).

---

## Features

- **Three type sources**: manual C-declaration input, TIL (type library) types, and local database types
- **Real-world input tolerant**: strips SAL annotations, `__declspec`, calling-convention macros, bracket annotations, and normalizes whitespace automatically
- **Windows-only CI**: MSVC builds for latest IDA SDK v9.2 and v9.3 tags via GitHub Actions
- **Auto-builds** for latest IDA SDK v9.2 and v9.3 tags

---

## Requirements

| Component | Version | Notes |
|-----------|---------|-------|
| **CMake** | 3.27+ | Build system |
| **C++ Compiler** | C++23 capable | MSVC 2022, GCC 14+, Clang 17+ |
| **IDA SDK** | 9.2 or 9.3 | From [HexRaysSA/ida-sdk](https://github.com/HexRaysSA/ida-sdk) |
| **ida-cmake** | latest | Cloned into IDA SDK root |
| **Qt 6** | 6.8.0 | Core, Gui, Widgets components. Headers and tools (moc) from public or IDA-compatible Qt. **Linking must use IDA SDK Qt import libs** from `idasdk/src/lib/x64_win_qt`. |
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

> **Important for Windows**: The final plugin must link against the IDA SDK Qt import libraries (`idasdk/src/lib/x64_win_qt/*.lib`) to import `QT_NAMESPACE=QT` symbols. Stock/public Qt builds will produce plugins that fail to load with `LoadLibraryA` error 127. The build system overrides Qt imported target library locations automatically on Windows.

Qt headers and tools (moc) can come from any Qt 6.8.0 installation. Set the `Qt6_DIR` CMake variable to point to your Qt 6.8.0 build. You can either pass it on the CMake command line or set it in `cmake.toml`.

The `cmake.toml` Qt6_DIR logic prefers `Qt6_DIR`/`CMAKE_PREFIX_PATH` from the caller (CI or command line) and falls back to a local hard-coded path near line 73.

```bash
cmake -B build -DQt6_DIR=C:/your-qt-build/lib/cmake/Qt6
# or
cmake -B build -DCMAKE_PREFIX_PATH=C:/your-qt-build
```

### 3. Build

```bash
cmake -P cmkr.cmake    # Generate CMakeLists.txt from cmake.toml
cmake -B build         # Configure
cmake --build build    # Compile
```

The plugin output goes to `$IDABIN/plugins/` (set via the `IDABIN` environment variable, defaults to SDK plugin directory).

### 4. Install in IDA Pro

Copy the built plugin to your IDA Pro plugins directory:

```bash
# Windows
copy build\Release\ApplyCalleTypeCpp.dll "%IDA_DIR%\plugins\"
```

---

## Usage

1. Open a database in IDA Pro
2. Place the cursor on an indirect `call` instruction
3. Press **Shift+A** (or Edit → Operand type → ApplyCalleTypeCpp)
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
- **Platforms**: Windows (MSVC) only
- **Qt**: 6.8.0 — public Qt headers/tools via `jurplel/install-qt-action`, IDA SDK-provided Qt import libs from `idasdk/src/lib/x64_win_qt` per SDK tag
- **Artifacts**: `.dll` files uploaded per build
- **Releases**: auto-published on push to `main`/`master` (not on PRs)

---

## Project Structure

```
ApplyCalleTypeCpp/
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
$env:IDASDK = "C:\path\to\ida-sdk"    # Windows PowerShell
```

### `Could not find ida-cmake/bootstrap.cmake`

Clone ida-cmake into your SDK:

```bash
cd $IDASDK
git clone https://github.com/allthingsida/ida-cmake.git ida-cmake
```

### `find_package(Qt6) fails`

Either set `Qt6_DIR` (see Setup step 2) or pass it to CMake:

```bash
cmake -B build -DQt6_DIR=/path/to/Qt/6.8.0/win64_msvc2022_64/lib/cmake/Qt6
```

### `LoadLibraryA` fails with error 127 on Windows

Error 127 means a dependent DLL was found but an imported procedure was missing. For this plugin, the cause is linking against stock/public Qt import libraries instead of IDA SDK Qt import libraries (`idasdk/src/lib/x64_win_qt/*.lib`).

The build system automatically overrides Qt imported target library locations on Windows with the IDA SDK-provided `.lib` files. Verify:

1. The IDA SDK checkout contains `idasdk/src/lib/x64_win_qt/Qt6Core.lib`, `Qt6Gui.lib`, and `Qt6Widgets.lib`.
2. The plugin was compiled with `QT_NAMESPACE=QT` (set automatically by cmake.toml on Windows).
3. Check the plugin imports:

```
dumpbin /IMPORTS ApplyCalleTypeCpp.dll
```

A working build should import namespaced Qt symbols containing `@QT@@`, for example `?fromLatin1@QString@QT@@...`. A broken build imports non-namespaced symbols such as `?fromLatin1@QString@@...` without `@QT@@`.

To fix: ensure the build uses `idasdk/src/lib/x64_win_qt/*.lib` Qt import libraries and `QT_NAMESPACE=QT`.

### Type parsing fails

- Ensure the input is a function declaration or function pointer typedef
- Check that SAL annotations and calling conventions are stripped (preprocessing handles most common patterns)
- Very long declarations (>4096 chars) are truncated

### `std::atomic::dont_use_wait` build error

This is caused by an IDA SDK macro that renames `wait` to `dont_use_wait`, corrupting C++23 `<atomic>` headers. The fix is applied automatically during CMake configure — if it persists, verify that `cmake.toml` contains the `sdk_bridge.hpp` patch block in the `[target.ApplyCalleTypeCpp]` section.

---

## License

MIT

## References

- [idax — A beautiful, idiomatic IDA C++ SDK](https://github.com/19h/idax)
- [cmkr — CMake with TOML](https://cmkr.build/)
- [ida-cmake — IDA SDK CMake integration](https://github.com/allthingsida/ida-cmake)
- [IDA Pro SDK](https://hex-rays.com/products/ida/support/idadoc/)

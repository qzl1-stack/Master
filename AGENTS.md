# AGENTS.md

## Cursor Cloud specific instructions

### Project overview

JT Studio is a Qt6/C++17/QML desktop application (master controller / process orchestrator). It manages child processes, provides IPC via local sockets, and includes a plugin store and auto-update system. This is primarily a Windows-targeted application with cross-platform compilation support on Linux.

### Build environment

- **Qt 6.8.2** is installed at `/opt/Qt/6.8.2/gcc_64` via `aqtinstall`. Modules installed: base, declarative, qt5compat, qtshadertools.
- **CMake 3.28** and **g++ 13.3** are available system-wide.
- Use `g++` as the C++ compiler (the default `c++` alternative points to clang++ which lacks proper libstdc++ linkage).

### Build commands

```bash
mkdir -p build && cd build
cmake -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.2/gcc_64 ..
cmake --build . -j$(nproc)
```

### Running the application

The built binary is at `build/bin/JT_Studio`. Set these environment variables before running:

```bash
export LD_LIBRARY_PATH=/opt/Qt/6.8.2/gcc_64/lib:$LD_LIBRARY_PATH
export QML2_IMPORT_PATH=/opt/Qt/6.8.2/gcc_64/qml
export QT_PLUGIN_PATH=/opt/Qt/6.8.2/gcc_64/plugins
./build/bin/JT_Studio
```

### Lint

```bash
cppcheck --enable=warning,style,performance --suppress=missingIncludeSystem --suppress=unusedFunction --suppress=unknownMacro --std=c++17 -I src/ src/
```

Note: `clang-tidy` has header resolution issues due to g++/clang toolchain mismatch. Use `cppcheck` instead.

### Key gotchas

- **Platform guard in `CheckForUpdates()`**: The `MainController::CheckForUpdates()` function uses Windows API calls (`CreateProcessW`, etc.). A `#ifdef Q_OS_WIN` guard was added for Linux compilation. The existing `#include <windows.h>` at the top of the file already had this guard.
- **No automated tests**: The project has no test framework or test files.
- **Windows-specific features**: Window embedding (`SetParent`, `EnumWindows`), auto-update (PowerShell scripts), and shortcut creation (VBScript) are Windows-only and have proper `#ifdef Q_OS_WIN` guards.
- **External dependencies**: Plugin catalog and updates fetch from Alibaba Cloud OSS endpoints. These are optional for local development.

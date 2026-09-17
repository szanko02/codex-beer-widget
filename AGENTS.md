# Repository Guidelines

## Project Structure & Architecture

This Windows x64 application uses C++20, Win32, Direct2D, and DirectComposition. In `src/`, `codex_source.*` handles the Codex App Server protocol, `worker.*` handles background polling, `limit_model.hpp` models quotas, `renderer.*` draws the widget, and `app.cpp` manages window lifecycle. Settings and their UI live in `settings.*` and `settings_ui.*`. Keep blocking protocol work off the UI thread.

`tests/` contains model and protocol tests. `assets/icons/` contains embedded icons; `tools/` contains packaging, benchmarking, and asset generators. `docs/` records design, validation, and performance results. Generated builds go in `build/`, packages in `dist/`, and local measurements in `.local/`.

## Build, Test, and Development Commands

Use Visual Studio 2022 Build Tools, Windows SDK, and CMake 3.24+ from Developer PowerShell:

```powershell
cmake --preset windows-x64              # Configure; fetch pinned JSON dependency
cmake --build --preset release          # Build application, probe, and tests
ctest --preset release --no-tests=error  # Run model and protocol tests
./build/Release/CodexBeerWidget.exe --demo # Run without consuming quota
./tools/package.ps1                     # Create ZIP, EXE, and checksums
```

## Coding Style & Naming

Follow `.clang-format`: LLVM base, four-space indentation, attached braces, and 110-column limit. Format changed C++ files with `clang-format -i`. Match existing names: `PascalCase` types, `snake_case` functions and variables, trailing underscores for private members, and the `beer` namespace. Preserve RAII ownership and existing module boundaries.

## Testing & Performance

Tests are standalone C++ executables registered with CTest, using explicit checks rather than an external framework. Follow `tests/*_tests.cpp`; add regression checks to the relevant model or protocol suite. No numeric coverage threshold is configured.

For rendering changes, also verify the visible widget, hide/restore, and click-through behavior. Use `tools/performance-suite.ps1` for paired measurements; keep DPI, size, mode, and duration comparable. Report actual FPS, CPU, and memory together.

## Commits & Pull Requests

Use focused commits with existing prefixes such as `perf:`, `fix:`, and `docs:`. Work on `codex/<topic>` branches and target `develop`; release integration targets `main`. Describe behavior changes, validation, and material limitations; include screenshots for visual changes and measurements for optimization. Link relevant issues. Windows CI must build, test, and package successfully. Keep application version metadata consistent when bumping versions.

## Configuration & Secrets

Settings belong in `%LOCALAPPDATA%\CodexBeerWidget`. Never commit account credentials or local diagnostics containing private data. `CODEX_WIDGET_CODEX_PATH` must point to a trusted native Codex executable. Display remaining percentages, not inferred token counts.

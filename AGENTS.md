# Repository Guidelines

## Project Structure & Architecture

Windows x64; C++20, Win32, Direct2D, DirectComposition. `src/`: `codex_source.*` Codex App Server protocol; `worker.*` background polling; `limit_model.hpp` quotas; `renderer.*` drawing; `app.cpp` window lifecycle; `settings.*` settings; `settings_ui.*` settings UI. No blocking protocol work on UI thread.

`tests/`: model/protocol tests. `assets/icons/`: embedded icons. `tools/`: packaging, benchmarks, asset generators. `docs/`: design, validation, performance. Outputs: `build/` builds, `dist/` packages, `.local/` measurements.

## Build, Test, and Development Commands

Requires Visual Studio 2022 Build Tools, Windows SDK, CMake 3.24+; run in Developer PowerShell:

```powershell
cmake --preset windows-x64              # Configure; fetch pinned JSON dependency
cmake --build --preset release          # Build application, probe, and tests
ctest --preset release --no-tests=error  # Run model and protocol tests
./build/Release/CodexBeerWidget.exe --demo # Run without consuming quota
./tools/package.ps1                     # Create ZIP, EXE, and checksums
```

## Coding Style & Naming

`.clang-format`: LLVM, four-space indent, attached braces, 110-column limit. Changed C++: `clang-format -i`. Types `PascalCase`; functions/variables `snake_case`; private members trailing underscores; namespace `beer`. Preserve RAII ownership and module boundaries.

## Testing & Performance

Standalone C++ executables; CTest; explicit checks, no external framework. Follow `tests/*_tests.cpp`; add regressions to relevant model/protocol suite. No numeric coverage threshold.

Rendering changes: verify visible widget, hide/restore, click-through. Paired measurements: `tools/performance-suite.ps1`; comparable DPI, size, mode, duration. Report actual FPS, CPU, memory together.

## Commits & Pull Requests

Focused commits: `perf:`, `fix:`, `docs:`. Branch `codex/<topic>` → `develop`; releases → `main`. PR: behavior changes, validation, material limits, relevant issues; screenshots for visuals, measurements for optimization. Windows CI must pass build, tests, packaging. Version bumps: keep metadata consistent.

## Configuration & Secrets

Settings: `%LOCALAPPDATA%\CodexBeerWidget`. Never commit credentials or private diagnostics. `CODEX_WIDGET_CODEX_PATH`: trusted native Codex executable only. Display remaining percentages, not inferred token counts.

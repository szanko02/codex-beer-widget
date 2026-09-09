# Windows x64 build

Install Visual Studio 2022 Build Tools with Desktop development with C++, CMake tools and Windows SDK.
From a Developer PowerShell (or with the bundled CMake on PATH):

```powershell
cmake --preset windows-x64
cmake --build --preset release
ctest --preset release
./build/Release/CodexQuotaProbe.exe
```

CMake downloads nlohmann/json 3.12.0 from the upstream release and verifies the pinned SHA-256.
The native runtime is statically linked. No Node.js or Python is required by the native client.
`tools/probe-source.mjs` is an optional development measurement utility using an existing Node.js installation.

The probe discovers the native npm-installed Codex x64 executable, then `codex.exe` on PATH.
For another installation set `CODEX_WIDGET_CODEX_PATH` to the absolute path of a trusted native `codex.exe`.
PowerShell and CMD shims are not executed by the native client.

## Release package

```powershell
./tools/package.ps1
```

CPack creates `dist/CodexBeerWidget-0.1.0-windows-x64.zip` with the application,
README, validation documents and the nlohmann/json MIT license. The directory also contains
the standalone EXE and `SHA256SUMS.txt`. Use the ZIP for distribution so the license accompanies the EXE.
Tests, probes, Codex CLI, local account data and settings are excluded from the package.
GitHub Actions builds, runs CTest and packages the same configuration on Windows 2022.
Downloads are hash-pinned; a configured toolchain and network access for the initial dependency fetch are required.
This is a repeatable source build, not a promise of bit-identical binaries across different toolchains.

For a fresh build use `cmake -S . -B out/verify -G "Visual Studio 17 2022" -A x64`,
`cmake --build out/verify --config Release`, `ctest --test-dir out/verify -C Release --output-on-failure`,
then `./tools/package.ps1 -BuildDirectory out/verify -OutputDirectory dist/verify`.

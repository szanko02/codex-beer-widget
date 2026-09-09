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

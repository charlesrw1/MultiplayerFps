# CsRemake Launcher

A small Windows launcher for the local engine checkout.

Run it from the repository root:

```powershell
dotnet run --project Launcher/CsRemakeLauncher.csproj
```

Or, from the repository root:

```powershell
.\EngineLauncher.ps1
```

Choose **Debug** or **Release** in the launcher before building/running. Use the dedicated game/editor launch buttons; editor mode starts `App.exe --editor`. Enable **Lua debugger** to start the engine's `actboy168.lua-debug` listener at `127.0.0.1:9966`; **Wait for Lua attach** also pauses engine startup until VS Code connects. It builds through the installed 64-bit Visual Studio MSBuild, starts `App.exe`, runs the existing unit/integration test scripts, lists local engine instances, and can attach the running Visual Studio debugger. Build and full rebuild stop detected engine instances first and restart them after a successful build. Cache cleanup only removes the repo-local `.cache` and `ShaderCache` folders after confirmation.

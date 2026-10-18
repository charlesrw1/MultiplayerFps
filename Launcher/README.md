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

Choose **Debug** or **Release** in the launcher before building/running. Use the dedicated game/editor launch buttons; editor mode starts `App.exe --editor`. Set **L# debug** to **Debug** to start the LuaSharp script debugger on port 4711 (`-ls_debug 1`), or to **Wait For Debug** to also pause engine startup until VS Code attaches (`-ls_debug 2`). **Attach L# debugger** starts the VS Code LuaSharp attach session for the selected instance. It builds through the installed 64-bit Visual Studio MSBuild, starts `App.exe`, runs the existing unit/integration test scripts, lists local engine instances, and can attach the running Visual Studio debugger. Build and full rebuild stop detected engine instances first and restart them after a successful build. Cache cleanup only removes the repo-local `.cache` and `ShaderCache` folders after confirmation.

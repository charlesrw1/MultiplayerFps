# LuaSharp VS Code extension

Install dependencies in this folder with `npm install`, then use VS Code's **Install from VSIX** flow after packaging, or launch the extension development host from this folder.

Set these workspace settings to the built executables:

```json
{
  "luasharp.languageServerPath": "C:\\Users\\charl\\source\\repos\\CsRemake\\x64\\Debug\\LuaSharpLspNative.exe"
}
```

The extension associates `.ls` with LuaSharp and launches the native LuaSharp language server. It does not launch or depend on LuaLS.

## Debugging

The `luasharp` debug type attaches to a program that serves the LuaSharp debugger:

- `LuaSharp.exe run <file.ls> --debug [--port N]`
- the engine with `ls_debug 1` (EngineVars.ini, or `-ls_debug 1` on the command line). `ls_debug 2` also waits for VS Code before scripts load.

The URI `vscode://charl.luasharp/attach?port=N` also starts an attach session. The engine launcher uses it.

Start the program, then press F5. Without a `launch.json`, the extension attaches to port 4711. To use a different port, add this configuration:

```json
{ "type": "luasharp", "request": "attach", "name": "Attach to LuaSharp", "port": 4711 }
```

You can set breakpoints in `.ls` files, step, and examine the call stack and the locals. Runtime errors stop as exceptions. Visual Studio can stay attached to the engine at the same time.

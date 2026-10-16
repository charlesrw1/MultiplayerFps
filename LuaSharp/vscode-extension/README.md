# LuaSharp VS Code extension

Install dependencies in this folder with `npm install`, then use VS Code's **Install from VSIX** flow after packaging, or launch the extension development host from this folder.

Set these workspace settings to the built executables:

```json
{
  "luasharp.languageServerPath": "C:\\Users\\charl\\source\\repos\\CsRemake\\x64\\Debug\\LuaSharpLspNative.exe"
}
```

The extension associates `.ls` with LuaSharp and launches the native LuaSharp language server. It does not launch or depend on LuaLS.

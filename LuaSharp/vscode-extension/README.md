# LuaSharp VS Code extension

Install dependencies in this folder with `npm install`, then use VS Code's **Install from VSIX** flow after packaging, or launch the extension development host from this folder.

Set these workspace settings to the built executables:

```json
{
  "luasharp.languageServerPath": "C:\\Users\\charl\\source\\repos\\CsRemake\\x64\\Debug\\LuaSharpLsp.exe",
  "luasharp.luaLsPath": "C:\\Users\\charl\\.vscode\\extensions\\sumneko.lua-3.19.2-win32-x64\\server\\bin\\lua-language-server.exe"
}
```

The extension associates `.ls` with LuaSharp and launches the native proxy. The proxy transpiles open documents into virtual Lua and delegates language features to LuaLS.

const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");
const path = require("path");
const fs = require("fs");

let client;

function configuredPath(name, fallback) {
    const value = vscode.workspace.getConfiguration("luasharp").get(name);
    return value || fallback;
}

function activate(context) {
    const repositoryRoot = path.resolve(context.extensionPath, "..", "..");
    const defaultProxy = path.join(repositoryRoot, "x64", "Debug", "LuaSharpLsp.exe");
    const defaultLuaLs = path.join(process.env.USERPROFILE || "", ".vscode", "extensions", "sumneko.lua-3.19.2-win32-x64", "server", "bin", "lua-language-server.exe");
    const proxyPath = configuredPath("languageServerPath", defaultProxy);
    const luaLsPath = configuredPath("luaLsPath", defaultLuaLs);

    if (!fs.existsSync(proxyPath)) {
        vscode.window.showErrorMessage(`LuaSharp: proxy not found: ${proxyPath}`);
        return;
    }
    if (!fs.existsSync(luaLsPath)) {
        vscode.window.showErrorMessage(`LuaSharp: LuaLS not found: ${luaLsPath}`);
        return;
    }

    client = new LanguageClient(
        "luasharp",
        "LuaSharp",
        { command: proxyPath, args: [luaLsPath], transport: TransportKind.stdio },
        { documentSelector: [{ scheme: "file", language: "luasharp" }] }
    );
    context.subscriptions.push(client.start());
}

function deactivate() {
    return client ? client.stop() : undefined;
}

module.exports = { activate, deactivate };

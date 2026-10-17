const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");
const path = require("path");
const fs = require("fs");

let client;

function activate(context) {
    const output = vscode.window.createOutputChannel("LuaSharp");
    const workspaceRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath || "";
    const configured = vscode.workspace.getConfiguration("luasharp").get("languageServerPath");
    const serverPath = configured || path.join(workspaceRoot, "x64", "Debug", "LuaSharpLspNative.exe");
    output.appendLine(`Using native server: ${serverPath}`);
    if (!fs.existsSync(serverPath)) {
        vscode.window.showErrorMessage(`LuaSharp: language server not found: ${serverPath}`);
        return;
    }
    client = new LanguageClient(
        "luasharp", "LuaSharp",
        { command: serverPath, args: [], transport: TransportKind.stdio },
        { documentSelector: [{ scheme: "file", language: "luasharp" }], outputChannel: output }
    );
    client.start().then(async () => {
        output.appendLine("Native LuaSharp language server started.");
        const readDocument = async uri => ({ uri: uri.toString(), text: Buffer.from(await vscode.workspace.fs.readFile(uri)).toString("utf8") });
        const files = await vscode.workspace.findFiles("**/*.ls", "**/{.git,node_modules}/**");
        const documents = await Promise.all(files.map(readDocument));
        await client.sendNotification("luasharp/indexWorkspace", { documents });
        output.appendLine(`Indexed ${documents.length} LuaSharp file(s).`);
        // After the first full index, send only the file that changed.
        const indexDocument = async uri => client.sendNotification("luasharp/indexDocument", await readDocument(uri));
        const watcher = vscode.workspace.createFileSystemWatcher("**/*.ls");
        watcher.onDidCreate(indexDocument); watcher.onDidChange(indexDocument);
        watcher.onDidDelete(uri => client.sendNotification("luasharp/removeDocument", { uri: uri.toString() }));
        context.subscriptions.push(watcher);
    }, error => {
        output.appendLine(`Native LuaSharp language server failed: ${error.stack || error}`);
        vscode.window.showErrorMessage(`LuaSharp language server failed: ${error.message || error}`);
    });
    context.subscriptions.push(output, { dispose: () => client?.stop() });
}

function deactivate() { return client ? client.stop() : undefined; }
module.exports = { activate, deactivate };

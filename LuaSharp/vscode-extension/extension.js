const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");
const path = require("path");
const fs = require("fs");

let client;

const DefaultDebugPort = 4711;

// The debugger runs in the program (LuaSharp.exe --debug, or the engine). VS Code attaches to its DAP port.
function registerDebugger(context) {
    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory("luasharp", {
            createDebugAdapterDescriptor: session =>
                new vscode.DebugAdapterServer(session.configuration.port || DefaultDebugPort, session.configuration.host || "127.0.0.1"),
        }),
        // F5 without a launch.json attaches to the default port.
        vscode.debug.registerDebugConfigurationProvider("luasharp", {
            resolveDebugConfiguration: (folder, config) =>
                config.type ? config : { type: "luasharp", request: "attach", name: "Attach to LuaSharp", port: DefaultDebugPort },
        }),
        // vscode://charl.luasharp/attach?port=N starts an attach session (the engine launcher opens it).
        vscode.window.registerUriHandler({
            handleUri: uri => {
                if (uri.path !== "/attach") return;
                const port = Number(new URLSearchParams(uri.query).get("port")) || DefaultDebugPort;
                vscode.debug.startDebugging(vscode.workspace.workspaceFolders?.[0], { type: "luasharp", request: "attach", name: "Attach to LuaSharp", port });
            },
        }));
}

function activate(context) {
    registerDebugger(context);
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

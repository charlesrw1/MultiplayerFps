using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Xml.Linq;

namespace CsRemakeLauncher;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new LauncherForm(FindRepositoryRoot()));
    }

    private static string FindRepositoryRoot()
    {
        for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory is not null; directory = directory.Parent)
            if (File.Exists(Path.Combine(directory.FullName, "CsRemake.sln"))) return directory.FullName;
        for (var directory = new DirectoryInfo(Environment.CurrentDirectory); directory is not null; directory = directory.Parent)
            if (File.Exists(Path.Combine(directory.FullName, "CsRemake.sln"))) return directory.FullName;
        throw new InvalidOperationException("Could not find CsRemake.sln. Start the launcher from inside the repository.");
    }
}

internal sealed class LauncherForm : Form
{
    private readonly string _repoRoot;
    private readonly ComboBox _configuration = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 82 };
    private readonly CheckBox _launchWithDebugger = new() { Text = "Launch with debugger", AutoSize = true };
    private readonly CheckBox _luaDebugger = new() { Text = "Enable Lua debugger", AutoSize = true };
    private readonly CheckBox _waitForLuaDebugger = new() { Text = "Wait for Lua attach", AutoSize = true };
    private readonly ComboBox _integrationMode = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 82 };
    private readonly TextBox _integrationPattern = new() { PlaceholderText = "Test pattern (optional)", Width = 160 };
    private readonly ListView _instances = new() { Dock = DockStyle.Fill, FullRowSelect = true, GridLines = true, MultiSelect = false, View = View.Details };
    private readonly TextBox _output = new() { Dock = DockStyle.Fill, Font = new Font("Consolas", 9), Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Both, WordWrap = false };
    private readonly System.Windows.Forms.Timer _refreshTimer = new() { Interval = 1500 };
    private readonly List<Button> _operationButtons = [];
    private readonly Button _cancelBuild = new() { Text = "Cancel build", AutoSize = true, Enabled = false };
    private readonly Button _viewTestResults = new() { Text = "View failures", AutoSize = true, Enabled = false };
    private Button? _attachVisualStudio;
    private Button? _attachLuaDebugger;
    private Button? _restartSelected;
    private Button? _stopSelected;
    private readonly Dictionary<int, LaunchSettings> _knownInstances = [];
    private List<TestFailure> _testFailures = [];
    private TestResultsForm? _testResultsWindow;
    private Process? _activeBuildProcess;
    private bool _operationRunning;
    private static readonly Color GameTint = Color.FromArgb(224, 240, 252);
    private static readonly Color EditorTint = Color.FromArgb(239, 231, 250);

    public LauncherForm(string repoRoot)
    {
        _repoRoot = repoRoot;
        Text = "CsRemake Launcher";
        MinimumSize = new Size(610, 560);
        Size = new Size(900, 730);
        StartPosition = FormStartPosition.CenterScreen;

        _configuration.Items.AddRange(["Debug", "Release"]);
        _configuration.SelectedIndex = 0;
        _integrationMode.Items.AddRange(["All", "Game", "Editor"]);
        _integrationMode.SelectedIndex = 0;
        _integrationMode.SelectedIndexChanged += (_, _) => UpdateIntegrationModeColor();
        _waitForLuaDebugger.CheckedChanged += (_, _) => { if (_waitForLuaDebugger.Checked) _luaDebugger.Checked = true; };

        _instances.Columns.Add("Instance", 780);
        _instances.DoubleClick += async (_, _) => await AttachDebuggerAsync();
        _instances.SelectedIndexChanged += (_, _) => UpdateInstanceButtons();

        var buildControls = new WrappingFlowPanel();
        buildControls.Controls.Add(new Label { Text = "Config", AutoSize = true, Padding = new Padding(0, 6, 0, 0) });
        buildControls.Controls.Add(_configuration);
        buildControls.Controls.Add(AddOperationButton("Build", () => BuildAsync(false)));
        buildControls.Controls.Add(AddOperationButton("Full rebuild", () => BuildAsync(true)));
        _cancelBuild.Click += (_, _) => CancelBuild();
        buildControls.Controls.Add(_cancelBuild);
        var runGame = AddOperationButton("Run game", () => RunAppAsync(false));
        var runEditor = AddOperationButton("Run editor", () => RunAppAsync(true));
        TintButton(runGame, GameTint);
        TintButton(runEditor, EditorTint);
        buildControls.Controls.Add(runGame);
        buildControls.Controls.Add(runEditor);
        buildControls.Controls.Add(_launchWithDebugger);
        buildControls.Controls.Add(_luaDebugger);
        buildControls.Controls.Add(_waitForLuaDebugger);

        var testControls = new WrappingFlowPanel();
        testControls.Controls.Add(AddOperationButton("Unit tests", RunUnitTestsAsync));
        testControls.Controls.Add(AddOperationButton("Lua check", RunLuaCheckAsync));
        testControls.Controls.Add(new Label { Text = "Integration", AutoSize = true, Padding = new Padding(0, 6, 0, 0) });
        testControls.Controls.Add(_integrationMode);
        testControls.Controls.Add(_integrationPattern);
        testControls.Controls.Add(AddOperationButton("Run integration", RunIntegrationTestsAsync));
        _viewTestResults.Click += (_, _) => ShowTestResults();
        testControls.Controls.Add(_viewTestResults);
        testControls.Controls.Add(AddOperationButton("Clean caches", CleanCachesAsync));

        var instanceControls = new WrappingFlowPanel();
        _attachVisualStudio = AddOperationButton("Attach Visual Studio", AttachDebuggerAsync);
        _attachLuaDebugger = AddOperationButton("Attach Lua debugger", AttachLuaDebuggerAsync);
        _restartSelected = AddOperationButton("Restart selected", RestartSelectedAsync);
        _stopSelected = AddOperationButton("Stop selected", StopSelectedAsync);
        instanceControls.Controls.Add(_attachVisualStudio);
        instanceControls.Controls.Add(_attachLuaDebugger);
        instanceControls.Controls.Add(_restartSelected);
        instanceControls.Controls.Add(_stopSelected);
        var refresh = new Button { Text = "Refresh", AutoSize = true };
        refresh.Click += (_, _) => RefreshInstances();
        instanceControls.Controls.Add(refresh);
        var controls = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Top, Padding = new Padding(8), ColumnCount = 1, RowCount = 3 };
        controls.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        controls.Controls.Add(CreateSection("Build & launch", buildControls), 0, 0);
        controls.Controls.Add(CreateSection("Tests & maintenance", testControls), 0, 1);
        controls.Controls.Add(CreateSection("Selected instance", instanceControls), 0, 2);

        var top = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8) };
        top.Controls.Add(_instances);
        top.Controls.Add(new Label { Text = "Running engine instances", Dock = DockStyle.Top, Height = 23, Font = new Font(Font, FontStyle.Bold) });
        var bottom = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8) };
        bottom.Controls.Add(_output);
        bottom.Controls.Add(new Label { Text = "Build and test output", Dock = DockStyle.Top, Height = 23, Font = new Font(Font, FontStyle.Bold) });
        var split = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, SplitterDistance = 220 };
        split.Panel1.Controls.Add(top);
        split.Panel2.Controls.Add(bottom);
        Controls.Add(split);
        Controls.Add(controls);

        _refreshTimer.Tick += (_, _) => RefreshInstances();
        _refreshTimer.Start();
        FormClosed += (_, _) => _refreshTimer.Dispose();
        RefreshInstances();
        UpdateInstanceButtons();
        UpdateIntegrationModeColor();
        Log($"Repository: {_repoRoot}");
    }

    private Button AddOperationButton(string text, Func<Task> action)
    {
        var button = new Button { Text = text, AutoSize = true };
        button.Click += async (_, _) => await RunOperationAsync(action);
        _operationButtons.Add(button);
        return button;
    }

    private static void TintButton(Button button, Color color)
    {
        button.UseVisualStyleBackColor = false;
        button.BackColor = color;
    }

    private void UpdateIntegrationModeColor()
    {
        _integrationMode.BackColor = _integrationMode.SelectedIndex switch
        {
            1 => GameTint,
            2 => EditorTint,
            _ => SystemColors.Window
        };
    }

    private static Control CreateSection(string title, Control controls)
    {
        return new CompactSection(title, (FlowLayoutPanel)controls);
    }

    private async Task RunOperationAsync(Func<Task> action)
    {
        if (_operationRunning) return;
        _operationRunning = true;
        _operationButtons.ForEach(button => button.Enabled = false);
        try { await action(); }
        catch (Exception ex) { Log($"ERROR: {ex.Message}"); }
        finally
        {
            _operationRunning = false;
            _operationButtons.ForEach(button => button.Enabled = true);
            UpdateInstanceButtons();
            RefreshInstances();
        }
    }

    private string Config => (string)_configuration.SelectedItem!;
    private string OutputDirectory => Path.Combine(_repoRoot, "x64", Config);
    private string AppPath => Path.Combine(OutputDirectory, "App.exe");

    private async Task BuildAsync(bool fullRebuild)
    {
        var running = FindEngineProcesses();
        var restart = running.Select(CaptureSettings).ToList();
        if (restart.Count > 0)
        {
            Log($"Stopping {restart.Count} engine instance(s) before build...");
            foreach (var process in running)
            {
                await StopProcessAsync(process);
                process.Dispose();
            }
        }

        var target = fullRebuild ? "Rebuild" : "Build";
        var codegen = fullRebuild ? " /p:CodegenFullRebuild=true" : "";
        var arguments = $"\"{Path.Combine(_repoRoot, "CsRemake.sln")}\" /m /t:{target} /p:Configuration={Config} /p:Platform=x64 /p:BuildingInsideVisualStudio=true /p:PreferredToolArchitecture=x64 /v:minimal{codegen}";
        var exitCode = await RunConsoleProcessAsync(FindMsBuild(), arguments, isBuild: true);
        if (exitCode != 0)
        {
            Log($"{target} failed (exit {exitCode}); instances will not be restarted.");
            return;
        }

        foreach (var settings in restart)
        {
            if (!File.Exists(settings.Executable)) continue;
            Log($"Restarting {Path.GetFileName(settings.Executable)} as {settings.Mode} ({settings.Configuration}).");
            var process = StartEngineProcess(settings.Executable, settings.Arguments());
            _knownInstances[process.Id] = settings;
            if (settings.LaunchWithDebugger) await AttachDebuggerAsync(process);
        }
    }

    private Task RunAppAsync(bool editor)
    {
        if (!File.Exists(AppPath)) throw new FileNotFoundException("App.exe was not found. Build App first.", AppPath);
        var settings = new LaunchSettings(AppPath, Config, editor, _launchWithDebugger.Checked, _luaDebugger.Checked || _waitForLuaDebugger.Checked, _waitForLuaDebugger.Checked);
        var process = StartEngineProcess(settings.Executable, settings.Arguments());
        _knownInstances[process.Id] = settings;
        Log($"Started App.exe in {settings.Mode} mode (PID {process.Id}).");
        return settings.LaunchWithDebugger ? AttachDebuggerAsync(process) : Task.CompletedTask;
    }

    private async Task RunUnitTestsAsync() => await RunPowerShellScriptAsync("build_and_test.ps1", $"-Configuration {Config} -Platform x64");

    private async Task RunLuaCheckAsync()
    {
        var exitCode = await RunPowerShellScriptAsync("lua_check.ps1", "-Level Warning");
        if (exitCode == 1) Log("Lua check completed with diagnostics.");
    }

    private async Task RunIntegrationTestsAsync()
    {
        var arguments = $"-Config {Config}";
        if (_integrationMode.SelectedIndex is 1 or 2) arguments += $" -Mode {((string)_integrationMode.SelectedItem!).ToLowerInvariant()}";
        if (!string.IsNullOrWhiteSpace(_integrationPattern.Text)) arguments += $" -Pattern \"{_integrationPattern.Text.Replace("\"", "`\"")}\"";
        _viewTestResults.Enabled = false;
        await RunPowerShellScriptAsync("integration_test.ps1", arguments);
        _testFailures = ReadIntegrationFailures();
        _viewTestResults.Enabled = _testFailures.Count > 0;
        if (_testResultsWindow is { IsDisposed: false }) _testResultsWindow.RefreshFailures(_testFailures);
        if (_testFailures.Count > 0) Log($"Integration tests reported {_testFailures.Count} failure(s). Use View failures for details.");
    }

    private List<TestFailure> ReadIntegrationFailures()
    {
        var failures = new List<TestFailure>();
        foreach (var mode in new[] { "game", "editor" })
        {
            var path = Path.Combine(_repoRoot, "TestFiles", $"integration_{mode}_results.xml");
            if (!File.Exists(path)) continue;
            try
            {
                var document = XDocument.Load(path);
                foreach (var test in document.Descendants("testcase"))
                {
                    var failure = test.Element("failure");
                    if (failure is null) continue;
                    var message = (string?)failure.Attribute("message") ?? failure.Value;
                    var marker = "screenshot failed: ";
                    var start = message.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
                    var screenshot = start >= 0 ? message[(start + marker.Length)..].Trim().Split([' ', '\r', '\n'])[0] : null;
                    failures.Add(new TestFailure(mode, (string?)test.Attribute("name") ?? "Unnamed test", message, screenshot));
                }
            }
            catch (Exception ex) { Log($"Could not read {Path.GetFileName(path)}: {ex.Message}"); }
        }
        return failures;
    }

    private void ShowTestResults()
    {
        if (_testFailures.Count == 0) return;
        if (_testResultsWindow is { IsDisposed: false })
        {
            _testResultsWindow.RefreshFailures(_testFailures);
            _testResultsWindow.Activate();
            return;
        }
        _testResultsWindow = new TestResultsForm(_repoRoot, _testFailures);
        _testResultsWindow.FormClosed += (_, _) => _testResultsWindow = null;
        _testResultsWindow.Show(this);
    }

    private async Task CleanCachesAsync()
    {
        if (MessageBox.Show(this, "Remove every file and subfolder in this repository's .cache and ShaderCache directories?", "Clean caches", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) return;
        foreach (var directory in new[] { Path.Combine(_repoRoot, ".cache"), Path.Combine(_repoRoot, "ShaderCache") })
        {
            if (!Directory.Exists(directory)) continue;
            Log($"Removing all cached files: {directory}");
            await Task.Run(() => Directory.Delete(directory, recursive: true));
        }
        Log("Cache cleanup complete.");
    }

    private async Task AttachDebuggerAsync()
    {
        var process = SelectedProcess();
        if (process is not null) await AttachDebuggerAsync(process);
    }

    private async Task AttachDebuggerAsync(Process process)
    {
        Log($"Requesting Visual Studio debugger attach for PID {process.Id}...");
        var helper = Path.Combine(_repoRoot, "Scripts", "_vs_attach.ps1").Replace("'", "''");
        await RunConsoleProcessAsync("powershell.exe", $"-NoProfile -ExecutionPolicy Bypass -Command \"& {{ . '{helper}'; Attach-VSDebugger -TargetPid {process.Id} }}\"");
        if (_knownInstances.TryGetValue(process.Id, out var settings)) _knownInstances[process.Id] = settings with { LaunchWithDebugger = true };
    }

    private async Task AttachLuaDebuggerAsync()
    {
        var process = SelectedProcess();
        if (process is null) return;
        var settings = CaptureSettings(process);
        if (!settings.LuaDebugger)
        {
            Log("Lua debugging was not enabled for this instance. Relaunch it with 'Enable Lua debugger' selected.");
            return;
        }
        Log("Opening VS Code's lua-debug attach configuration (127.0.0.1:9966)...");
        await RunConsoleProcessAsync("code", $"--reuse-window \"{_repoRoot}\" --command workbench.action.debug.start");
    }

    private async Task StopSelectedAsync()
    {
        var process = SelectedProcess();
        if (process is not null) await StopProcessAsync(process);
    }

    private async Task RestartSelectedAsync()
    {
        var process = SelectedProcess();
        if (process is null) return;
        var settings = CaptureSettings(process);
        await StopProcessAsync(process);
        if (!File.Exists(settings.Executable))
        {
            Log($"Cannot restart: {settings.Executable} no longer exists.");
            return;
        }
        var restarted = StartEngineProcess(settings.Executable, settings.Arguments());
        _knownInstances[restarted.Id] = settings;
        Log($"Restarted {Path.GetFileName(settings.Executable)} as {settings.Mode} ({settings.Configuration}), PID {restarted.Id}.");
        if (settings.LaunchWithDebugger) await AttachDebuggerAsync(restarted);
    }

    private Process? SelectedProcess()
    {
        if (_instances.SelectedItems.Count == 0) { Log("Select an engine instance first."); return null; }
        try { return Process.GetProcessById((int)_instances.SelectedItems[0].Tag!); }
        catch { Log("The selected instance has already exited."); return null; }
    }

    private async Task StopProcessAsync(Process process)
    {
        if (!process.HasExited)
        {
            Log($"Stopping {process.ProcessName}.exe (PID {process.Id})...");
            process.CloseMainWindow();
            if (!await Task.Run(() => process.WaitForExit(5000)))
            {
                Log($"{process.ProcessName}.exe did not close in time; terminating it.");
                process.Kill(entireProcessTree: true);
                await Task.Run(process.WaitForExit);
            }
        }
        _knownInstances.Remove(process.Id);
        process.Dispose();
    }

    private Process StartEngineProcess(string executable, string arguments) => Process.Start(new ProcessStartInfo(executable, arguments) { WorkingDirectory = _repoRoot, UseShellExecute = false }) ?? throw new InvalidOperationException($"Could not start {executable}.");

    private Task<int> RunPowerShellScriptAsync(string scriptName, string arguments) => RunConsoleProcessAsync("powershell.exe", $"-NoProfile -ExecutionPolicy Bypass -File \"{Path.Combine(_repoRoot, "Scripts", scriptName)}\" {arguments}");

    private async Task<int> RunConsoleProcessAsync(string fileName, string arguments, bool isBuild = false)
    {
        Log($"> {Path.GetFileName(fileName)} {arguments}");
        using var process = new Process { StartInfo = new ProcessStartInfo(fileName, arguments) { WorkingDirectory = _repoRoot, UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, CreateNoWindow = true } };
        process.OutputDataReceived += (_, e) => { if (e.Data is not null) Log(e.Data); };
        process.ErrorDataReceived += (_, e) => { if (e.Data is not null) Log(e.Data); };
        process.Start(); process.BeginOutputReadLine(); process.BeginErrorReadLine();
        if (isBuild)
        {
            _activeBuildProcess = process;
            _cancelBuild.Enabled = true;
        }
        await process.WaitForExitAsync();
        if (isBuild && ReferenceEquals(_activeBuildProcess, process))
        {
            _activeBuildProcess = null;
            _cancelBuild.Enabled = false;
        }
        Log($"Process exited with code {process.ExitCode}.");
        return process.ExitCode;
    }

    private string FindMsBuild()
    {
        var vswhere = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Microsoft Visual Studio", "Installer", "vswhere.exe");
        using var process = Process.Start(new ProcessStartInfo(vswhere, "-latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\amd64\\MSBuild.exe") { UseShellExecute = false, RedirectStandardOutput = true, CreateNoWindow = true }) ?? throw new FileNotFoundException("vswhere.exe was not found.", vswhere);
        var result = process.StandardOutput.ReadToEnd().Trim(); process.WaitForExit();
        return result.Split(["\r\n", "\n"], StringSplitOptions.RemoveEmptyEntries).FirstOrDefault(path => File.Exists(path)) ?? throw new FileNotFoundException("Could not locate 64-bit MSBuild through vswhere.");
    }

    private IReadOnlyList<Process> FindEngineProcesses()
    {
        var matches = new List<Process>();
        var outputRoot = Path.GetFullPath(Path.Combine(_repoRoot, "x64")) + Path.DirectorySeparatorChar;
        foreach (var name in new[] { "App", "SlimRenderApp" }) foreach (var process in Process.GetProcessesByName(name))
        {
            var path = GetProcessPath(process);
            if (path is not null && Path.GetFullPath(path).StartsWith(outputRoot, StringComparison.OrdinalIgnoreCase)) matches.Add(process); else process.Dispose();
        }
        return matches;
    }

    private LaunchSettings CaptureSettings(Process process)
    {
        if (_knownInstances.TryGetValue(process.Id, out var known)) return known;
        var commandLine = GetCommandLine(process.Id);
        var executable = GetProcessPath(process) ?? throw new InvalidOperationException("Could not resolve process executable.");
        var config = executable.Contains("\\Release\\", StringComparison.OrdinalIgnoreCase) ? "Release" : "Debug";
        var settings = new LaunchSettings(executable, config, HasArgument(commandLine, "--editor"), HasArgument(commandLine, "--wait-for-debugger") || IsDebuggerAttached(process), HasArgument(commandLine, "--lua_debug"), HasArgument(commandLine, "--lua_debug_wait"));
        _knownInstances[process.Id] = settings;
        return settings;
    }

    private static bool HasArgument(string commandLine, string argument) => commandLine.Contains(argument, StringComparison.OrdinalIgnoreCase);

    private static string GetCommandLine(int pid)
    {
        try
        {
            using var process = Process.Start(new ProcessStartInfo("powershell.exe", $"-NoProfile -Command \"(Get-CimInstance Win32_Process -Filter 'ProcessId = {pid}').CommandLine\"") { UseShellExecute = false, RedirectStandardOutput = true, CreateNoWindow = true });
            return process is null ? "" : process.StandardOutput.ReadToEnd();
        }
        catch { return ""; }
    }

    private static string? GetProcessPath(Process process) { try { return process.MainModule?.FileName; } catch { return null; } }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CheckRemoteDebuggerPresent(IntPtr process, ref bool debuggerPresent);

    private static bool IsDebuggerAttached(Process process)
    {
        try { var present = false; return CheckRemoteDebuggerPresent(process.Handle, ref present) && present; }
        catch { return false; }
    }

    private void RefreshInstances()
    {
        var selectedPid = _instances.SelectedItems.Count == 1 ? (int?)_instances.SelectedItems[0].Tag : null;
        _instances.BeginUpdate(); _instances.Items.Clear();
        foreach (var process in FindEngineProcesses().OrderBy(process => process.ProcessName).ThenBy(process => process.Id))
        {
            var settings = CaptureSettings(process);
            var details = new List<string> { settings.Mode, settings.Configuration };
            if (IsDebuggerAttached(process)) details.Add("Debugger");
            if (settings.LuaDebugger) details.Add(settings.WaitForLuaDebugger ? "Lua debugger (waiting)" : "Lua debugger");
            var item = new ListViewItem($"{process.ProcessName}.exe (PID {process.Id}) — {string.Join(", ", details)}")
            { Tag = process.Id, Selected = selectedPid == process.Id };
            _instances.Items.Add(item); process.Dispose();
        }
        _instances.EndUpdate();
        UpdateInstanceButtons();
    }

    private void UpdateInstanceButtons()
    {
        var selected = _instances.SelectedItems.Count == 1;
        var luaAvailable = false;
        if (selected)
        {
            try
            {
                using var process = Process.GetProcessById((int)_instances.SelectedItems[0].Tag!);
                luaAvailable = CaptureSettings(process).LuaDebugger;
            }
            catch { selected = false; }
        }
        if (_attachVisualStudio is not null) _attachVisualStudio.Enabled = !_operationRunning && selected;
        if (_restartSelected is not null) _restartSelected.Enabled = !_operationRunning && selected;
        if (_stopSelected is not null) _stopSelected.Enabled = !_operationRunning && selected;
        if (_attachLuaDebugger is not null) _attachLuaDebugger.Enabled = !_operationRunning && selected && luaAvailable;
    }

    private void Log(string message)
    {
        if (IsDisposed) return;
        if (InvokeRequired) { BeginInvoke(() => Log(message)); return; }
        _output.AppendText($"[{DateTime.Now:HH:mm:ss}] {message}{Environment.NewLine}");
    }

    private void CancelBuild()
    {
        if (_activeBuildProcess is not { HasExited: false } process)
        {
            Log("There is no active build to cancel.");
            return;
        }
        Log("Cancelling build...");
        process.Kill(entireProcessTree: true);
    }

    private sealed record LaunchSettings(string Executable, string Configuration, bool Editor, bool LaunchWithDebugger, bool LuaDebugger, bool WaitForLuaDebugger)
    {
        public string Mode => Editor ? "Editor" : "Game";
        public string Arguments()
        {
            var arguments = new List<string>();
            if (Editor) arguments.Add("--editor");
            if (LaunchWithDebugger) arguments.Add("--wait-for-debugger");
            if (LuaDebugger) arguments.Add("--lua_debug");
            if (WaitForLuaDebugger) arguments.Add("--lua_debug_wait");
            return string.Join(' ', arguments);
        }
    }

}

internal sealed record TestFailure(string Mode, string Name, string Message, string? ScreenshotName);

internal sealed class WrappingFlowPanel : FlowLayoutPanel
{
    public WrappingFlowPanel()
    {
        AutoSize = false;
        WrapContents = true;
        Margin = new Padding(0);
    }
}

internal sealed class CompactSection : Panel
{
    private readonly Label _title;
    private readonly FlowLayoutPanel _content;
    private bool _layouting;

    public CompactSection(string title, FlowLayoutPanel content)
    {
        Dock = DockStyle.Top;
        Margin = new Padding(0, 0, 0, 5);
        _title = new Label { Text = title, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold), Height = 20 };
        _content = content;
        Controls.Add(_content);
        Controls.Add(_title);
    }

    protected override void OnLayout(LayoutEventArgs levent)
    {
        base.OnLayout(levent);
        if (_layouting || ClientSize.Width <= 0) return;
        _layouting = true;
        try
        {
            _title.SetBounds(0, 0, ClientSize.Width, 20);
            _content.SetBounds(0, _title.Bottom, ClientSize.Width, Math.Max(1, Height - _title.Height));
            var contentHeight = _content.GetPreferredSize(new Size(_content.ClientSize.Width, 0)).Height;
            _content.Height = Math.Max(26, contentHeight);
            Height = _title.Height + _content.Height + 3;
        }
        finally { _layouting = false; }
    }
}

internal sealed class TestResultsForm : Form
{
    private readonly string _repoRoot;
    private readonly List<TestFailure> _failures;
    private readonly ListView _failuresView = new() { Dock = DockStyle.Fill, FullRowSelect = true, View = View.Details };
    private readonly TextBox _message = new() { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical };
    private readonly ZoomableImageView _screenshot = new() { Dock = DockStyle.Fill };
    private readonly Label _screenshotCaption = new() { Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft };
    private readonly Button _toggleScreenshot = new() { Text = "Show actual", Width = 120, Dock = DockStyle.Right, Enabled = false };
    private readonly Button _promoteSelected = new() { Text = "Promote this", Width = 110, Dock = DockStyle.Right, Enabled = false };
    private readonly Button _promoteAll = new() { Text = "Promote all", Width = 105, Dock = DockStyle.Right, Enabled = false };
    private string? _goldenPath;
    private string? _actualPath;
    private bool _showingGolden = true;

    public TestResultsForm(string repoRoot, List<TestFailure> failures)
    {
        _repoRoot = repoRoot;
        _failures = failures;
        Text = $"Integration test failures ({failures.Count})";
        MinimumSize = new Size(650, 520);
        Size = new Size(900, 760);
        StartPosition = FormStartPosition.CenterParent;
        _failuresView.Columns.Add("Mode", 70);
        _failuresView.Columns.Add("Test", 370);
        _failuresView.Columns.Add("Type", 160);
        _failuresView.SelectedIndexChanged += (_, _) => UpdateSelection();
        _toggleScreenshot.Click += (_, _) => { _showingGolden = !_showingGolden; ShowSelectedImage(preserveView: true); };
        _promoteSelected.Click += (_, _) => PromoteSelected();
        _promoteAll.Click += (_, _) => PromoteAll();

        var details = new SplitContainer { Dock = DockStyle.Top, Orientation = Orientation.Horizontal, Height = 98, SplitterDistance = 50, IsSplitterFixed = true };
        details.Panel1.Controls.Add(_message);
        var screenshotHeader = new Panel { Dock = DockStyle.Top, Height = 34, Padding = new Padding(8, 4, 8, 4) };
        screenshotHeader.Controls.Add(_screenshotCaption);
        screenshotHeader.Controls.Add(_toggleScreenshot);
        screenshotHeader.Controls.Add(_promoteSelected);
        screenshotHeader.Controls.Add(_promoteAll);
        var lower = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8) };
        lower.Controls.Add(_screenshot);
        lower.Controls.Add(screenshotHeader);
        lower.Controls.Add(details);
        var split = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, SplitterDistance = 230 };
        split.Panel1.Controls.Add(_failuresView);
        split.Panel2.Controls.Add(lower);
        Controls.Add(split);
        RefreshFailures(failures);
    }

    public void RefreshFailures(IEnumerable<TestFailure> failures)
    {
        _failures.Clear();
        _failures.AddRange(failures);
        _failuresView.BeginUpdate();
        _failuresView.Items.Clear();
        foreach (var failure in _failures)
        {
            var item = new ListViewItem([failure.Mode, failure.Name, failure.ScreenshotName is null ? "Test failure" : "Screenshot failure"]) { Tag = failure };
            item.BackColor = failure.Mode.Equals("game", StringComparison.OrdinalIgnoreCase) ? Color.FromArgb(224, 240, 252) : Color.FromArgb(239, 231, 250);
            _failuresView.Items.Add(item);
        }
        _failuresView.EndUpdate();
        Text = _failures.Count == 0 ? "Integration test failures — no failures" : $"Integration test failures ({_failures.Count})";
        if (_failuresView.Items.Count > 0) _failuresView.Items[0].Selected = true;
        else UpdateSelection();
    }

    private TestFailure? SelectedFailure => _failuresView.SelectedItems.Count == 1 ? _failuresView.SelectedItems[0].Tag as TestFailure : null;

    private void UpdateSelection()
    {
        var failure = SelectedFailure;
        _message.Text = failure?.Message ?? "";
        _showingGolden = true;
        _goldenPath = null;
        _actualPath = null;
        if (failure?.ScreenshotName is not null)
        {
            _goldenPath = Path.Combine(_repoRoot, "TestFiles", "goldens", failure.ScreenshotName + ".png");
            _actualPath = Path.Combine(_repoRoot, "TestFiles", "screenshots", failure.ScreenshotName + "_actual.png");
        }
        _toggleScreenshot.Enabled = _goldenPath is not null && File.Exists(_goldenPath) && _actualPath is not null && File.Exists(_actualPath);
        _promoteSelected.Enabled = _toggleScreenshot.Enabled;
        _promoteAll.Enabled = GetPromotableFailures().Count > 0;
        ShowSelectedImage();
    }

    private List<(TestFailure Failure, string Golden, string Actual)> GetPromotableFailures()
    {
        var items = new List<(TestFailure, string, string)>();
        foreach (var failure in _failures.Where(failure => failure.ScreenshotName is not null))
        {
            var golden = Path.Combine(_repoRoot, "TestFiles", "goldens", failure.ScreenshotName! + ".png");
            var actual = Path.Combine(_repoRoot, "TestFiles", "screenshots", failure.ScreenshotName + "_actual.png");
            if (File.Exists(actual)) items.Add((failure, golden, actual));
        }
        return items;
    }

    private void PromoteSelected()
    {
        if (_goldenPath is null || _actualPath is null || !File.Exists(_actualPath)) return;
        var failure = SelectedFailure;
        if (MessageBox.Show(this, $"Replace the golden screenshot for '{failure?.Name}' with the current test capture?\n\nThis changes the checked-in baseline.", "Promote screenshot", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) return;
        Directory.CreateDirectory(Path.GetDirectoryName(_goldenPath)!);
        File.Copy(_actualPath, _goldenPath, overwrite: true);
        _showingGolden = true;
        ShowSelectedImage(preserveView: true);
    }

    private void PromoteAll()
    {
        var screenshots = GetPromotableFailures();
        if (screenshots.Count == 0) return;
        if (MessageBox.Show(this, $"Replace {screenshots.Count} golden screenshot baseline(s) with the current test captures?\n\nThis changes checked-in files and should only be done after reviewing the failures.", "Promote all screenshots", MessageBoxButtons.OKCancel, MessageBoxIcon.Warning) != DialogResult.OK) return;
        foreach (var (_, golden, actual) in screenshots)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(golden)!);
            File.Copy(actual, golden, overwrite: true);
        }
        _showingGolden = true;
        UpdateSelection();
    }

    private void ShowSelectedImage(bool preserveView = false)
    {
        var path = _showingGolden ? _goldenPath : _actualPath;
        if (path is null || !File.Exists(path))
        {
            _screenshot.Image = null;
            _screenshotCaption.Text = SelectedFailure?.ScreenshotName is null ? "Select a screenshot failure to view its image." : "Golden or actual screenshot was not found.";
            return;
        }
        using var source = Image.FromFile(path);
        _screenshot.SetImage(new Bitmap(source), resetView: !preserveView);
        _screenshotCaption.Text = _showingGolden ? "Golden baseline — scroll over image to zoom" : "Actual test capture — scroll over image to zoom";
        _toggleScreenshot.Text = _showingGolden ? "Show actual" : "Show golden";
    }
}

internal sealed class ZoomableImageView : Control
{
    private Image? _image;
    private float _zoom = 1f;
    private PointF _pan;
    private bool _panning;
    private Point _lastPanPoint;

    public ZoomableImageView()
    {
        DoubleBuffered = true;
        BackColor = Color.FromArgb(28, 28, 28);
        TabStop = true;
    }

    public Image? Image
    {
        get => _image;
        set
        {
            SetImage(value, resetView: true);
        }
    }

    public void SetImage(Image? image, bool resetView)
    {
        _image?.Dispose();
        _image = image;
        if (resetView)
        {
            _zoom = 1f;
            _pan = PointF.Empty;
        }
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        if (_image is null) return;
        var size = new SizeF(_image.Width * _zoom, _image.Height * _zoom);
        var origin = new PointF((ClientSize.Width - size.Width) / 2f + _pan.X, (ClientSize.Height - size.Height) / 2f + _pan.Y);
        e.Graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
        e.Graphics.DrawImage(_image, new RectangleF(origin, size));
    }

    protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); Focus(); }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button != MouseButtons.Right || _image is null) return;
        _panning = true;
        _lastPanPoint = e.Location;
        Capture = true;
        Cursor = Cursors.SizeAll;
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (!_panning) return;
        _pan = new PointF(_pan.X + e.X - _lastPanPoint.X, _pan.Y + e.Y - _lastPanPoint.Y);
        _lastPanPoint = e.Location;
        Invalidate();
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        if (e.Button != MouseButtons.Right) return;
        _panning = false;
        Capture = false;
        Cursor = Cursors.Default;
    }

    protected override void OnMouseWheel(MouseEventArgs e)
    {
        base.OnMouseWheel(e);
        if (_image is null) return;
        var oldZoom = _zoom;
        _zoom = Math.Clamp(_zoom * (e.Delta > 0 ? 1.2f : 1f / 1.2f), 0.1f, 16f);
        if (Math.Abs(oldZoom - _zoom) < 0.001f) return;
        var oldOrigin = ImageOrigin(oldZoom);
        var imagePoint = new PointF((e.X - oldOrigin.X) / oldZoom, (e.Y - oldOrigin.Y) / oldZoom);
        var newCenteredOrigin = CenteredOrigin(_zoom);
        _pan = new PointF(e.X - imagePoint.X * _zoom - newCenteredOrigin.X, e.Y - imagePoint.Y * _zoom - newCenteredOrigin.Y);
        Invalidate();
    }

    private PointF CenteredOrigin(float zoom) => new((ClientSize.Width - _image!.Width * zoom) / 2f, (ClientSize.Height - _image!.Height * zoom) / 2f);
    private PointF ImageOrigin(float zoom)
    {
        var centered = CenteredOrigin(zoom);
        return new PointF(centered.X + _pan.X, centered.Y + _pan.Y);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _image?.Dispose();
        base.Dispose(disposing);
    }
}

<#
.SYNOPSIS
    Stateless cdb.exe wrapper for inspecting a minidump from the CLI.

.DESCRIPTION
    Runs one or more cdb commands against the supplied .dmp and prints the
    result with prompt-echo, NatVis chatter, and symbol-loader spam stripped.
    Designed for AI agents that want to interrogate a crash dump ad-hoc.

    `.ecxr` is run automatically before the user's command so the current
    context starts at the faulting instruction (no-op on non-crash dumps).

.PARAMETER DumpPath
    Path to the .dmp file.

.PARAMETER Cmd
    A single cdb command string. Use `;` to chain multiple commands.
    Common queries:
        k                                 # stack of faulting thread
        kP 40                             # stack with parameters, 40 frames
        .frame 7                          # switch scope to frame 7
        dv /V                             # locals (addrs+types) in current frame
        dx -r2 @$curframe.LocalVariables  # locals with VALUES, deref 1 level
        dx -r2 @$curframe.Parameters      # parameters with VALUES
        dt -r2 MyType <addr>              # struct dump, 2 levels
        ?? expr                           # C++ expression: ?? this->m_x
        !analyze -v                       # MS auto-diagnosis (verbose)
        lm vm UnitTests                   # confirm PDB resolved
        ~* k                              # stacks of ALL threads
        u <addr>                          # disassemble at address

.EXAMPLE
    Scripts/dbg.ps1 crash.dmp "kP 40"

.EXAMPLE
    Scripts/dbg.ps1 crash.dmp ".frame 7; dx -r2 @`$curframe.LocalVariables"
#>
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$DumpPath,
    [Parameter(Mandatory = $true, Position = 1)][string]$Cmd,
    [string]$SymbolPath = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $DumpPath)) {
    Write-Error "Dump not found: $DumpPath"
    exit 1
}
$DumpPath = (Resolve-Path $DumpPath).Path
$dumpDir  = Split-Path $DumpPath -Parent

$candidates = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\cdb.exe",
    "${env:ProgramFiles}\Windows Kits\10\Debuggers\x64\cdb.exe",
    "${env:ProgramFiles(x86)}\Windows Kits\11\Debuggers\x64\cdb.exe",
    "${env:ProgramFiles}\Windows Kits\11\Debuggers\x64\cdb.exe"
)
$cdb = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cdb) {
    Write-Error "cdb.exe not found. Install Windows SDK 'Debugging Tools for Windows'."
    exit 2
}

if (-not $SymbolPath) {
    $RepoRoot = Split-Path $PSScriptRoot -Parent
    $cache = Join-Path $env:TEMP "symcache"
    $SymbolPath = "$RepoRoot\x64\Debug;$RepoRoot\x64\Release;$dumpDir;srv*$cache*https://msdl.microsoft.com/download/symbols"
}

# Run user command after .reload and .ecxr. The unique marker `@@@CMD@@@` lets
# us discard everything cdb prints before the user's command actually runs
# (symbol path validation, NatVis loading, etc.).
$preamble = ".symopt+0x40; .reload /f; .ecxr; .echo @@@CMD@@@"
$full = "$preamble; $Cmd; .echo @@@END@@@; q"

# `-c` runs the command string non-interactively. cdb still writes everything
# to stdout; we filter below.
$raw = & $cdb -z $DumpPath -y "$SymbolPath" -c $full -lines 2>&1 | Out-String
$lines = $raw -split "`r?`n"

$started = $false
$buf = New-Object System.Collections.Generic.List[string]
foreach ($l in $lines) {
    $s = $l -replace '^\d+:\d+>\s*', ''         # strip cdb prompt prefix
    if (-not $started) {
        if ($s -match '^@@@CMD@@@$') { $started = $true }
        continue
    }
    if ($s -match '^@@@END@@@$')           { break }
    if ($s -match '^\.(echo|reload|symopt|ecxr)\b') { continue }
    if ($s -match '^q$')                   { continue }
    if ($s -match '^N?atVis script ')      { continue }
    if ($s -match '^DBGHELP:|^SYMSRV:')    { continue }
    if ($s -match '^Loading unloaded')     { continue }
    $buf.Add($s)
}

# Collapse trailing blanks.
while ($buf.Count -gt 0 -and [string]::IsNullOrWhiteSpace($buf[$buf.Count - 1])) {
    $buf.RemoveAt($buf.Count - 1)
}

$buf -join "`n" | Write-Output

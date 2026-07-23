# Crash dumps

Post-mortem variable inspection for SEH crashes. Goal: an AI agent reading test output should be able to inspect locals, parameters, and dereferenced pointer state at crash time without launching a GUI debugger.

## Pipeline

1. `install_crash_handler()` (`Source/Framework/Util.cpp`) registers an SEH filter via `SetUnhandledExceptionFilter`. Called from `Source/App/main.cpp` and `Source/UnitTests/main.cpp`.
2. On an unhandled exception (access violation, divide-by-zero, etc.) the filter:
   - logs `[CRASH] code=... addr=...` to stderr + the engine log,
   - writes a minidump via `MiniDumpWriteDump` **before** symbolisation (dbghelp can deadlock inside the filter on some toolchains),
   - then logs the symbolised stack (function name + file:line, no raw addresses). Raw PCs are only printed as a fallback if `SymInitialize` fails — the minidump always has full addresses for `dbg.ps1`/cdb regardless.
3. Dump flags: `WithDataSegs | WithHandleData | WithThreadInfo | WithProcessThreadData | WithIndirectlyReferencedMemory | WithUnloadedModules`. The indirect-referenced flag is what makes pointer-from-locals dereferenceable in cdb.
4. Dump path derives from `set_assert_log_path()`: e.g. `Logs/test_game_output.log` → `Logs/test_game_output.dmp`. If unset, falls back to `crash_<pid>_<ticks>.dmp` in CWD.

## CLI tool: `Scripts/dbg.ps1`

A stateless cdb wrapper. Pass a dump path and one cdb command; it strips prompt echoes, NatVis chatter, and symbol-server noise, and prints clean output. Run as many times as needed — each call is independent.

```
Scripts/dbg.ps1 <dump.dmp> "<cdb command>"
```

Useful commands:

| Command | Effect |
|---|---|
| `kP 30` | Stack of faulting thread, 30 frames, parameters expanded |
| `.frame N; dx -r2 @$curframe.LocalVariables` | Locals at frame N **with values**, one level of pointer deref |
| `.frame N; dx -r2 @$curframe.Parameters` | Parameters at frame N with values |
| `.frame N; ?? expr` | C++ expression: `?? this->m_components`, `?? p->x` |
| `.frame N; dt -r2 TypeName <addr>` | Struct dump, 2 levels deep |
| `~* k` | Stacks of all threads |
| `u <addr>` | Disassemble at address |
| `lm vm modname` | Confirm module's PDB resolved |
| `!analyze -v` | Microsoft's automatic root-cause guess |

`.ecxr` is run automatically before each command, so the current frame already points at the faulting instruction.

### Example

```
> Scripts/dbg.ps1 crash_selftest.dmp ".frame 0; dx -r2 @`$curframe.LocalVariables"
@$curframe.LocalVariables
    marker_local     : -1059128626 [Type: int]
    marker_string    : 0x7ff... : "crash-self-test-marker" [Type: char *]
    payload          [Type: CrashSelfTestPayload]
        [+0x000] marker           : 48879 [Type: int]
        [+0x008] name             : 0x7ff... : "payload-name" [Type: char *]
    payload_ptr      : 0x8f... [Type: CrashSelfTestPayload *]
        [+0x000] marker           : 48879 [Type: int]
        [+0x008] name             : 0x7ff... : "payload-name" [Type: char *]
```

## Test-runner integration

`Scripts/integration_test.ps1` and `Scripts/build_and_test.ps1` automatically print a first-look summary (`kP 30` + frame-0 locals) for any `.dmp` produced by a failing run, and tell you the `dbg.ps1` invocation for deeper queries. No extra setup needed.

## Caveats

- **Release builds elide locals.** The optimizer puts most locals in registers that get clobbered. `dx` reports them as missing. Reproduce in Debug if locals are needed.
- **PDBs must match the binary.** They're emitted alongside the .exe by default (`<GenerateDebugInformation>true` is set for both configs). The script's symbol path includes `x64/Debug`, `x64/Release`, and the dump's directory.
- **`!analyze -v` is noisy.** Useful for the auto-root-cause guess, but it produces hundreds of lines. Prefer `kP` + `dx` for targeted inspection.
- **Async corruption.** If the heap is corrupted when the SEH filter runs, `MiniDumpWriteDump` can itself fault — `[CRASH] minidump write FAILED` will appear in the log instead.
- **Self-test:** `UnitTests.exe --crash-self-test` produces a deliberate crash in `Source/UnitTests/main.cpp:run_crash_self_test` for verifying the pipeline end-to-end.
- **`write_snapshot_minidump(path)`** is exposed for non-crash probe points. Note that snapshots park the calling thread in `NtGetContextThread` (ntdll) rather than user code, so they can't show user-frame locals — they're only useful for module/heap inspection. See `Source/UnitTests/crash_dump_smoke_test.cpp` for the pipeline regression test.

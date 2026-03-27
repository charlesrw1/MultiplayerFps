# Global Variable Inspector — Design Spec

**Date:** 2026-03-26
**Status:** Approved

## Overview

A standalone Python script (`global_inspector.py`) that analyzes a C++ source directory and reports, per file, which global variables are accessed — both directly and transitively via the call graph. Uses libclang for real AST parsing.

## Goals

- Per-file stats on direct and transitive global variable access
- Distinguish `static` (file-scope) vs non-static (linkage-visible) globals
- Best-effort: skip files that fail to parse, report errors and continue
- Skip `External/` directories by default (third-party code)
- Output: pretty-printed console summary + `globals_report.json`

## Non-Goals

- Does not require a build system or `compile_commands.json`
- Does not analyze class member variables (only file/namespace scope vars)
- Does not resolve macros that generate globals

## Architecture

Three passes over the source directory:

### Pass 1 — Global Collection
Parse every `.cpp` and `.h` (excluding `External/` and user-specified excludes). Walk the top-level AST of each translation unit. Collect all `VAR_DECL` cursors at file or namespace scope (not inside classes or functions). For each, record:
- `name`, `qualified_name`, `type`
- `file`, `line`
- `is_static` (whether it has internal linkage)

### Pass 2 — Function Analysis
For each function definition in `.cpp` files, walk its body and collect:
- **Direct global refs:** `DECL_REF_EXPR` cursors resolving to a known global
- **Calls made:** `CALL_EXPR` cursors resolved to their callee's qualified name

### Pass 3 — Transitive Closure
For each source file:
1. Union all direct globals from functions defined in that file → `direct_globals`
2. BFS over the call graph (cycle-safe) from those functions → `transitive_globals`

## Data Model (JSON Output)

```json
{
  "globals": {
    "qualified::name": {
      "name": "var_name",
      "type": "float",
      "file": "Source/Game/CarGame/CarGame.cpp",
      "line": 23,
      "is_static": true
    }
  },
  "files": {
    "Source/Game/CarGame/CarGame.cpp": {
      "direct_globals": ["qualified::name"],
      "transitive_globals": ["qualified::name", "other::global"],
      "functions": {
        "CarGame::update": {
          "direct_globals": ["qualified::name"],
          "calls": ["CarGame::tick"]
        }
      }
    }
  },
  "parse_errors": ["Source/Foo.cpp: <error message>"]
}
```

- `direct_globals` on a file = union of direct globals of all functions defined in that file
- `transitive_globals` = direct + all globals reachable via the call graph
- `parse_errors` = files libclang couldn't fully parse (non-fatal)

## CLI Interface

```
python global_inspector.py <src_dir> [options]

Options:
  --output FILE       Write JSON to FILE (default: globals_report.json)
  --exclude DIR       Additional directories to skip (repeatable)
  --no-pretty         Skip console pretty-print, only write JSON
  --include-headers   Also analyze .h files for function bodies (off by default)
```

Example:
```
python global_inspector.py Source/ --output report.json
```

## Console Output

Files sorted descending by transitive global count. Example:

```
=== Global Variable Access Report ===

Source/EngineMain.cpp  [direct: 4, transitive: 11]
  Direct:
    (non-static) eng — GameEngineLocal
    (non-static) g_pending_test_runner — ITestRunner*
    (static)     program_time_start — double
  Transitive (additional):
    (non-static) developer_mode — ConfigVar
    ...

parse errors: 2 files (see globals_report.json)
```

## Dependencies

- `pip install libclang`
- LLVM `libclang.dll` on Windows (from LLVM installer or bundled with clang)

## Error Handling

- Files that libclang fails to parse are skipped and added to `parse_errors`
- Unknown call targets (unresolved symbols) are silently ignored in the call graph
- Missing `libclang` produces a clear install message and exits

## Size Estimate

Target: under 1000 lines. Expected ~500-700 lines.

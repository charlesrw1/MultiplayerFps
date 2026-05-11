# `asset_cli` — asset-aware REPL

Interactive shell for browsing/editing the asset tree under `Data/`. Understands **asset groups** (a logical asset = all sibling files sharing a stem, e.g. `rock.glb` + `rock.cmdl` + `rock.dds` + `rock.tis`) and rewrites references on rename/move.

Launch: `py [[Scripts/asset_cli.py]] [asset_root]`. Default root = `./Data` (falls back to cwd if absent). History at `~/.asset_cli_history`. Tab-completion via `prompt_toolkit`; falls back to plain `input()` if no console buffer.

Backed by `asset_manager.AssetManager` (mutations) and `asset_types` (extension → `AssetType`, group_files).

## Asset model

Asset types recognised by extension (see `Scripts/asset_types.py`):

- **TEXTURE**: `.tis .dds .hdr .png .jpeg .jpg`
- **MODEL**: `.mis .cmdl .glb`
- **MAP**: `.tmap`
- **MATERIAL**: `.mm` (master) `.mi` (instance) `.glsl`

Asset group = stem shared by sibling files. `ls` / `find` collapse a group to one row; mutations (`mv`, `trash`) act on the whole group. Unknown extensions are ignored by group operations but still appear via shell escape.

**Valid reference formats** (only these get path-rewritten on `mv`): `.cmdl .dds .mm .mi .tmap`. Source formats (`.glb`, `.png`, …) are moved but not searched for inside other files.

## Commands

| Cmd | Form | Notes |
|---|---|---|
| `pwd` | `pwd` | current dir |
| `cd` | `cd <path>` | constrained to asset root |
| `ls` | `ls [path]` | groups assets; shows type column |
| `mkdir` | `mkdir <path>` | refuses paths escaping root |
| `cat` | `cat <file>` | reads as text |
| `cp` | `cp <src> <dst>` | single file only, no group semantics |
| `mv` | `mv <src> <dst>` | **moves whole asset group + rewrites refs**. Supports `*`/`?` wildcards; dst ending `/` → directory move |
| `trash` | `trash <name>` | deletes the whole asset group |
| `find` | `find <pattern>` | glob over asset names; 2-column output. Materials annotated `(master)`/`(instance)` based on `.mm`/`.mi` presence |
| `references` | `references <name>` | inbound refs via `rg -F` (must be installed); accepts filename or asset name; scoped to asset root |
| `undo` | `undo` | reverts the **single most recent** mutating op (`mv` `cp` `mkdir` `trash`). One slot only — subsequent op overwrites |
| `help` | `help [cmd]` | docstring dump |
| `exit`/`quit` | | |
| `!<cmd>` | `!Get-ChildItem` | runs PowerShell in cwd |

## `mv` reference rewriting

`AssetManager.mv` uses `_smart_replace` (`Scripts/asset_manager.py:62`): the old string is only substituted when surrounded by quotes, spaces, or string boundaries. Substrings inside larger paths are left alone — `rock` in `bedrock/foo` won't be touched. Edits to reference files are captured in the `UndoRecord` so `undo` restores byte-for-byte.

Wildcard `mv` (`mv pattern* dst/`) iterates `find_assets` results, accumulates per-asset undo records into one composite record, and reports the count + distinct touched-file count.

## Undo model

`AssetManager.undo` replays an `UndoRecord` (`Scripts/asset_manager.py` dataclasses `FileMove`/`ReferenceEdit`/`DeletedFile`). Only one record is retained on the CLI (`self._undo_record`); chained undo is **not** supported. After a successful `undo`, the slot is cleared.

## Gotchas

- `references` requires `rg.exe` on PATH; missing → `RuntimeError`.
- Tab completion is skipped when the target dir has > 100 entries (perf guard, `AssetCompleter.COMPLETION_THRESHOLD`).
- `cp` does **not** copy related files in the asset group — it's a literal single-file copy. Use `mv` for group semantics; there is no `cp`-group.
- `_smart_replace` won't touch references where the old path is embedded mid-token. If a tool writes refs without quoting/spacing, `mv` will silently miss them.
- Materials: `find` distinguishes master vs instance by probing for `.mm`/`.mi` in the resolved dir, not from the asset_group itself.

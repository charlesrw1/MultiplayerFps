# `docs` CLI — documentation harness

`docs.exe` validates wiki-style links across markdown and `@docs` refs in source code. Configured by `docs.toml` at project root (CLI walks up from cwd).

## Link syntax

**In markdown:**

```markdown
[[engine_overview#Build System]]   file + header
[[#Some Section]]                  same file
[[testing]]                        file only
[[rendering/materials#…]]          subfolder
[[Source/Render/MaterialLocal.cpp] source file (no header allowed)
```

- Path is relative to `docs/`, `.md` optional.
- Header match: case-insensitive, exact text (`Build-System` ≠ `Build System`). Fuzzy fallback for `locate`/`section`/`refs`/`context` only — `check` is strict.
- File part with no slash falls back to basename match. Multiple matches → `ambiguous_file` error.
- Non-`.md` extension = source file ref; must be in `[sources].paths` from `docs.toml`.

**In source code** — any line with `@docs` + wiki-link, any comment style:

```cpp
// @docs [[rendering/materials#Loading Flow]]
```

## Commands

| Command | Purpose |
|---|---|
| `docs check` | Validate all links + freshness of source refs. **Must exit 0 before commit.** |
| `docs bless [targets...]` | Record current source-file SHAs for wiki links from the named doc(s). `--all` blesses every tracked link. `--prune` drops manifest entries whose link no longer exists. |
| `docs locate <query>` | Find headers (substring → fuzzy). |
| `docs section <ref>` | Print section body + outbound links + inbound refs. File-only ref dumps whole file. |
| `docs refs <ref>` | Inbound refs only, one per line. |
| `docs context <path>` | For a source file: its `@docs` outbound refs + docs that mention the path. |

All commands accept `--json` for stable machine-readable output. Errors become `{ "ok": false, "error": { "kind": "...", "message": "..." } }`.

Fuzzy substitutions are reported on stderr as `fuzzy: '…' -> '…'`.

## Freshness

`docs check` flags doc → source wiki links whose source file has changed since the doc was last validated. State lives in `docs/.freshness.toml`, an auto-managed TOML array of validation entries:

```toml
[validation]]
doc = "testing.md"
source = "Scripts/build_and_test.ps1"
sha = "04a28d50..."   # git blob SHA of the working-tree file; sha256 fallback when not in git
```

Three warning states (still exit 0):

- `unblessed: <doc> -> <source>` — no manifest entry yet. Run `docs bless <doc>` after reading the source and confirming the doc is accurate.
- `stale: <doc> -> <source>` — source changed since the recorded SHA. Re-read source, update doc, re-bless.
- `orphan: <doc> -> <source>` — manifest entry references a doc or link that no longer exists. Clean with `docs bless --prune`.

Only links resolving to paths in `[sources].paths` (from `docs.toml`) participate — doc-to-doc links and external URLs are ignored.

`docs check --strict-freshness` promotes all three warnings to errors (for CI gating). Default behavior keeps them as warnings so they don't block commits.

## Agent workflow

1. **Before editing a source file** — `docs context <path>`. Read what's pinned.
2. **Before renaming a section/file** — `docs refs <ref>` to find dependents.
3. **Finding a topic** — `docs locate <query>`.
4. **Before commit** — `docs check` must exit 0.
5. **After verifying a doc against its source links** — `docs bless <doc>` to clear stale/unblessed warnings.
6. **Adding a new doc** — add one line to [[README]]; for internals docs, sprinkle `@docs` refs in relevant source headers.

## Notes

- Fenced code blocks are ignored for both headers and links.
- BOMs (UTF-8 / UTF-16 LE/BE) are auto-detected.
- The `#Section` same-file shortcut (no file part) works in markdown only; `@docs` refs always need the file part.

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
| `docs check` | Validate all links. **Must exit 0 before commit.** |
| `docs locate <query>` | Find headers (substring → fuzzy). |
| `docs section <ref>` | Print section body + outbound links + inbound refs. File-only ref dumps whole file. |
| `docs refs <ref>` | Inbound refs only, one per line. |
| `docs context <path>` | For a source file: its `@docs` outbound refs + docs that mention the path. |

All commands accept `--json` for stable machine-readable output. Errors become `{ "ok": false, "error": { "kind": "...", "message": "..." } }`.

Fuzzy substitutions are reported on stderr as `fuzzy: '…' -> '…'`.

## Agent workflow

1. **Before editing a source file** — `docs context <path>`. Read what's pinned.
2. **Before renaming a section/file** — `docs refs <ref>` to find dependents.
3. **Finding a topic** — `docs locate <query>`.
4. **Before commit** — `docs check` must exit 0.
5. **Adding a new doc** — add one line to [[README]]; for internals docs, sprinkle `@docs` refs in relevant source headers.

## Notes

- Fenced code blocks are ignored for both headers and links.
- BOMs (UTF-8 / UTF-16 LE/BE) are auto-detected.
- The `#Section` same-file shortcut (no file part) works in markdown only; `@docs` refs always need the file part.

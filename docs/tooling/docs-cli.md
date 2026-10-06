# Docs CLI

`docs.exe` is the wiki harness for `docs/`. `Scripts/check_codebase.ps1` runs `docs.exe check` as its first section.

```
docs [--json] {check,bless,locate,section,refs,context} ...
```

| Command | Purpose |
|---|---|
| `docs check` | Validate wiki-links and `@docs` references. Also check freshness of doc-to-source links. |
| `docs bless [targets...]` | Record current source-file hashes for wiki-links from docs. |
| `docs locate <query>` | Find sections by header text. |
| `docs section <file[#Header]>` | Show a section body, outbound links, and inbound refs. |
| `docs refs <file[#Header]>` | List references to a section or file. |
| `docs context <source-path>` | Show docs that a source file points to, and docs that mention it. |

`--json` emits machine-readable JSON.

## Check

```
docs check [--strict-freshness]
```

Errors fail the run. Freshness issues are warnings unless you pass `--strict-freshness`.

Place `@docs [[folder/page]]` or `@docs [[folder/page#Header]]` comments in source files that a page describes. Wiki-links from markdown use the same `[[path]]` / `[[path#Header]]` / `[[path|text]]` syntax.

## Freshness

`docs/.freshness.toml` stores source-file hashes for wiki-links from docs. `docs bless` writes this file. Do not edit it by hand.

| Status | Meaning |
|---|---|
| `stale` | The linked source file changed since the last bless. |
| `unblessed` | A wiki-link to source has no manifest entry. |
| `orphan` | A manifest entry has no matching wiki-link. |

By default these are warnings. Pass `--strict-freshness` to treat them as errors.

Bless one page, or every tracked link:

```
docs bless tooling/docs-cli.md
docs bless --all
docs bless --prune
```

`--prune` drops manifest entries whose link no longer exists.

See [[testing]] for how `check_codebase.ps1` uses this tool.

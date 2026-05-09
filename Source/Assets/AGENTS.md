# Assets Module — Agent Summary

Runtime asset loading/caching (`AssetDatabase`, all configs) plus an editor-only registry and hot-reload pipeline.

## FileWatcher (editor-only)

Background thread on `ReadDirectoryChangesW` (overlapped I/O). Emits forward-slash paths relative to the watched data dir. Debounce default 150 ms — coalesces multi-stage editor saves ("double-change" noise). Re-arms immediately after each completion so events aren't dropped. 64 KB buffer overflow is non-fatal: re-arm and continue; next write re-notifies. Thread-safe via `std::mutex`.

## AssetRegistrySystem (editor-only)

Singleton, polled per frame at the 150 ms debounce. Routes changed paths to typed reloads by extension:

- `.mm` / `.mi` → MaterialInstance
- `.mis` / `.glb` → Model
- `.png` / `.jpg` / `.tis` → Texture
- `.wav` → SoundFile
- `.lua` → ScriptManager

Owns the `AssetFilesystemNode` tree for the editor browser; reindex is rebuilt from `FileSys::find_game_files()` and committed in `post_load()`. Registers console commands `sys.ls`, `sys.print_deps`, `sys.print_refs`, `touch_asset`, `reload_asset`.

## AssetMetadata

Per-type browser metadata (color, display name, extensions, extra enumeration). Use `REGISTER_ASSETMETADATA_MACRO(Type)` for static auto-registration.

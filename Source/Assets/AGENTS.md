# Assets Module — Agent Summary

## Key Components

### FileWatcher (`FileWatcher.h` / `FileWatcher.cpp`)
- **Editor-only** (`#ifdef EDITOR_BUILD`).
- Background thread runs `ReadDirectoryChangesW` (overlapped I/O) on a directory tree.
- `init(dir)` — opens directory handle, starts the background thread.
- `poll(debounce_ms)` — returns forward-slash paths relative to the watched dir that have been stable for at least `debounce_ms` ms; removes them from the internal map. Thread-safe via `std::mutex`.
- Paths match game-path convention (forward slashes, relative to the data dir).
- **Debounce** (default 150 ms) deduplicates rapid multi-stage editor saves ("double-change" noise). The background thread re-arms `ReadDirectoryChangesW` immediately after each completion so no events are dropped between reads.
- Buffer overflow (64 KB exhausted) is handled gracefully: re-arm and continue; the next write to the same file will generate a new notification.

### AssetRegistrySystem (`AssetRegistry.h` / `AssetRegistry.cpp`)
- **Editor-only** (`#ifdef EDITOR_BUILD`).
- Singleton: `AssetRegistrySystem::get()`.
- `init()` — opens `g_project_base` dir with `FileWatcher`; calls `reindex_all_assets()`; registers console commands.
- `update()` — called each frame; calls `file_watcher_.poll(150)`, dispatches changed paths to `update_on_changed_paths()`, optionally calls `reindex_all_assets()`.
- `update_on_changed_paths(paths)` — triggers typed reloads: `.mm`/`.mi` → MaterialInstance, `.mis`/`.glb` → Model, `.png`/`.jpg`/`.tis` → Texture, `.wav` → SoundFile, `.lua` → ScriptManager.
- `reindex_all_assets()` — rebuilds the `AssetFilesystemNode` tree via `HackedAsyncAssetRegReindex`.
- Console commands: `sys.ls`, `sys.print_deps`, `sys.print_refs`, `touch_asset`, `reload_asset`.

### AssetDatabase (`AssetDatabase.h` / `AssetDatabase.cpp`)
- Runtime asset loading/caching, all build configs.
- `g_assets.find_sync<T>(path)`, `g_assets.find_sync_sptr<T>(path)`, `g_assets.reload_sync(asset)`, `g_assets.is_asset_loaded(path)`.

### AssetFilesystemNode / HackedAsyncAssetRegReindex
- `AssetFilesystemNode` — tree node for the in-editor asset browser; `sorted_list` for display ordering.
- `HackedAsyncAssetRegReindex` — rebuilds the node tree from `FileSys::find_game_files()`; `post_load()` commits the new tree to `AssetRegistrySystem`.

### AssetMetadata / AutoRegisterAsset
- `AssetMetadata` — abstract base for per-type browser metadata (color, display name, extensions, extra asset enumeration).
- `REGISTER_ASSETMETADATA_MACRO(Type)` — static auto-registration helper.

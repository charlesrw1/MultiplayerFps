# LevelEditor Module

In-editor level authoring: selection, ImGuizmo manipulation, property editing, outliner, undo/redo, level export. Compiled only when `EDITOR_BUILD` is defined.

## Concepts

- **EditorDocLocal** is the editor's `IEditorTool`. Fetched off `eng->get_tool()` (cast required); not a global. Owns the active level, `SelectionState`, and command history.
- **All mutations go through `ICommand`**. Direct property writes or transform changes always wrap in a command and `push_command(cmd)` so undo/redo (Ctrl+Z / Ctrl+Y) stays correct. Skipping the command path silently breaks undo.
- **Selection sync** — viewport, outliner, and property grid all read from one `SelectionState`. Change callbacks keep them aligned; do not cache selection elsewhere.
- **Property grid via reflection** — extends Framework `PropertyEd`, dispatching on `core_type_id` of `PropertyInfoList`. Game-specific widgets (asset picker for `AssetPtr<Model>`, `color32`, enum dropdowns) are registered in `PropertyEditors.cpp`.
- **Physics picking** — mouse-click casts a ray through the physics world; the hit `Render_Object::owner` resolves to the clicked entity.
- **Scene export** — `SceneExport` serializes via Framework `Serializer`. Components implementing `editor_compile()` bake data at export time (lightmaps, nav mesh, collision); call this before writing.

## Prefab editing mode

Double-clicking a `.tprefab` loads it into `EditorDoc` with `editing_prefab = true` and `[Prefab]` in the title. The save path comes from two interacting hooks: `set_document_path` records the file, and `save_document_internal` consults `get_save_file_extension()` to pick `.tprefab` vs `.tmap`. Both must agree or the editor will write back to the wrong format. `InstantiatePrefabCommand` and `MakePrefabFromSelectionCommand` are the undoable spawn/serialize paths.

## Gotchas

- ImGuizmo returns the updated matrix per frame; apply it via a `MoveCommand`, not by writing the transform directly (otherwise undo breaks mid-drag).
- Outliner re-parent is drag-and-drop; the icon column comes from each component's `get_editor_outliner_icon()`.

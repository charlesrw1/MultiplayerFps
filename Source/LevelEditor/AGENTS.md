# LevelEditor Module

In-editor level authoring tool: entity selection, 3D gizmo manipulation, property editing, outliner, undo/redo, and level export.

Compiled only when `EDITOR_BUILD` is defined.

## Key Files

- `EditorDocLocal.h/cpp` — Main editor document and tool state
- `Commands.h/cpp` — Command pattern for undo/redo
- `PropertyEditors.h/cpp` — Custom ImGui editors for specific property types
- `ObjectOutliner.cpp` — Scene hierarchy tree view
- `ObjectOutlineFilter.h` — Outliner search/filter
- `SelectionState.h` — Selected entity/component tracking
- `SceneExport.cpp` — Export level to runtime format

## Key Classes

### `EditorDocLocal` (EditorDocLocal.h)
Main editor document. Inherits `IEditorTool`.
- Owns the active level, selection state, and command history
- Integrates with:
  - **ImGuizmo** — 3D translate/rotate/scale gizmos drawn in the viewport
  - **Level / Entity system** — Creates, deletes, and modifies entities and components
  - **Physics system** — Used for mouse-pick ray casts to select objects
  - **Asset registry / browser** — Drag-and-drop asset assignment
  - **PropertyEd** — Renders property grid for selected entity/component
  - **Commands** — All mutations go through the command system for undo/redo
- Key responsibilities:
  - Viewport interaction (click to select, drag to transform)
  - Multi-select with Ctrl+click
  - Gizmo mode toggle (W/E/R for translate/rotate/scale)
  - Property panel showing reflected properties of selection
  - Outliner integration (click in outliner selects in viewport and vice versa)

### Command System (`Commands.h`)
Undo/redo stack using the command pattern.
- `ICommand` — Interface with `execute()` and `undo()` pure virtuals
- Commands wrap mutations: move entity, change property value, add/remove component, etc.
- `EditorDocLocal` calls `push_command(cmd)` to execute and record
- Ctrl+Z / Ctrl+Y undo/redo

### `SelectionState` (SelectionState.h)
Tracks the current editor selection.
- Stores selected `Entity*` list and optionally a focused `Component*`
- Fires change callbacks when selection changes
- Used by both the outliner and viewport to stay in sync

### `PropertyEditors` (PropertyEditors.h/cpp)
Custom ImGui editors registered for specific reflected types.
- Extends the generic `PropertyEd` from Framework with game-specific widgets
- Examples: asset picker for `AssetPtr<Model>`, color picker for `color32`, enum dropdowns

### Object Outliner (`ObjectOutliner.cpp`)
ImGui tree view of all entities in the level.
- Displays entity hierarchy (parent/child indentation)
- Shows entity name, tag, and per-component icons (via `get_editor_outliner_icon()`)
- `ObjectOutlineFilter` — Text search filter; hides non-matching entities
- Click selects entity; Ctrl+click multi-selects
- Drag-and-drop to re-parent entities

### Scene Export (`SceneExport.cpp`)
Serializes the current level to the runtime level format.
- Uses Framework `Serializer` (binary or JSON) to write entity/component state
- Triggers `editor_compile()` on components that need pre-export baking (e.g., lightmaps, nav mesh)

## Key Concepts

- **`EDITOR_BUILD`** — All editor code is guarded by this preprocessor define; excluded from game/test builds
- **ImGuizmo integration** — Gizmos are drawn each frame over the scene; `ImGuizmo::Manipulate()` returns the updated matrix which `EditorDocLocal` applies via a `MoveCommand`
- **All mutations go through commands** — Direct property writes or entity moves always create and push an `ICommand`; this ensures undo always works
- **Property grid via reflection** — `PropertyEd` iterates `PropertyInfoList` of the selected object, rendering appropriate widgets per `core_type_id`; custom type editors registered in `PropertyEditors.cpp` override defaults
- **Physics picking** — Mouse-click in viewport casts a ray through the physics world; the hit `Render_Object::owner` component identifies the clicked entity
- **`editor_compile()`** — Components can implement this to bake data at export time (lightmaps, collision meshes, etc.)

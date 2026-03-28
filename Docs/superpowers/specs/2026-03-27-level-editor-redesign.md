# Level Editor Redesign — Design Spec
**Date:** 2026-03-27
**Status:** Approved for implementation

---

## Goals

- **Modular and decoupled** — working on any one subsystem (outliner, a command, a tool) requires no knowledge of the rest of the editor
- **Integration testable** — each module can be constructed and exercised with a stub `IEditorAPI`, no full editor spin-up required
- **Undo everything** — every mutation is undoable by construction; spotty coverage is architecturally impossible
- **Same features** — selection, gizmo manipulation, property editor, outliner, undo/redo, save/load/export, drag-and-drop, camera control, foliage paint, decal stamp

---

## Architecture Overview

`EditorDoc` becomes a thin orchestrator that owns, constructs, and wires all modules together. It implements `IEditorAPI`. Nothing outside `EditorDoc` ever sees or depends on `EditorDoc` — they depend only on `IEditorAPI` or specific module references.

```
┌─────────────────────────────────────────────────────┐
│                    IEditorAPI                       │
│  (single public surface: commands, selection,       │
│   entity mutation, viewport input)                  │
└──────────────┬──────────────────────────────────────┘
               │ implemented by
┌──────────────▼──────────────────────────────────────┐
│                    EditorDoc                        │
│  (thin orchestrator — wires modules, owns nothing   │
│   except the module instances themselves)           │
│                                                     │
│  UndoRedoSystem    SelectionState    EditorCamera   │
│  ManipulateTool    ObjectOutliner    EdPropertyGrid  │
│  Tools (enum)      DragDropPreview   EditorInputs   │
└─────────────────────────────────────────────────────┘
```

External tools, commands, UI panels, and future plugins all take `IEditorAPI&` only.

---

## Core Interfaces (`IEditorAPI.h`)

Three narrow interfaces composed into one public API surface.

### `ICommandDispatcher`
The write side of the undo system. Every mutation goes through here.

```cpp
class ICommandDispatcher {
public:
    virtual void submit(std::unique_ptr<Command> cmd) = 0;
    virtual void command_group_start(std::string label) = 0;
    virtual void command_group_end() = 0;
    virtual void undo() = 0;
    virtual void redo() = 0;
};
```

### `ISceneQuery`
Read-only scene and selection access. No mutation.

```cpp
class ISceneQuery {
public:
    virtual const SelectionState& get_selection() const = 0;
    virtual Entity* get_entity(uint64_t handle) const = 0;
    virtual void for_each_entity(std::function<void(Entity*)> fn) const = 0;
};
```

### `IEntityMutator`
Structural scene changes. Used exclusively by command `execute()`/`undo()` implementations.

```cpp
class IEntityMutator {
public:
    virtual Entity* spawn_entity(EntityPtr parent = {}) = 0;
    virtual void destroy_entity(EntityPtr) = 0;
    virtual void insert_scene(UnserializedSceneFile&) = 0;
};
```

### `IEditorAPI`
Aggregates all three plus selection write access and viewport input.

```cpp
class IEditorAPI
    : public ICommandDispatcher
    , public ISceneQuery
    , public IEntityMutator
{
public:
    virtual SelectionState&        get_selection_mutable() = 0;
    virtual EditorViewportInput    get_viewport_input() const = 0;
};
```

---

## `Command` Base Class (`Commands/Command.h`)

Commands receive `IEditorAPI&` in `execute()`/`undo()`. They never store it — this is what makes them testable and decoupled from `EditorDoc`.

```cpp
class Command {
public:
    virtual ~Command() = default;
    virtual void execute(IEditorAPI&) = 0;
    virtual void undo(IEditorAPI&) = 0;
    virtual std::string to_string() const = 0;

    // Return true if this command absorbed `prev` (merging into one undo step).
    // Used by UndoRedoSystem to collapse rapid repeated mutations.
    virtual bool try_merge(const Command& prev) { return false; }
};
```

---

## `UndoRedoSystem` (`Core/UndoRedoSystem.h`)

Standalone. Takes `IEditorAPI&` at construction for executing commands against the scene.

### Responsibilities
- Execute and store commands
- Support command grouping (nested `command_group_start`/`end`)
- Support command merging (`try_merge`) to collapse rapid mutations into one undo step
- Expose history for the undo history panel

### Command Grouping

`command_group_start`/`command_group_end` calls can be nested. Only the outermost `end` commits the group as one atomic undo entry. A `GroupCommand` composite wraps accumulated commands and reports the group label.

```
command_group_start("Paste Objects")
  submit(SpawnCommand)         ← accumulated, not pushed to history yet
  submit(TransformCommand)     ← accumulated
command_group_end()            ← wraps both into GroupCommand, pushes once
```

### Command Merging

On `submit`, before pushing to history:
1. If a group is active, accumulate — no merge attempt.
2. Otherwise, call `cmd->try_merge(*history[cursor-1])`.
3. If merge returns `true`, the top history entry absorbs the new command. No new entry added.
4. If merge returns `false`, truncate redo tail, push, execute, advance cursor.

**`TransformCommand::try_merge`** — returns true if same entity set. Keeps original pre-transform, updates post-transform.
**`SetPropertyCommand::try_merge`** — returns true if same object handle + same property path. Keeps original `before_value`, updates `after_value`.

### History Panel API

```cpp
struct HistoryEntry {
    std::string label;
    bool        is_current_position;
};

std::vector<HistoryEntry> get_history() const;
void jump_to(int history_index);   // multi-step undo/redo

MulticastDelegate<> on_history_changed; // UndoHistoryPanel subscribes
```

### Constraints
- Max 256 history entries (configurable constant)
- All command storage uses `unique_ptr<Command>` — no raw owning pointers
- `UndoRedoSystem` is not copyable or movable

---

## `SelectionState` (`Core/SelectionState.h`)

Fully standalone — no `EditorDoc` dependency, no engine global access in the header. Fires `on_selection_changed` when the selection set changes. The outliner, property grid, and gizmo tool all subscribe to this delegate.

### Key API
```cpp
class SelectionState {
public:
    MulticastDelegate<> on_selection_changed;

    void add_to_selection(EntityPtr);
    void add_to_selection(const std::vector<EntityPtr>&);
    void remove_from_selection(EntityPtr);
    void set_only(EntityPtr);
    void clear();
    void validate();  // prune stale handles

    bool is_selected(EntityPtr) const;
    bool has_any() const;
    bool has_only_one() const;
    EntityPtr get_only_one() const;
    const std::unordered_set<uint64_t>& get_handles() const;
    std::vector<EntityPtr> as_vector() const;
};
```

---

## Viewport-Abstracted Tool Input (`EditorViewportInput.h`)

Tools never touch `UiSystem::inst`, `Input::is_mouse_down()`, or ImGui state directly. `EditorDoc` builds this struct once per frame and passes it to the active tool and to `EditorCamera`.

```cpp
struct EditorViewportInput {
    Ray        cursor_ray;       // unprojected world-space ray under cursor
    glm::vec2  mouse_delta;      // screen-space pixels moved this frame
    glm::vec2  mouse_pos_ndc;    // normalized device coords [-1, 1]
    bool       left_clicked;     // single click this frame (not held)
    bool       left_held;
    bool       right_held;
    bool       shift_held;
    bool       ctrl_held;
    bool       viewport_hovered;
    bool       viewport_focused;
    Rect2d     viewport_rect;
    float      scroll_delta;
    View_Setup view;             // current camera for tools that need to project
};
```

---

## Tool System (`Tools/`)

Three built-in tools selected by enum. No plugin registry.

```cpp
enum class EditorToolMode { Selection, FoliagePaint, DecalStamp };
```

All tools implement:

```cpp
class IEditorMode {
public:
    virtual ~IEditorMode() = default;
    virtual void tick(const EditorViewportInput& input, IEditorAPI& api) = 0;
    virtual void on_activate(IEditorAPI& api) {}
    virtual void on_deactivate(IEditorAPI& api) {}
    virtual std::string get_name() const = 0;
};
```

`EditorDoc` holds `unique_ptr<IEditorMode>` instances for each tool and switches the active pointer when `set_active_tool(EditorToolMode)` is called.

Tools submit commands via `api.submit(...)`. Tools read selection via `api.get_selection()`. Tools never hold a reference to any specific module.

---

## Commands (`Commands/`)

Each command file is independently understandable. All commands follow the same pattern: capture needed state at construction time (entity handles, serialized snapshots, pre/post values), then use only `IEditorAPI&` in `execute()`/`undo()`.

| File | Commands |
|---|---|
| `EntityCommands.h/cpp` | `SpawnEntityCommand`, `DestroyEntitiesCommand`, `DuplicateEntitiesCommand` |
| `TransformCommand.h/cpp` | `TransformCommand` (with `try_merge`) |
| `SetPropertyCommand.h/cpp` | `SetPropertyCommand` (with `try_merge`) |
| `HierarchyCommands.h/cpp` | `SetFolderCommand`, `MoveInHierarchyCommand` |

`DestroyEntitiesCommand` and `DuplicateEntitiesCommand` use `NewSerialization::serialize_to_text` to snapshot entities before destruction so undo can restore them via `insert_scene`.

`SetPropertyCommand` is submitted by `EdPropertyGrid` before applying any property change. This makes all property changes automatically undoable without per-property command authoring.

---

## UI Panels (`UI/`)

Each panel takes concrete references to exactly what it needs — never `EditorDoc`.

| Panel | Constructor dependencies |
|---|---|
| `EdPropertyGrid` | `SelectionState&`, `ICommandDispatcher&` |
| `ObjectOutliner` | `SelectionState&`, `ICommandDispatcher&` |
| `UndoHistoryPanel` | `UndoRedoSystem&` |
| `PropertyEditors` | (registers custom ImGui widgets, no runtime deps) |

`EdPropertyGrid` captures before-value via reflection before applying a change, then submits a `SetPropertyCommand`. This is the chokepoint that guarantees property undo coverage.

---

## `EditorCamera` (`Core/EditorCamera.h`)

Orbit and ortho modes. Takes `EditorViewportInput` each tick — no global input access. Exposes `View_Setup make_view() const` and `Ray unproject(glm::vec2 ndc) const` for use by `EditorDoc` when building the next frame's `EditorViewportInput`.

---

## Scene Export (`SceneExport.h/cpp`)

Unchanged in responsibility — serializes the current level to the runtime `.tmap` format using `NewSerialization`. Takes a list of entities and a path. No editor state knowledge required.

---

## `EditorDoc` — Thin Orchestrator

`EditorDoc` owns all module instances and wires them together. Its public interface is `IEditorAPI`. Internally it:

1. Builds `EditorViewportInput` each frame
2. Ticks `EditorCamera` with that input
3. Ticks the active `IEditorMode` with that input and `*this` as `IEditorAPI&`
4. Ticks `ManipulateTransformTool`
5. Draws UI panels (`imgui_draw`)
6. Handles save/load via `NewSerialization` and `SceneExport`

```cpp
class EditorDoc : public IEditorAPI {
public:
    static EditorDoc* create_scene(std::optional<std::string> path);

    // IEditorAPI implementation
    void submit(std::unique_ptr<Command>) override;
    void command_group_start(std::string label) override;
    void command_group_end() override;
    void undo() override;
    void redo() override;
    const SelectionState& get_selection() const override;
    SelectionState& get_selection_mutable() override;
    Entity* get_entity(uint64_t handle) const override;
    void for_each_entity(std::function<void(Entity*)> fn) const override;
    Entity* spawn_entity(EntityPtr parent = {}) override;
    void destroy_entity(EntityPtr) override;
    void insert_scene(UnserializedSceneFile&) override;
    EditorViewportInput get_viewport_input() const override;

    void set_active_tool(EditorToolMode);
    bool save();
    bool save_as(std::string path);

private:
    EditorDoc();

    std::unique_ptr<UndoRedoSystem>        undo_redo;
    std::unique_ptr<SelectionState>        selection;
    std::unique_ptr<EditorCamera>          camera;
    std::unique_ptr<EdPropertyGrid>        prop_grid;
    std::unique_ptr<ObjectOutliner>        outliner;
    std::unique_ptr<UndoHistoryPanel>      history_panel;
    std::unique_ptr<ManipulateTransformTool> manipulate;
    std::unique_ptr<DragDropPreview>       drag_drop;

    std::unique_ptr<SelectionMode>         tool_selection;
    std::unique_ptr<FoliagePaintTool>      tool_foliage;
    std::unique_ptr<DecalStampTool>        tool_decal;
    IEditorMode*                           active_tool = nullptr;

    EditorInputs       inputs;
    EditorToolMode     active_tool_mode = EditorToolMode::Selection;
    std::optional<std::string> asset_name;
};
```

---

## Integration Tests (`IntegrationTests/LevelEditor/`)

Tests construct only the modules they need with a stub `IEditorAPI` implementation. No full editor, no ImGui, no render loop.

| Test file | Covers |
|---|---|
| `test_undo_redo.cpp` | Submit commands, undo, verify scene state restored |
| `test_selection.cpp` | Add/remove/clear/validate selection |
| `test_transform_merge.cpp` | Rapid transforms collapse to one undo step |
| `test_command_groups.cpp` | `group_start`/`group_end` produces single undo entry |
| `test_property_undo.cpp` | Property change → undo → original value restored |

---

## What Is Removed vs Current Design

| Current | New |
|---|---|
| `EditorDocLocal.h` (947-line god header) | Split across 15+ focused headers |
| Every class takes `EditorDoc&` | Classes take only what they need |
| Raw `Command*` with manual delete | `unique_ptr<Command>` throughout |
| `#if 0` dead ObjectOutliner code | Deleted |
| `hacked_entity_MFER` | Replaced with proper state in ManipulateTool |
| Spotty undo (only some ops) | All mutations through `ICommandDispatcher` |
| Tools reach into global input | Tools receive `EditorViewportInput` struct |
| No history panel | `UndoHistoryPanel` with `jump_to()` |
| No command merging | `try_merge()` on Command base |
| No command grouping | `command_group_start`/`command_group_end` |

# Level Editor Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the monolithic `EditorDocLocal.h` god-header with a modular, interface-driven level editor where every mutation is undoable by construction, all subsystems depend only on `IEditorAPI`, and each module can be built and tested without pulling in the whole editor.

**Architecture:** `EditorDoc` remains the engine-facing tool (keeps `IEditorTool`) and also becomes the public API surface (implements new `IEditorAPI`). All modules — `SelectionState`, `UndoRedoSystem`, tools, UI panels, commands — are constructed independently and wired inside `EditorDoc`. Commands capture pre/post state at construction; `execute()`/`undo()` accept `IEditorAPI&` at call time and never store it.

**Tech Stack:** C++17, MSVC 2019, ImGui, ImGuizmo, glm, engine types (`Entity`, `EntityPtr`, `NewSerialization`, `UnserializedSceneFile`, `View_Setup` from `Render/DrawPublic.h`, `Ray` from `Framework/MathLib.h`, `Rect2d` from `Framework/Rect2d.h`, `AssetDatabase::loader` for `IAssetLoadingInterface`)

---

## File Map

**New files to create:**
```
Source/LevelEditor/
├── IEditorAPI.h
├── EditorViewportInput.h
├── EditorDoc.h
├── EditorDoc.cpp
├── Core/
│   ├── SelectionState.h / SelectionState.cpp
│   ├── UndoRedoSystem.h / UndoRedoSystem.cpp
│   └── EditorCamera.h / EditorCamera.cpp
├── Commands/
│   ├── Command.h
│   ├── EntityCommands.h / EntityCommands.cpp
│   ├── TransformCommand.h / TransformCommand.cpp
│   ├── SetPropertyCommand.h / SetPropertyCommand.cpp
│   └── HierarchyCommands.h / HierarchyCommands.cpp
├── Tools/
│   ├── IEditorMode.h
│   ├── SelectionMode.h / SelectionMode.cpp
│   ├── FoliagePaintTool.h / FoliagePaintTool.cpp
│   └── DecalStampTool.h / DecalStampTool.cpp
└── UI/
    ├── EdPropertyGrid.h / EdPropertyGrid.cpp
    ├── ObjectOutliner.h / ObjectOutliner.cpp
    └── UndoHistoryPanel.h / UndoHistoryPanel.cpp

Source/IntegrationTests/Tests/Editor/
├── test_editor_undo.cpp
└── test_editor_selection.cpp
```

**Files to modify:**
- `Source/IntegrationTests/EditorTestContext.h` — expand to expose selection count and undo history size
- `Source/IntegrationTests/EditorTestContext.cpp` — update include path; delegate through `IEditorAPI`
- `Source/IntegrationTests/Tests/Editor/test_serialize.cpp` — update `save_level` call after API change
- `CsRemake.vcxproj` + `CsRemake.vcxproj.filters` — add every new `.cpp`/`.h`

**Files to delete (Task 18):**
- `Source/LevelEditor/EditorDocLocal.h`, `EditorDocLocal.cpp`
- `Source/LevelEditor/Commands.h`, `Commands.cpp`
- `Source/LevelEditor/SelectionState.h` (old, replaced by `Core/SelectionState.h`)
- `Source/LevelEditor/ObjectOutliner.cpp`, `ObjectOutlineFilter.h`

---

## Task 1: Core interfaces — IEditorAPI.h, Command.h, EditorViewportInput.h

**Files:**
- Create: `Source/LevelEditor/IEditorAPI.h`
- Create: `Source/LevelEditor/Commands/Command.h`
- Create: `Source/LevelEditor/EditorViewportInput.h`

- [ ] **Step 1: Create `Source/LevelEditor/IEditorAPI.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <memory>
#include <functional>
#include <cstdint>
#include "Game/EntityPtr.h"

class Entity;
class SelectionState;
struct EditorViewportInput;
class Command;
struct UnserializedSceneFile;

// Write side of the undo system. Every scene mutation goes through here.
class ICommandDispatcher {
public:
    virtual ~ICommandDispatcher() = default;
    virtual void submit(std::unique_ptr<Command> cmd) = 0;
    virtual void command_group_start(std::string label) = 0;
    virtual void command_group_end() = 0;
    virtual void undo() = 0;
    virtual void redo() = 0;
};

// Read-only scene and selection access.
class ISceneQuery {
public:
    virtual ~ISceneQuery() = default;
    virtual const SelectionState& get_selection() const = 0;
    virtual Entity* get_entity(uint64_t handle) const = 0;
    virtual void for_each_entity(std::function<void(Entity*)> fn) const = 0;
};

// Structural scene changes. Used exclusively by Command execute()/undo().
class IEntityMutator {
public:
    virtual ~IEntityMutator() = default;
    virtual Entity* spawn_entity(EntityPtr parent = {}) = 0;
    virtual void destroy_entity(EntityPtr) = 0;
    virtual void insert_scene(UnserializedSceneFile&) = 0;
};

// Public surface exposed to tools, commands, and UI panels.
class IEditorAPI
    : public ICommandDispatcher
    , public ISceneQuery
    , public IEntityMutator
{
public:
    virtual SelectionState&     get_selection_mutable() = 0;
    virtual EditorViewportInput get_viewport_input() const = 0;
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Commands/Command.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <memory>

class IEditorAPI;

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(IEditorAPI& api) = 0;
    virtual void undo(IEditorAPI& api) = 0;
    virtual std::string to_string() const = 0;

    // Return true if this command absorbed `prev` (merging into one undo step).
    // Used by UndoRedoSystem to collapse rapid repeated mutations (e.g. drag).
    virtual bool try_merge(const Command& prev) { return false; }
};
#endif
```

- [ ] **Step 3: Create `Source/LevelEditor/EditorViewportInput.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <glm/glm.hpp>
#include "Framework/MathLib.h"
#include "Framework/Rect2d.h"
#include "Render/DrawPublic.h"

struct EditorViewportInput {
    Ray        cursor_ray;        // unprojected world-space ray under cursor
    glm::vec2  mouse_delta;       // screen-space pixels moved this frame
    glm::vec2  mouse_pos_ndc;     // normalized device coords [-1, 1]
    bool       left_clicked  = false; // single click this frame (not held)
    bool       left_held     = false;
    bool       right_held    = false;
    bool       shift_held    = false;
    bool       ctrl_held     = false;
    bool       viewport_hovered = false;
    bool       viewport_focused = false;
    Rect2d     viewport_rect;
    float      scroll_delta  = 0.f;
    View_Setup view;              // current camera for tools that need to project
};
#endif
```

- [ ] **Step 4: Add new headers to `CsRemake.vcxproj`**

Open `CsRemake.vcxproj`, find the `<ItemGroup>` that contains `<ClInclude Include="Source\LevelEditor\Commands.h" />` and add alongside it:
```xml
<ClInclude Include="Source\LevelEditor\IEditorAPI.h" />
<ClInclude Include="Source\LevelEditor\EditorViewportInput.h" />
<ClInclude Include="Source\LevelEditor\Commands\Command.h" />
```

In `CsRemake.vcxproj.filters`, add matching filter entries under the `LevelEditor` filter.

- [ ] **Step 5: Verify the headers compile by including them in `EditorDocLocal.cpp`**

Add at top of `Source/LevelEditor/EditorDocLocal.cpp` (temporary, remove after Task 13):
```cpp
#include "LevelEditor/IEditorAPI.h"
#include "LevelEditor/EditorViewportInput.h"
#include "LevelEditor/Commands/Command.h"
```

- [ ] **Step 6: Build — expect clean compile**

Run: `powershell.exe Scripts/build_and_test.ps1`
Expected: Build succeeds. Any error here is an include path or syntax issue in the new headers — fix before continuing.

- [ ] **Step 7: Remove temporary includes from `EditorDocLocal.cpp`**

Remove the three lines added in Step 5.

- [ ] **Step 8: Commit**

```
git add Source/LevelEditor/IEditorAPI.h Source/LevelEditor/EditorViewportInput.h Source/LevelEditor/Commands/Command.h CsRemake.vcxproj CsRemake.vcxproj.filters
git commit -m "Add IEditorAPI, Command base, EditorViewportInput headers"
```

---

## Task 2: SelectionState (standalone module)

**Files:**
- Create: `Source/LevelEditor/Core/SelectionState.h`
- Create: `Source/LevelEditor/Core/SelectionState.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/Core/SelectionState.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <unordered_set>
#include <vector>
#include <cstdint>
#include "Game/EntityPtr.h"
#include "Framework/MulticastDelegate.h"

class Entity;

class SelectionState {
public:
    SelectionState() = default;
    SelectionState(const SelectionState&) = delete;
    SelectionState& operator=(const SelectionState&) = delete;

    MulticastDelegate<> on_selection_changed;

    void add_to_selection(EntityPtr ptr);
    void add_to_selection(const std::vector<EntityPtr>& ptrs);
    void remove_from_selection(EntityPtr ptr);
    void set_only(EntityPtr ptr);
    void clear();
    void validate(); // prune stale handles

    bool is_selected(EntityPtr ptr) const;
    bool has_any() const;
    bool has_only_one() const;
    EntityPtr get_only_one() const; // asserts has_only_one()

    const std::unordered_set<uint64_t>& get_handles() const { return handles; }
    std::vector<EntityPtr> as_vector() const;

private:
    std::unordered_set<uint64_t> handles;
    void fire_changed() { on_selection_changed.invoke(); }
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Core/SelectionState.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/Core/SelectionState.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include <cassert>
#include <algorithm>

void SelectionState::add_to_selection(EntityPtr ptr) {
    if (!ptr.get()) return;
    handles.insert(ptr.handle);
    fire_changed();
}

void SelectionState::add_to_selection(const std::vector<EntityPtr>& ptrs) {
    bool any = false;
    for (auto& p : ptrs) {
        if (p.get()) { handles.insert(p.handle); any = true; }
    }
    if (any) fire_changed();
}

void SelectionState::remove_from_selection(EntityPtr ptr) {
    if (handles.erase(ptr.handle)) fire_changed();
}

void SelectionState::set_only(EntityPtr ptr) {
    handles.clear();
    if (ptr.get()) handles.insert(ptr.handle);
    fire_changed();
}

void SelectionState::clear() {
    if (handles.empty()) return;
    handles.clear();
    fire_changed();
}

void SelectionState::validate() {
    bool changed = false;
    for (auto it = handles.begin(); it != handles.end();) {
        if (!eng->get_object(*it)) { it = handles.erase(it); changed = true; }
        else ++it;
    }
    if (changed) fire_changed();
}

bool SelectionState::is_selected(EntityPtr ptr) const {
    return handles.count(ptr.handle) > 0;
}

bool SelectionState::has_any() const { return !handles.empty(); }

bool SelectionState::has_only_one() const { return handles.size() == 1; }

EntityPtr SelectionState::get_only_one() const {
    assert(has_only_one());
    uint64_t h = *handles.begin();
    return EntityPtr{ h };
}

std::vector<EntityPtr> SelectionState::as_vector() const {
    std::vector<EntityPtr> out;
    out.reserve(handles.size());
    for (uint64_t h : handles) out.push_back(EntityPtr{ h });
    return out;
}
#endif
```

> **Note on EntityPtr:** `EntityPtr` stores a `handle` field (uint64_t). Check `Source/Game/EntityPtr.h` for the exact field name; adjust `ptr.handle` if the field is named differently (e.g., `ptr.id`). Also verify `eng->get_object(uint64_t)` signature — the old `Commands.cpp` uses `eng->get_level()->get_entity(handle)`.

- [ ] **Step 3: Add to `CsRemake.vcxproj`**

```xml
<ClInclude Include="Source\LevelEditor\Core\SelectionState.h" />
<ClCompile Include="Source\LevelEditor\Core\SelectionState.cpp" />
```

- [ ] **Step 4: Verify compile (add temp include to `EditorDocLocal.cpp`, build, remove)**

- [ ] **Step 5: Commit**

```
git add Source/LevelEditor/Core/SelectionState.h Source/LevelEditor/Core/SelectionState.cpp CsRemake.vcxproj CsRemake.vcxproj.filters
git commit -m "Add standalone SelectionState to Core/"
```

---

## Task 3: UndoRedoSystem

**Files:**
- Create: `Source/LevelEditor/Core/UndoRedoSystem.h`
- Create: `Source/LevelEditor/Core/UndoRedoSystem.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/Core/UndoRedoSystem.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <vector>
#include <memory>
#include <string>
#include "Framework/MulticastDelegate.h"

class Command;
class IEditorAPI;

struct HistoryEntry {
    std::string label;
    bool is_current_position = false;
};

class UndoRedoSystem {
public:
    static constexpr int MAX_HISTORY = 256;

    explicit UndoRedoSystem(IEditorAPI& api);
    ~UndoRedoSystem() = default;
    UndoRedoSystem(const UndoRedoSystem&) = delete;
    UndoRedoSystem& operator=(const UndoRedoSystem&) = delete;

    // Submit a command. Executes it immediately. Applies try_merge.
    void submit(std::unique_ptr<Command> cmd);

    // Grouping: nested calls ok; only outermost end() commits.
    void command_group_start(std::string label);
    void command_group_end();

    void undo();
    void redo();

    std::vector<HistoryEntry> get_history() const;
    void jump_to(int history_index); // multi-step undo/redo

    MulticastDelegate<> on_history_changed;

private:
    struct GroupCommand; // defined in .cpp

    IEditorAPI& api;

    // Flat history. cursor points to the last executed entry (or -1).
    std::vector<std::unique_ptr<Command>> history;
    int cursor = -1; // index of last executed command (-1 = nothing executed)

    // Grouping stack
    struct GroupFrame {
        std::string label;
        std::vector<std::unique_ptr<Command>> accumulated;
    };
    std::vector<GroupFrame> group_stack;

    void push_command(std::unique_ptr<Command> cmd);
    void truncate_redo_tail();
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Core/UndoRedoSystem.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/Core/UndoRedoSystem.h"
#include "LevelEditor/IEditorAPI.h"
#include "LevelEditor/Commands/Command.h"
#include <cassert>
#include <algorithm>

// A GroupCommand wraps a list of commands accumulated between
// command_group_start/end and executes/undoes them as one atomic step.
struct UndoRedoSystem::GroupCommand : public Command {
    std::string label;
    std::vector<std::unique_ptr<Command>> cmds;

    explicit GroupCommand(std::string lbl, std::vector<std::unique_ptr<Command>> c)
        : label(std::move(lbl)), cmds(std::move(c)) {}

    void execute(IEditorAPI& api) override {
        for (auto& c : cmds) c->execute(api);
    }
    void undo(IEditorAPI& api) override {
        for (int i = (int)cmds.size() - 1; i >= 0; --i)
            cmds[i]->undo(api);
    }
    std::string to_string() const override { return label; }
};

UndoRedoSystem::UndoRedoSystem(IEditorAPI& api) : api(api) {}

void UndoRedoSystem::submit(std::unique_ptr<Command> cmd) {
    assert(cmd);

    // If inside a group, accumulate without executing yet.
    if (!group_stack.empty()) {
        cmd->execute(api);
        group_stack.back().accumulated.push_back(std::move(cmd));
        return;
    }

    // Try merging into top of history.
    if (cursor >= 0 && !history.empty()) {
        if (cmd->try_merge(*history[cursor])) {
            // Merged: re-execute the top entry (now updated) so the scene
            // reflects the merged state. The merged command updated its own
            // post-state in try_merge.
            history[cursor]->execute(api);
            on_history_changed.invoke();
            return;
        }
    }

    cmd->execute(api);
    push_command(std::move(cmd));
    on_history_changed.invoke();
}

void UndoRedoSystem::command_group_start(std::string label) {
    group_stack.push_back({ std::move(label), {} });
}

void UndoRedoSystem::command_group_end() {
    assert(!group_stack.empty() && "command_group_end without matching start");
    GroupFrame frame = std::move(group_stack.back());
    group_stack.pop_back();

    if (group_stack.empty()) {
        // Outermost end: wrap accumulated commands into one GroupCommand and commit.
        if (!frame.accumulated.empty()) {
            auto gc = std::make_unique<GroupCommand>(
                std::move(frame.label), std::move(frame.accumulated));
            // Commands already executed during accumulation; don't re-execute.
            push_command(std::move(gc));
            on_history_changed.invoke();
        }
    } else {
        // Still nested: forward accumulated commands to the parent frame.
        for (auto& c : frame.accumulated)
            group_stack.back().accumulated.push_back(std::move(c));
    }
}

void UndoRedoSystem::undo() {
    if (cursor < 0) return;
    history[cursor]->undo(api);
    --cursor;
    on_history_changed.invoke();
}

void UndoRedoSystem::redo() {
    if (cursor + 1 >= (int)history.size()) return;
    ++cursor;
    history[cursor]->execute(api);
    on_history_changed.invoke();
}

std::vector<HistoryEntry> UndoRedoSystem::get_history() const {
    std::vector<HistoryEntry> out;
    out.reserve(history.size());
    for (int i = 0; i < (int)history.size(); ++i) {
        out.push_back({ history[i]->to_string(), i == cursor });
    }
    return out;
}

void UndoRedoSystem::jump_to(int idx) {
    idx = std::clamp(idx, -1, (int)history.size() - 1);
    while (cursor > idx) { history[cursor]->undo(api); --cursor; }
    while (cursor < idx) { ++cursor; history[cursor]->execute(api); }
    on_history_changed.invoke();
}

void UndoRedoSystem::push_command(std::unique_ptr<Command> cmd) {
    truncate_redo_tail();
    if ((int)history.size() >= MAX_HISTORY) {
        history.erase(history.begin());
        if (cursor > 0) --cursor;
    }
    history.push_back(std::move(cmd));
    cursor = (int)history.size() - 1;
}

void UndoRedoSystem::truncate_redo_tail() {
    if (cursor + 1 < (int)history.size())
        history.erase(history.begin() + cursor + 1, history.end());
}
#endif
```

> **Merge semantics note:** In `submit()`, when `try_merge` returns true the spec says the top entry "absorbs the new command." The expectation is that `try_merge` *updates the top entry's post-state in place* (e.g. `TransformCommand::try_merge` overwrites `post_transform`). After merging, `history[cursor]->execute(api)` replays the updated top entry so the scene reflects the new merged state. This is only correct if `execute()` is idempotent with respect to the post-state — verify with `TransformCommand` (Task 6).

- [ ] **Step 3: Add to vcxproj + verify compile + commit**

```xml
<ClInclude Include="Source\LevelEditor\Core\UndoRedoSystem.h" />
<ClCompile Include="Source\LevelEditor\Core\UndoRedoSystem.cpp" />
```

```
git add Source/LevelEditor/Core/UndoRedoSystem.h Source/LevelEditor/Core/UndoRedoSystem.cpp CsRemake.vcxproj CsRemake.vcxproj.filters
git commit -m "Add UndoRedoSystem with grouping and try_merge support"
```

---

## Task 4: EditorCamera (standalone)

**Files:**
- Create: `Source/LevelEditor/Core/EditorCamera.h`
- Create: `Source/LevelEditor/Core/EditorCamera.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/Core/EditorCamera.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "Render/DrawPublic.h"
#include "Framework/MathLib.h"
#include <glm/glm.hpp>

struct EditorViewportInput;

// Orbit (perspective) and ortho camera.
// Receives EditorViewportInput each tick — no global input access.
class EditorCamera {
public:
    EditorCamera() = default;

    void tick(const EditorViewportInput& input);

    View_Setup make_view() const;
    Ray unproject(glm::vec2 ndc) const; // used by EditorDoc to build next frame's cursor_ray

    bool get_is_using_ortho() const { return is_ortho; }
    void set_orbit_target(glm::vec3 target) { orbit_target = target; }
    glm::vec3 get_orbit_target() const { return orbit_target; }

private:
    glm::vec3 orbit_target = glm::vec3(0.f);
    float orbit_distance   = 10.f;
    float yaw              = 0.f;
    float pitch            = -0.4f;
    bool  is_ortho         = false;

    // Ortho state
    glm::vec3 ortho_pos   = glm::vec3(0.f);
    glm::vec3 ortho_front = glm::vec3(1.f, 0.f, 0.f);
    float     ortho_width = 25.f;

    void tick_orbit(const EditorViewportInput& input);
    void tick_ortho(const EditorViewportInput& input);
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Core/EditorCamera.cpp`**

Port the orbit/ortho camera logic from `Source/LevelEditor/EditorDocLocal.h` (`EditorCamera` and `OrthoCamera` classes, lines ~306–700). The existing implementation uses `UiSystem::inst` and global `Input::` calls — replace all of those with the corresponding fields from `EditorViewportInput` (`input.right_held`, `input.mouse_delta`, `input.scroll_delta`, `input.viewport_focused`, etc.).

Key mapping:
- `Input::is_mouse_down(1)` → `input.right_held`
- `Input::get_mouse_delta()` → `input.mouse_delta`
- `UiSystem::inst->is_vp_focused()` → `input.viewport_focused`
- `UiSystem::inst->is_vp_hovered()` → `input.viewport_hovered`
- Mouse scroll → `input.scroll_delta`

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/Core/EditorCamera.h"
#include "LevelEditor/EditorViewportInput.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void EditorCamera::tick(const EditorViewportInput& input) {
    if (!input.viewport_focused && !input.viewport_hovered) return;
    if (is_ortho) tick_ortho(input);
    else          tick_orbit(input);
}

void EditorCamera::tick_orbit(const EditorViewportInput& input) {
    if (input.right_held) {
        yaw   += input.mouse_delta.x * 0.005f;
        pitch += input.mouse_delta.y * 0.005f;
        pitch  = std::clamp(pitch, -1.5f, 1.5f);
    }
    orbit_distance -= input.scroll_delta * orbit_distance * 0.1f;
    orbit_distance  = std::clamp(orbit_distance, 0.1f, 2000.f);
}

void EditorCamera::tick_ortho(const EditorViewportInput& input) {
    // Port ortho pan/zoom from OrthoCamera::tick in EditorDocLocal.h
}

View_Setup EditorCamera::make_view() const {
    if (is_ortho) {
        // Build ortho View_Setup from ortho_pos / ortho_front / ortho_width
        // Refer to OrthoCamera in EditorDocLocal.h for the existing matrix construction.
        View_Setup vs;
        vs.is_ortho = true;
        vs.origin = ortho_pos;
        vs.front  = ortho_front;
        // ... set vs.view, vs.proj using glm::ortho + glm::lookAt
        return vs;
    }
    glm::vec3 front(
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    );
    glm::vec3 pos = orbit_target - front * orbit_distance;
    return View_Setup(pos, glm::normalize(front), 60.f, 0.1f, 8000.f, 1280, 720);
}

Ray EditorCamera::unproject(glm::vec2 ndc) const {
    View_Setup vs = make_view();
    glm::vec4 clip(ndc.x, ndc.y, -1.f, 1.f);
    glm::mat4 inv_vp = glm::inverse(vs.viewproj);
    glm::vec4 world  = inv_vp * clip;
    world /= world.w;
    glm::vec3 dir = glm::normalize(glm::vec3(world) - vs.origin);
    return Ray(vs.origin, dir);
}
#endif
```

> The `View_Setup(pos, front, fov, near, far, width, height)` constructor sets `view`, `proj`, `viewproj` internally — see `Source/Render/DrawPublic.h`. Width/height will need to come from `viewport_rect` in a full implementation; use the values from `EditorViewportInput::viewport_rect`.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add EditorCamera: orbit/ortho camera taking EditorViewportInput"
```

---

## Task 5: EntityCommands

**Files:**
- Create: `Source/LevelEditor/Commands/EntityCommands.h`
- Create: `Source/LevelEditor/Commands/EntityCommands.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/Commands/EntityCommands.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/Command.h"
#include "Game/EntityPtr.h"
#include <vector>
#include <string>

// Spawn one entity (optionally under a parent).
// Undo destroys it; redo re-spawns a blank entity (no model/components).
// Callers that need model/property setup should follow SpawnEntityCommand
// with a SetPropertyCommand in a command group.
class SpawnEntityCommand : public Command {
public:
    explicit SpawnEntityCommand(EntityPtr parent = {}) : parent(parent) {}

    void execute(IEditorAPI& api) override;
    void undo(IEditorAPI& api) override;
    std::string to_string() const override { return "Spawn Entity"; }

    EntityPtr get_spawned() const { return spawned; }

private:
    EntityPtr parent;
    EntityPtr spawned;
};

// Destroy one or more entities. Serializes them before destruction so undo
// can restore exact state (handles, transforms, properties).
class DestroyEntitiesCommand : public Command {
public:
    explicit DestroyEntitiesCommand(std::vector<EntityPtr> targets);

    void execute(IEditorAPI& api) override;
    void undo(IEditorAPI& api) override;
    std::string to_string() const override { return "Destroy Entities"; }

private:
    std::vector<EntityPtr> targets;
    std::string snapshot; // serialized before first execute()
    bool snapshot_taken = false;
};

// Duplicate selected entities into the scene, offset slightly.
class DuplicateEntitiesCommand : public Command {
public:
    explicit DuplicateEntitiesCommand(std::vector<EntityPtr> sources);

    void execute(IEditorAPI& api) override;
    void undo(IEditorAPI& api) override;
    std::string to_string() const override { return "Duplicate Entities"; }

private:
    std::vector<EntityPtr> sources;
    std::string snapshot;   // serialized copy of originals (for re-spawn on redo)
    std::vector<EntityPtr> duplicated; // handles of created copies
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Commands/EntityCommands.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/EntityCommands.h"
#include "LevelEditor/IEditorAPI.h"
#include "LevelEditor/Core/SelectionState.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"
#include <cassert>

// ---- SpawnEntityCommand ----

void SpawnEntityCommand::execute(IEditorAPI& api) {
    Entity* e = api.spawn_entity(parent);
    assert(e);
    spawned = e->get_self_ptr();
}

void SpawnEntityCommand::undo(IEditorAPI& api) {
    if (spawned.get())
        api.destroy_entity(spawned);
}

// ---- DestroyEntitiesCommand ----

DestroyEntitiesCommand::DestroyEntitiesCommand(std::vector<EntityPtr> targets)
    : targets(std::move(targets)) {}

void DestroyEntitiesCommand::execute(IEditorAPI& api) {
    if (!snapshot_taken) {
        // Collect live Entity* pointers and serialize before first destroy.
        std::vector<Entity*> live;
        api.for_each_entity([&](Entity* e) {
            for (auto& t : targets)
                if (t.handle == e->get_instance_id()) { live.push_back(e); break; }
        });
        auto serial = NewSerialization::serialize_to_text("destroy_undo", live, /*write_ids=*/true);
        snapshot = serial.text;
        snapshot_taken = true;
    }
    for (auto& t : targets) {
        if (t.get()) api.destroy_entity(t);
    }
    api.get_selection_mutable().validate();
}

void DestroyEntitiesCommand::undo(IEditorAPI& api) {
    auto unserialized = NewSerialization::unserialize_from_text(
        "destroy_undo", snapshot, *AssetDatabase::loader, /*keepid=*/true);
    api.insert_scene(unserialized);
}

// ---- DuplicateEntitiesCommand ----

DuplicateEntitiesCommand::DuplicateEntitiesCommand(std::vector<EntityPtr> sources)
    : sources(std::move(sources)) {}

void DuplicateEntitiesCommand::execute(IEditorAPI& api) {
    if (snapshot.empty()) {
        // First execute: serialize originals, spawn copies.
        std::vector<Entity*> live;
        api.for_each_entity([&](Entity* e) {
            for (auto& s : sources)
                if (s.handle == e->get_instance_id()) { live.push_back(e); break; }
        });
        auto serial = NewSerialization::serialize_to_text("dup", live, /*write_ids=*/false);
        snapshot = serial.text;
    }

    auto unserialized = NewSerialization::unserialize_from_text(
        "dup", snapshot, *AssetDatabase::loader, /*keepid=*/false);
    api.insert_scene(unserialized);

    duplicated.clear();
    api.get_selection_mutable().clear();
    api.for_each_entity([&](Entity* e) {
        // The insert gives new handles; collect them.
        // Insert_scene adds entities to the level; check which ones are "new"
        // by comparing against sources list.
        bool is_source = false;
        for (auto& s : sources)
            if (s.handle == e->get_instance_id()) { is_source = true; break; }
        // This approach is imperfect — see note below.
    });
}

void DuplicateEntitiesCommand::undo(IEditorAPI& api) {
    for (auto& d : duplicated) {
        if (d.get()) api.destroy_entity(d);
    }
    duplicated.clear();
    api.get_selection_mutable().validate();
}
#endif
```

> **DuplicateEntitiesCommand note:** Tracking which entities were newly created by `insert_scene` is the hard part. The existing `Commands.cpp` solves this by iterating `all_obj_vec` on the `UnserializedSceneFile`. Update `IEntityMutator::insert_scene` to return the created entity handles, or pass a callback. Alternatively, store the new handles from `UnserializedSceneFile::all_obj_vec` before calling `insert_scene` — examine the existing `DuplicateEntitiesCommand::execute` in `Source/LevelEditor/Commands.cpp` for the pattern used there.

> **AssetDatabase::loader:** This is a `static IAssetLoadingInterface*` on `AssetDatabase`. Include `Assets/AssetDatabase.h`.

> **`e->get_instance_id()`:** Verify the method name on `Entity`. The old code uses `eng->get_level()->get_entity(handle)` — check `Game/Entity.h` and `Game/EntityPtr.h` for the correct handle accessor.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add EntityCommands: Spawn, Destroy, Duplicate"
```

---

## Task 6: TransformCommand

**Files:**
- Create: `Source/LevelEditor/Commands/TransformCommand.h`
- Create: `Source/LevelEditor/Commands/TransformCommand.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/Commands/TransformCommand.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/Command.h"
#include "Game/EntityPtr.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

class TransformCommand : public Command {
public:
    struct Entry {
        EntityPtr   ptr;
        glm::mat4   pre_transform;
        glm::mat4   post_transform;
    };

    // pre_transforms: snapshot of world transforms BEFORE the move began.
    // selection: the set of entity handles being moved.
    TransformCommand(std::vector<Entry> entries);

    void execute(IEditorAPI& api) override;
    void undo(IEditorAPI& api) override;
    std::string to_string() const override { return "Transform Entities"; }

    // Merge: same entity set → keep pre from prev, update post from this.
    bool try_merge(const Command& prev) override;

    const std::vector<Entry>& get_entries() const { return entries; }

private:
    std::vector<Entry> entries;
    void apply(IEditorAPI& api, bool use_post);
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Commands/TransformCommand.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/TransformCommand.h"
#include "LevelEditor/IEditorAPI.h"
#include "Game/Entity.h"
#include <algorithm>

TransformCommand::TransformCommand(std::vector<Entry> entries)
    : entries(std::move(entries)) {}

void TransformCommand::execute(IEditorAPI& api) { apply(api, /*use_post=*/true); }
void TransformCommand::undo(IEditorAPI& api)    { apply(api, /*use_post=*/false); }

void TransformCommand::apply(IEditorAPI& api, bool use_post) {
    for (auto& e : entries) {
        Entity* ent = e.ptr.get();
        if (!ent) continue;
        const glm::mat4& mat = use_post ? e.post_transform : e.pre_transform;
        ent->set_ws_transform(mat);
    }
}

bool TransformCommand::try_merge(const Command& prev) {
    const auto* p = dynamic_cast<const TransformCommand*>(&prev);
    if (!p) return false;
    if (p->entries.size() != entries.size()) return false;

    // Same entity set (order-insensitive check via handle comparison)?
    for (const auto& my_e : entries) {
        bool found = false;
        for (const auto& prev_e : p->entries)
            if (my_e.ptr.handle == prev_e.ptr.handle) { found = true; break; }
        if (!found) return false;
    }

    // Keep pre-transforms from prev; update post-transforms from this.
    for (auto& my_e : entries) {
        for (const auto& prev_e : p->entries) {
            if (my_e.ptr.handle == prev_e.ptr.handle) {
                my_e.pre_transform = prev_e.pre_transform;
                break;
            }
        }
    }
    return true;
}
#endif
```

> **`ent->set_ws_transform(mat)`:** Verify the method name on `Entity`. The old code calls `eng->set_object_transform(...)` or `e->set_world_transform(...)`. Check `Source/Game/Entity.h` for the correct setter.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add TransformCommand with try_merge for drag-collapse"
```

---

## Task 7: SetPropertyCommand

**Files:**
- Create: `Source/LevelEditor/Commands/SetPropertyCommand.h`
- Create: `Source/LevelEditor/Commands/SetPropertyCommand.cpp`

**Design:** Uses entity snapshots (same pattern as `DestroyEntitiesCommand`) to avoid needing per-property serialization hooks. Before the property change, the property grid serializes the entity to `before_snapshot`. After the change, it serializes again to `after_snapshot`. `execute` destroys + re-inserts from `after_snapshot`; `undo` destroys + re-inserts from `before_snapshot`. `try_merge` collapses rapid changes to the same property (same entity handle + same property path).

- [ ] **Step 1: Create `Source/LevelEditor/Commands/SetPropertyCommand.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/Command.h"
#include <string>
#include <cstdint>

class SetPropertyCommand : public Command {
public:
    // prop_path: the property name (e.g. "position"), used only for try_merge.
    // before_snapshot / after_snapshot: full entity serialized text.
    SetPropertyCommand(uint64_t entity_handle,
                       std::string prop_path,
                       std::string before_snapshot,
                       std::string after_snapshot);

    void execute(IEditorAPI& api) override;
    void undo(IEditorAPI& api) override;
    std::string to_string() const override { return "Set Property"; }

    // Merge: same entity + same property → keep before from prev, update after from this.
    bool try_merge(const Command& prev) override;

private:
    uint64_t    entity_handle;
    std::string prop_path;
    std::string before_snapshot;
    std::string after_snapshot;

    void restore_from(IEditorAPI& api, const std::string& snapshot);
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Commands/SetPropertyCommand.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/SetPropertyCommand.h"
#include "LevelEditor/IEditorAPI.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"

SetPropertyCommand::SetPropertyCommand(uint64_t entity_handle,
                                       std::string prop_path,
                                       std::string before_snapshot,
                                       std::string after_snapshot)
    : entity_handle(entity_handle)
    , prop_path(std::move(prop_path))
    , before_snapshot(std::move(before_snapshot))
    , after_snapshot(std::move(after_snapshot))
{}

void SetPropertyCommand::execute(IEditorAPI& api) { restore_from(api, after_snapshot); }
void SetPropertyCommand::undo(IEditorAPI& api)    { restore_from(api, before_snapshot); }

void SetPropertyCommand::restore_from(IEditorAPI& api, const std::string& snapshot) {
    // Destroy the current version, then re-insert from snapshot.
    Entity* ent = api.get_entity(entity_handle);
    if (!ent) return;
    api.destroy_entity(ent->get_self_ptr());

    auto unserialized = NewSerialization::unserialize_from_text(
        "prop_undo", snapshot, *AssetDatabase::loader, /*keepid=*/true);
    api.insert_scene(unserialized);
}

bool SetPropertyCommand::try_merge(const Command& prev) {
    const auto* p = dynamic_cast<const SetPropertyCommand*>(&prev);
    if (!p) return false;
    if (p->entity_handle != entity_handle) return false;
    if (p->prop_path != prop_path) return false;

    // Keep before_snapshot from prev; update after_snapshot from this.
    before_snapshot = p->before_snapshot;
    return true;
}
#endif
```

> **EdPropertyGrid usage:** `EdPropertyGrid::draw()` (Task 10) must call `NewSerialization::serialize_to_text` *before* calling the PropertyGrid `update()` that applies the change, and again *after*, then submit `SetPropertyCommand`. Hook into `PropertyGrid`'s pre/post-change callbacks, or restructure the draw loop to serialize before delegating to `PropertyGrid::update()`.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add SetPropertyCommand: entity-snapshot-based property undo with try_merge"
```

---

## Task 8: HierarchyCommands

**Files:**
- Create: `Source/LevelEditor/Commands/HierarchyCommands.h`
- Create: `Source/LevelEditor/Commands/HierarchyCommands.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/Commands/HierarchyCommands.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/Command.h"
#include "Game/EntityPtr.h"
#include <vector>
#include <string>

// Set the editor folder ID for a set of entities (used by ObjectOutliner folders).
class SetFolderCommand : public Command {
public:
    SetFolderCommand(std::vector<EntityPtr> targets, int8_t folder_id);

    void execute(IEditorAPI& api) override;
    void undo(IEditorAPI& api) override;
    std::string to_string() const override { return "Set Folder"; }

private:
    std::vector<EntityPtr> targets;
    std::vector<int8_t>    prev_ids;
    int8_t                 set_to;
};

// Move an entity's position in its parent's child list
// (for outliner ordering: Next, Prev, First, Last).
class MoveInHierarchyCommand : public Command {
public:
    enum class Direction { Next, Prev, First, Last };

    MoveInHierarchyCommand(EntityPtr target, Direction dir);

    void execute(IEditorAPI& api) override;
    void undo(IEditorAPI& api) override;
    std::string to_string() const override { return "Move In Hierarchy"; }

private:
    EntityPtr target;
    Direction dir;
    int       from_index = 0;
    int       to_index   = 0;
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/Commands/HierarchyCommands.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/Commands/HierarchyCommands.h"
#include "LevelEditor/IEditorAPI.h"
#include "Game/Entity.h"

// ---- SetFolderCommand ----

SetFolderCommand::SetFolderCommand(std::vector<EntityPtr> targets, int8_t folder_id)
    : targets(std::move(targets)), set_to(folder_id) {}

void SetFolderCommand::execute(IEditorAPI& api) {
    prev_ids.resize(targets.size(), 0);
    for (int i = 0; i < (int)targets.size(); ++i) {
        Entity* e = targets[i].get();
        if (e) {
            prev_ids[i] = e->get_folder_id();
            e->set_folder_id(set_to);
        }
    }
}

void SetFolderCommand::undo(IEditorAPI& api) {
    for (int i = 0; i < (int)targets.size() && i < (int)prev_ids.size(); ++i) {
        Entity* e = targets[i].get();
        if (e) e->set_folder_id(prev_ids[i]);
    }
}

// ---- MoveInHierarchyCommand ----

MoveInHierarchyCommand::MoveInHierarchyCommand(EntityPtr target, Direction dir)
    : target(target), dir(dir) {}

void MoveInHierarchyCommand::execute(IEditorAPI& api) {
    Entity* e = target.get();
    if (!e) return;
    // Port the sibling-reorder logic from MovePositionInHierarchy::execute
    // in Source/LevelEditor/Commands.cpp (lines ~182-199).
    // Store from_index and to_index for undo.
}

void MoveInHierarchyCommand::undo(IEditorAPI& api) {
    Entity* e = target.get();
    if (!e) return;
    // Reverse the move: restore from_index using Level's child-list API.
}
#endif
```

> Port the full sibling-reorder logic from `MovePositionInHierarchy` in `Source/LevelEditor/Commands.cpp` into `execute()`/`undo()` above. The logic uses `Entity`'s parent/child sibling list — check `Source/Game/Entity.h` for the relevant child-order methods.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add HierarchyCommands: SetFolder, MoveInHierarchy"
```

---

## Task 9: Tool system (IEditorMode + 3 tools)

**Files:**
- Create: `Source/LevelEditor/Tools/IEditorMode.h`
- Create: `Source/LevelEditor/Tools/SelectionMode.h/.cpp`
- Create: `Source/LevelEditor/Tools/FoliagePaintTool.h/.cpp`
- Create: `Source/LevelEditor/Tools/DecalStampTool.h/.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/Tools/IEditorMode.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <string>

class IEditorAPI;
struct EditorViewportInput;

enum class EditorToolMode { Selection, FoliagePaint, DecalStamp };

class IEditorMode {
public:
    virtual ~IEditorMode() = default;
    virtual void tick(const EditorViewportInput& input, IEditorAPI& api) = 0;
    virtual void on_activate(IEditorAPI& api) {}
    virtual void on_deactivate(IEditorAPI& api) {}
    virtual std::string get_name() const = 0;
};
#endif
```

- [ ] **Step 2: Create SelectionMode, FoliagePaintTool, DecalStampTool headers**

`Source/LevelEditor/Tools/SelectionMode.h`:
```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditor/Tools/IEditorMode.h"

// Default tool: mouse-click selection, drag-marquee select.
// ManipulateTransformTool (ImGuizmo gizmo) is ticked separately by EditorDoc.
class SelectionMode : public IEditorMode {
public:
    void tick(const EditorViewportInput& input, IEditorAPI& api) override;
    void on_activate(IEditorAPI& api) override {}
    void on_deactivate(IEditorAPI& api) override {}
    std::string get_name() const override { return "Selection"; }
};
#endif
```

`Source/LevelEditor/Tools/FoliagePaintTool.h`:
```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditor/Tools/IEditorMode.h"
#include "Game/EntityPtr.h"
#include "Render/RenderObj.h"
#include "Framework/Handle.h"
#include <vector>

class FoliagePaintTool : public IEditorMode {
public:
    ~FoliagePaintTool() override;
    void tick(const EditorViewportInput& input, IEditorAPI& api) override;
    void on_deactivate(IEditorAPI& api) override;
    std::string get_name() const override { return "Foliage Paint"; }
private:
    struct FoliageItem { handle<Render_Object> object; glm::vec3 pos; };
    std::vector<FoliageItem> foliage;
    EntityPtr orb_cursor;
};
#endif
```

`Source/LevelEditor/Tools/DecalStampTool.h`:
```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditor/Tools/IEditorMode.h"
#include "Game/EntityPtr.h"

class DecalStampTool : public IEditorMode {
public:
    ~DecalStampTool() override;
    void tick(const EditorViewportInput& input, IEditorAPI& api) override;
    void on_deactivate(IEditorAPI& api) override;
    std::string get_name() const override { return "Decal Stamp"; }
private:
    float rotation = 0.f, scale = 1.f, depth = 1.f;
    EntityPtr preview;
};
#endif
```

- [ ] **Step 3: Create .cpp stubs**

For each tool `.cpp`, create a minimal implementation that compiles and port the logic from:
- `SelectionMode::tick` — port from `SelectionMode::tick()` in `Source/LevelEditor/EditorDocLocal.h` (~line 775). Replace `doc.` field accesses with `api.` calls.
- `FoliagePaintTool::tick` — port from `FoliagePaintTool::tick()` in `EditorDocLocal.h` (~line 734). Replace `doc.command_mgr->add_command(...)` with `api.submit(...)`.
- `DecalStampTool::tick` — port from `DecalStampTool::tick()` in `EditorDocLocal.h` (~line 757).

Start each `.cpp` with the `#ifdef EDITOR_BUILD` guard and the new-style `tick(const EditorViewportInput& input, IEditorAPI& api)` signature.

- [ ] **Step 4: Add all tool files to vcxproj + compile + commit**

```
git commit -m "Add IEditorMode tool system: Selection, FoliagePaint, DecalStamp"
```

---

## Task 10: UI — EdPropertyGrid

**Files:**
- Create: `Source/LevelEditor/UI/EdPropertyGrid.h`
- Create: `Source/LevelEditor/UI/EdPropertyGrid.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/UI/EdPropertyGrid.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "Framework/PropertyEd.h"
#include "Framework/FnFactory.h"
#include <memory>
#include <cstdint>

class SelectionState;
class ICommandDispatcher;

class EdPropertyGrid {
public:
    EdPropertyGrid(SelectionState& selection, ICommandDispatcher& dispatcher,
                   const FnFactory<IPropertyEditor>& factory);

    void draw(); // call each ImGui frame while the editor is open

private:
    SelectionState&                  selection;
    ICommandDispatcher&              dispatcher;
    PropertyGrid                     grid;
    uint64_t                         selected_component = 0;

    void refresh_grid();
    void draw_components(Entity* entity);
    Entity* get_focused_entity() const;
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/UI/EdPropertyGrid.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/UI/EdPropertyGrid.h"
#include "LevelEditor/Core/SelectionState.h"
#include "LevelEditor/IEditorAPI.h"
#include "LevelEditor/Commands/SetPropertyCommand.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"

EdPropertyGrid::EdPropertyGrid(SelectionState& selection,
                               ICommandDispatcher& dispatcher,
                               const FnFactory<IPropertyEditor>& factory)
    : selection(selection), dispatcher(dispatcher), grid(factory)
{
    selection.on_selection_changed.add(this, [this]() { refresh_grid(); });
}

void EdPropertyGrid::draw() {
    // Port the ImGui draw body from EdPropertyGrid::draw() in EditorDocLocal.cpp.
    //
    // IMPORTANT — property undo hook:
    // Before calling grid.update() (or the equivalent property-change path),
    // serialize the focused entity:
    //   Entity* e = get_focused_entity();
    //   auto before = NewSerialization::serialize_to_text("prop", {e}, true);
    // After the change:
    //   auto after = NewSerialization::serialize_to_text("prop", {e}, true);
    //   dispatcher.submit(std::make_unique<SetPropertyCommand>(
    //       e->get_instance_id(), prop_name, before.text, after.text));
    //
    // Check PropertyGrid for a pre/post change callback mechanism, or wrap
    // the ImGui property widgets yourself to intercept before/after.
}

void EdPropertyGrid::refresh_grid() {
    grid.clear_all();
    if (!selection.has_only_one()) return;
    Entity* e = selection.get_only_one().get();
    if (!e) return;
    const PropertyInfoList* list = e->get_type().props;
    if (list) grid.add_property_list_to_grid(list, e);
}

Entity* EdPropertyGrid::get_focused_entity() const {
    if (!selection.has_only_one()) return nullptr;
    return selection.get_only_one().get();
}
#endif
```

> The full ImGui draw loop, component tab, context menus, etc. should be ported from `EdPropertyGrid::draw()` in `Source/LevelEditor/EditorDocLocal.cpp`. The key structural change is the constructor signature and the property-undo hook described in the comment above.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add EdPropertyGrid: takes SelectionState& + ICommandDispatcher&, hooks property undo"
```

---

## Task 11: UI — ObjectOutliner

**Files:**
- Create: `Source/LevelEditor/UI/ObjectOutliner.h`
- Create: `Source/LevelEditor/UI/ObjectOutliner.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/UI/ObjectOutliner.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <memory>

class SelectionState;
class ICommandDispatcher;

class ObjectOutliner {
public:
    ObjectOutliner(SelectionState& selection, ICommandDispatcher& dispatcher);
    ~ObjectOutliner();

    void draw(); // call each ImGui frame

    // Call whenever entities are created or deleted so the tree is rebuilt.
    void notify_scene_changed();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/UI/ObjectOutliner.cpp`**

Port the implementation from `Source/LevelEditor/ObjectOutliner.cpp` and the `ObjectOutliner` class defined in `Source/LevelEditor/EditorDocLocal.h` (lines ~61–163).

Key change: constructor receives `SelectionState& selection, ICommandDispatcher& dispatcher` instead of `EditorDoc& ed_doc`. Replace all `ed_doc.selection_state->` calls with `selection.` and `ed_doc.command_mgr->add_command(...)` with `dispatcher.submit(...)`.

The `Impl` struct carries the tree state (root node, filter, texture handles) — identical to what the old class had privately.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add ObjectOutliner: decoupled from EditorDoc, takes SelectionState& + ICommandDispatcher&"
```

---

## Task 12: UI — UndoHistoryPanel

**Files:**
- Create: `Source/LevelEditor/UI/UndoHistoryPanel.h`
- Create: `Source/LevelEditor/UI/UndoHistoryPanel.cpp`

- [ ] **Step 1: Create `Source/LevelEditor/UI/UndoHistoryPanel.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD

class UndoRedoSystem;

class UndoHistoryPanel {
public:
    explicit UndoHistoryPanel(UndoRedoSystem& undo);
    void draw(); // shows the history list; click to jump_to()
private:
    UndoRedoSystem& undo;
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/UI/UndoHistoryPanel.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/UI/UndoHistoryPanel.h"
#include "LevelEditor/Core/UndoRedoSystem.h"
#include "External/imgui/imgui.h"

UndoHistoryPanel::UndoHistoryPanel(UndoRedoSystem& undo) : undo(undo) {}

void UndoHistoryPanel::draw() {
    if (!ImGui::Begin("Undo History")) { ImGui::End(); return; }
    auto history = undo.get_history();
    for (int i = (int)history.size() - 1; i >= 0; --i) {
        const auto& entry = history[i];
        bool selected = entry.is_current_position;
        if (ImGui::Selectable(entry.label.c_str(), selected))
            undo.jump_to(i);
        if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::End();
}
#endif
```

> Check the ImGui include path for this project — may be `"imgui.h"`, `"External/imgui/imgui.h"`, or another path. Match what other files in `Source/LevelEditor/` use.

- [ ] **Step 3: Add to vcxproj + compile + commit**

```
git commit -m "Add UndoHistoryPanel: shows history list, click to jump_to"
```

---

## Task 13: New EditorDoc (thin orchestrator)

**Files:**
- Create: `Source/LevelEditor/EditorDoc.h`
- Create: `Source/LevelEditor/EditorDoc.cpp`

This is the biggest task. `EditorDoc` keeps `IEditorTool` (so the engine's `EditorState` continues to work) and adds `IEditorAPI`.

- [ ] **Step 1: Create `Source/LevelEditor/EditorDoc.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"          // engine-facing interface (keep for EditorState)
#include "LevelEditor/IEditorAPI.h"
#include "LevelEditor/EditorViewportInput.h"
#include <memory>
#include <optional>
#include <string>
#include <functional>

class UndoRedoSystem;
class SelectionState;
class EditorCamera;
class EdPropertyGrid;
class ObjectOutliner;
class UndoHistoryPanel;
class SelectionMode;
class FoliagePaintTool;
class DecalStampTool;
class DragDropPreview;
class IEditorMode;
template <typename T> class FnFactory;
class IPropertyEditor;

class EditorDoc : public IEditorTool, public IEditorAPI {
public:
    static EditorDoc* create_scene(std::optional<std::string> path);
    ~EditorDoc() override;

    // ---- IEditorTool ----
    void tick(float dt) override;
    void imgui_draw() override;
    const View_Setup* get_vs() override;
    bool save_document_internal() override;
    void hook_menu_bar() override;
    void hook_imgui_newframe() override;
    void hook_scene_viewport_draw() override;
    void hook_pre_scene_viewport_draw() override;
    bool wants_scene_viewport_menu_bar() const override { return true; }
    const char* get_save_file_extension() const override { return "tmap"; }
    std::string get_doc_name() const override;

    // ---- ICommandDispatcher ----
    void submit(std::unique_ptr<Command> cmd) override;
    void command_group_start(std::string label) override;
    void command_group_end() override;
    void undo() override;
    void redo() override;

    // ---- ISceneQuery ----
    const SelectionState& get_selection() const override;
    Entity* get_entity(uint64_t handle) const override;
    void for_each_entity(std::function<void(Entity*)> fn) const override;

    // ---- IEntityMutator ----
    Entity* spawn_entity(EntityPtr parent = {}) override;
    void destroy_entity(EntityPtr) override;
    void insert_scene(UnserializedSceneFile&) override;

    // ---- IEditorAPI extras ----
    SelectionState& get_selection_mutable() override;
    EditorViewportInput get_viewport_input() const override;

    void set_active_tool(EditorToolMode mode);

    // Legacy accessors used by EditorTestContext (can be removed once test
    // context is fully updated in Task 14):
    void set_document_path(const std::string& path);

private:
    EditorDoc();
    void init(std::optional<std::string> path);
    EditorViewportInput build_viewport_input() const;

    std::unique_ptr<UndoRedoSystem>   undo_redo;
    std::unique_ptr<SelectionState>   selection;
    std::unique_ptr<EditorCamera>     camera;
    std::unique_ptr<EdPropertyGrid>   prop_grid;
    std::unique_ptr<ObjectOutliner>   outliner;
    std::unique_ptr<UndoHistoryPanel> history_panel;
    std::unique_ptr<DragDropPreview>  drag_drop;

    std::unique_ptr<SelectionMode>    tool_selection;
    std::unique_ptr<FoliagePaintTool> tool_foliage;
    std::unique_ptr<DecalStampTool>   tool_decal;
    IEditorMode*                      active_tool = nullptr;

    std::unique_ptr<FnFactory<IPropertyEditor>> prop_factory;
    std::optional<std::string>         asset_name;
    View_Setup                         vs_setup;

    EditorViewportInput                last_viewport_input;
};
#endif
```

- [ ] **Step 2: Create `Source/LevelEditor/EditorDoc.cpp`**

Implement by porting `EditorDocLocal.cpp`. Key wiring:

```cpp
#ifdef EDITOR_BUILD
#include "LevelEditor/EditorDoc.h"
#include "LevelEditor/Core/SelectionState.h"
#include "LevelEditor/Core/UndoRedoSystem.h"
#include "LevelEditor/Core/EditorCamera.h"
#include "LevelEditor/Commands/EntityCommands.h"
#include "LevelEditor/Tools/IEditorMode.h"
#include "LevelEditor/Tools/SelectionMode.h"
#include "LevelEditor/Tools/FoliagePaintTool.h"
#include "LevelEditor/Tools/DecalStampTool.h"
#include "LevelEditor/UI/EdPropertyGrid.h"
#include "LevelEditor/UI/ObjectOutliner.h"
#include "LevelEditor/UI/UndoHistoryPanel.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include "Level.h"
// ... (port remaining includes from EditorDocLocal.cpp)

EditorDoc* EditorDoc::create_scene(std::optional<std::string> path) {
    auto* doc = new EditorDoc();
    doc->init(path);
    return doc;
}

EditorDoc::EditorDoc() = default;
EditorDoc::~EditorDoc() = default;

void EditorDoc::init(std::optional<std::string> path) {
    asset_name = path;

    // Build modules in dependency order.
    selection     = std::make_unique<SelectionState>();
    undo_redo     = std::make_unique<UndoRedoSystem>(*this);
    camera        = std::make_unique<EditorCamera>();
    prop_factory  = std::make_unique<FnFactory<IPropertyEditor>>();
    // Register custom property editors (port from PropertyEditors.cpp init).

    prop_grid     = std::make_unique<EdPropertyGrid>(*selection, *this, *prop_factory);
    outliner      = std::make_unique<ObjectOutliner>(*selection, *this);
    history_panel = std::make_unique<UndoHistoryPanel>(*undo_redo);

    tool_selection = std::make_unique<SelectionMode>();
    tool_foliage   = std::make_unique<FoliagePaintTool>();
    tool_decal     = std::make_unique<DecalStampTool>();
    set_active_tool(EditorToolMode::Selection);

    if (path) eng->load_level(path->c_str());
}

void EditorDoc::tick(float dt) {
    last_viewport_input = build_viewport_input();
    camera->tick(last_viewport_input);
    if (active_tool) active_tool->tick(last_viewport_input, *this);
    vs_setup = camera->make_view();
}

void EditorDoc::imgui_draw() {
    prop_grid->draw();
    outliner->draw();
    history_panel->draw();
    // Port remaining ImGui panels from EditorDocLocal::imgui_draw().
}

const View_Setup* EditorDoc::get_vs() { return &vs_setup; }

EditorViewportInput EditorDoc::build_viewport_input() const {
    // Port from EditorDocLocal — read UiSystem::inst / Input:: globals here
    // (this is the ONE place in the new design where global input is touched).
    // Fill in all fields of EditorViewportInput from the current frame.
    // cursor_ray: camera->unproject(mouse_pos_ndc)
    EditorViewportInput inp;
    // ... populate inp from Input:: and UiSystem::inst
    inp.cursor_ray = camera->unproject(inp.mouse_pos_ndc);
    return inp;
}

// ---- ICommandDispatcher ----
void EditorDoc::submit(std::unique_ptr<Command> cmd)    { undo_redo->submit(std::move(cmd)); }
void EditorDoc::command_group_start(std::string label)  { undo_redo->command_group_start(std::move(label)); }
void EditorDoc::command_group_end()                     { undo_redo->command_group_end(); }
void EditorDoc::undo()                                  { undo_redo->undo(); }
void EditorDoc::redo()                                  { undo_redo->redo(); }

// ---- ISceneQuery ----
const SelectionState& EditorDoc::get_selection() const  { return *selection; }
SelectionState&  EditorDoc::get_selection_mutable()     { return *selection; }
EditorViewportInput EditorDoc::get_viewport_input() const { return last_viewport_input; }

Entity* EditorDoc::get_entity(uint64_t handle) const {
    return static_cast<Entity*>(eng->get_level()->get_entity(handle));
}

void EditorDoc::for_each_entity(std::function<void(Entity*)> fn) const {
    // Port from existing EditorDoc — iterate the level's object list.
    // E.g.: eng->get_level()->for_each_entity(fn);
    // Check Level.h for the correct iteration method.
}

// ---- IEntityMutator ----
Entity* EditorDoc::spawn_entity(EntityPtr parent) {
    // Port from existing EditorDoc::spawn_entity().
    // Returns the new Entity*.
}

void EditorDoc::destroy_entity(EntityPtr ptr) {
    Entity* e = ptr.get();
    if (e) e->destroy();
}

void EditorDoc::insert_scene(UnserializedSceneFile& file) {
    eng->get_level()->insert_unserialized_entities_into_level(file);
    // Notify outliner.
    if (outliner) outliner->notify_scene_changed();
}

// ---- IEditorTool save/load ----
bool EditorDoc::save_document_internal() {
    // Port from EditorDocLocal::save_document_internal().
    return true;
}

void EditorDoc::set_active_tool(EditorToolMode mode) {
    if (active_tool) active_tool->on_deactivate(*this);
    switch (mode) {
        case EditorToolMode::Selection:    active_tool = tool_selection.get(); break;
        case EditorToolMode::FoliagePaint: active_tool = tool_foliage.get(); break;
        case EditorToolMode::DecalStamp:   active_tool = tool_decal.get(); break;
    }
    if (active_tool) active_tool->on_activate(*this);
}

void EditorDoc::set_document_path(const std::string& path) { asset_name = path; }
std::string EditorDoc::get_doc_name() const { return asset_name.value_or("<unnamed>"); }
#endif
```

> Port all remaining methods from `EditorDocLocal.cpp`. The key rule: methods in the new `EditorDoc.cpp` may read global input (`Input::`, `UiSystem::inst`) only inside `build_viewport_input()`. Everything else receives data from modules via interfaces.

- [ ] **Step 3: Add to vcxproj**

```xml
<ClInclude Include="Source\LevelEditor\EditorDoc.h" />
<ClCompile Include="Source\LevelEditor\EditorDoc.cpp" />
```

- [ ] **Step 4: Build**

Run: `powershell.exe Scripts/build_and_test.ps1`

Fix all compilation errors. Common issues:
- Missing includes in new headers
- Method name mismatches (check `Entity.h`, `Level.h`, `EntityPtr.h` for exact signatures)
- `IEditorTool` virtual method signatures must match `Source/IEditorTool.h` exactly

- [ ] **Step 5: Commit**

```
git commit -m "Add new thin-orchestrator EditorDoc implementing IEditorAPI + IEditorTool"
```

---

## Task 14: Update EditorTestContext + switch to new EditorDoc

**Files:**
- Modify: `Source/IntegrationTests/EditorTestContext.h`
- Modify: `Source/IntegrationTests/EditorTestContext.cpp`
- Modify: `Source/IntegrationTests/Tests/Editor/test_serialize.cpp`

- [ ] **Step 1: Update `Source/IntegrationTests/EditorTestContext.h`**

```cpp
#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <cstdint>

class EditorTestContext {
public:
    // Number of entities currently in the level.
    int entity_count() const;

    // Save the current level to a game-relative path.
    void save_level(const char* path);

    // Undo one step.
    void undo();

    // Count of entities currently selected.
    int selection_count() const;

    // Number of entries in the undo history.
    int undo_history_size() const;
};
#endif
```

- [ ] **Step 2: Update `Source/IntegrationTests/EditorTestContext.cpp`**

```cpp
#ifdef EDITOR_BUILD
#include "EditorTestContext.h"
#include "GameEngineLocal.h"
#include "LevelEditor/EditorDoc.h"   // <-- was EditorDocLocal.h
#include "Level.h"
#include <cassert>

static EditorDoc* get_doc() {
    assert(eng_local.editorState && "not in editor mode");
    auto* doc = static_cast<EditorDoc*>(eng_local.editorState->get_tool());
    assert(doc);
    return doc;
}

int EditorTestContext::entity_count() const {
    return (int)eng->get_level()->get_all_objects().num_used;
}

void EditorTestContext::save_level(const char* path) {
    EditorDoc* doc = get_doc();
    doc->set_document_path(path);
    doc->save_document_internal();
}

void EditorTestContext::undo() {
    get_doc()->undo(); // now calls IEditorAPI::undo() → UndoRedoSystem
}

int EditorTestContext::selection_count() const {
    return (int)get_doc()->get_selection().get_handles().size();
}

int EditorTestContext::undo_history_size() const {
    // UndoRedoSystem::get_history() is on the undo_redo member which is private.
    // Either expose a method on IEditorAPI or cast through EditorDoc.
    // For now: cast to EditorDoc* and call a dedicated accessor.
    // Add `int get_undo_history_size() const;` to EditorDoc.h if needed.
    return 0; // TODO: expose from EditorDoc
}
#endif
```

> Add `int get_undo_history_size() const { return (int)undo_redo->get_history().size(); }` to `EditorDoc.h`'s public interface, then use it in `undo_history_size()` above.

- [ ] **Step 3: Build + run existing editor tests**

Run: `powershell.exe Scripts/build_and_test.ps1`

The existing `editor/serialize_round_trip` and `editor/undo_noop` tests must pass. Fix any failures before continuing.

- [ ] **Step 4: Commit**

```
git commit -m "Update EditorTestContext to use new EditorDoc/IEditorAPI"
```

---

## Task 15: Integration test — spawn, undo, command groups

**Files:**
- Create: `Source/IntegrationTests/Tests/Editor/test_editor_undo.cpp`

- [ ] **Step 1: Write the test file**

```cpp
// Source/IntegrationTests/Tests/Editor/test_editor_undo.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"

// Spawn an entity, verify count increases, undo, verify count restores.
static TestTask test_spawn_undo(TestContext& t) {
    eng->load_level("Data/demo_level_1.tmap");
    co_await t.wait_ticks(1);

    int before = t.editor().entity_count();
    t.editor().spawn_entity(); // new method to add to EditorTestContext
    co_await t.wait_ticks(1);

    t.check(t.editor().entity_count() == before + 1, "entity count increased after spawn");

    t.editor().undo();
    co_await t.wait_ticks(1);

    t.check(t.editor().entity_count() == before, "entity count restored after undo");
}
EDITOR_TEST("editor/spawn_undo", 15.f, test_spawn_undo);

// Undo with empty history must not crash and must leave entity count unchanged.
static TestTask test_undo_empty_history(TestContext& t) {
    eng->load_level("Data/demo_level_1.tmap");
    int before = t.editor().entity_count();

    t.editor().undo();
    co_await t.wait_ticks(1);

    t.check(t.editor().entity_count() == before, "undo with empty history is a no-op");
}
EDITOR_TEST("editor/undo_empty_history", 10.f, test_undo_empty_history);

// command_group_start/end produces a single undo entry that undoes both spawns.
static TestTask test_command_group(TestContext& t) {
    eng->load_level("Data/demo_level_1.tmap");
    co_await t.wait_ticks(1);

    int before = t.editor().entity_count();
    t.editor().group_spawn_two(); // new helper: group_start, spawn, spawn, group_end
    co_await t.wait_ticks(1);

    t.check(t.editor().entity_count() == before + 2, "two spawned in group");

    t.editor().undo(); // one undo should undo both
    co_await t.wait_ticks(1);

    t.check(t.editor().entity_count() == before, "single undo reverts both group spawns");
}
EDITOR_TEST("editor/command_group", 15.f, test_command_group);
```

- [ ] **Step 2: Add `spawn_entity()` and `group_spawn_two()` to `EditorTestContext`**

In `EditorTestContext.h` add:
```cpp
void spawn_entity();
void group_spawn_two();
```

In `EditorTestContext.cpp` add:
```cpp
#include "LevelEditor/Commands/EntityCommands.h"

void EditorTestContext::spawn_entity() {
    get_doc()->submit(std::make_unique<SpawnEntityCommand>());
}

void EditorTestContext::group_spawn_two() {
    EditorDoc* doc = get_doc();
    doc->command_group_start("Spawn Two");
    doc->submit(std::make_unique<SpawnEntityCommand>());
    doc->submit(std::make_unique<SpawnEntityCommand>());
    doc->command_group_end();
}
```

- [ ] **Step 3: Add test file to vcxproj**

```xml
<ClCompile Include="Source\IntegrationTests\Tests\Editor\test_editor_undo.cpp" />
```

- [ ] **Step 4: Build, then run the new tests**

Run: `powershell.exe Scripts/build_and_test.ps1`
Expected: `editor/spawn_undo`, `editor/undo_empty_history`, `editor/command_group` all PASS.

- [ ] **Step 5: Commit**

```
git commit -m "Add editor integration tests: spawn+undo, empty undo, command groups"
```

---

## Task 16: Integration test — selection

**Files:**
- Create: `Source/IntegrationTests/Tests/Editor/test_editor_selection.cpp`

- [ ] **Step 1: Write the test file**

```cpp
// Source/IntegrationTests/Tests/Editor/test_editor_selection.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"

// Spawn two entities, select each, verify count; clear, verify count 0.
static TestTask test_selection_add_clear(TestContext& t) {
    eng->load_level("Data/demo_level_1.tmap");
    co_await t.wait_ticks(1);

    t.editor().spawn_entity();
    t.editor().spawn_entity();
    co_await t.wait_ticks(1);

    t.editor().select_last_spawned(); // new helper — see below
    t.check(t.editor().selection_count() == 1, "one entity selected");

    t.editor().clear_selection();
    t.check(t.editor().selection_count() == 0, "selection cleared");
}
EDITOR_TEST("editor/selection_add_clear", 15.f, test_selection_add_clear);

// Validate prunes stale handles after an entity is destroyed.
static TestTask test_selection_validate(TestContext& t) {
    eng->load_level("Data/demo_level_1.tmap");
    co_await t.wait_ticks(1);

    t.editor().spawn_entity();
    co_await t.wait_ticks(1);

    t.editor().select_last_spawned();
    t.check(t.editor().selection_count() == 1, "spawned entity selected");

    t.editor().undo(); // destroys the spawned entity
    co_await t.wait_ticks(1);

    t.editor().validate_selection();
    t.check(t.editor().selection_count() == 0, "stale selection pruned after entity destroyed");
}
EDITOR_TEST("editor/selection_validate", 15.f, test_selection_validate);
```

- [ ] **Step 2: Add helpers to `EditorTestContext`**

In `EditorTestContext.h`:
```cpp
void clear_selection();
void select_last_spawned();
void validate_selection();
```

In `EditorTestContext.cpp`:
```cpp
void EditorTestContext::clear_selection() {
    get_doc()->get_selection_mutable().clear();
}

void EditorTestContext::select_last_spawned() {
    // Iterate entities and select the one with the highest handle (most recently spawned).
    uint64_t latest = 0;
    get_doc()->for_each_entity([&](Entity* e) {
        if (e->get_instance_id() > latest) latest = e->get_instance_id();
    });
    if (latest) get_doc()->get_selection_mutable().set_only(EntityPtr{ latest });
}

void EditorTestContext::validate_selection() {
    get_doc()->get_selection_mutable().validate();
}
```

- [ ] **Step 3: Add to vcxproj + build + run + commit**

Run: `powershell.exe Scripts/build_and_test.ps1`
Expected: both selection tests PASS.

```
git commit -m "Add editor selection integration tests: add/clear, validate after destroy"
```

---

## Task 17: Delete old code and update AGENTS.md

**Files:**
- Delete: `Source/LevelEditor/EditorDocLocal.h`, `EditorDocLocal.cpp`
- Delete: `Source/LevelEditor/Commands.h`, `Commands.cpp`
- Delete: `Source/LevelEditor/SelectionState.h`
- Delete: `Source/LevelEditor/ObjectOutliner.cpp`, `ObjectOutlineFilter.h`
- Modify: `Source/LevelEditor/AGENTS.md`
- Modify: `CsRemake.vcxproj`, `CsRemake.vcxproj.filters`

- [ ] **Step 1: Remove deleted files from `CsRemake.vcxproj`**

Remove these `<ClCompile>` entries:
```xml
<ClCompile Include="Source\LevelEditor\EditorDocLocal.cpp" />
<ClCompile Include="Source\LevelEditor\Commands.cpp" />
<ClCompile Include="Source\LevelEditor\ObjectOutliner.cpp" />
```

Remove these `<ClInclude>` entries:
```xml
<ClInclude Include="Source\LevelEditor\EditorDocLocal.h" />
<ClInclude Include="Source\LevelEditor\Commands.h" />
<ClInclude Include="Source\LevelEditor\SelectionState.h" />
<ClInclude Include="Source\LevelEditor\ObjectOutlineFilter.h" />
```

- [ ] **Step 2: Delete the old source files**

```bash
trash Source/LevelEditor/EditorDocLocal.h
trash Source/LevelEditor/EditorDocLocal.cpp
trash Source/LevelEditor/Commands.h
trash Source/LevelEditor/Commands.cpp
trash Source/LevelEditor/SelectionState.h
trash Source/LevelEditor/ObjectOutliner.cpp
trash Source/LevelEditor/ObjectOutlineFilter.h
```

(`trash` is the PowerShell alias — see CLAUDE.md.)

- [ ] **Step 3: Build and verify it still compiles**

Run: `powershell.exe Scripts/build_and_test.ps1`
Expected: clean build with no references to deleted files.

- [ ] **Step 4: Update `Source/LevelEditor/AGENTS.md`**

Replace the AGENTS.md content to reflect the new structure:

```markdown
# LevelEditor — AGENTS.md

## Purpose
The level editor module. Provides a modular, interface-driven scene editor with guaranteed undo coverage for all mutations.

## Key Architecture

### Public API surface: `IEditorAPI.h`
- `ICommandDispatcher` — submit commands, undo/redo, command groups
- `ISceneQuery` — read-only scene access (get_entity, for_each_entity, get_selection)
- `IEntityMutator` — spawn/destroy/insert_scene
- `IEditorAPI` — aggregates all three + get_selection_mutable + get_viewport_input

### Thin orchestrator: `EditorDoc.h/.cpp`
- Implements both `IEditorTool` (engine-facing) and `IEditorAPI`
- Owns all module instances; nothing outside `EditorDoc` depends on `EditorDoc`
- `build_viewport_input()` is the only place global input state is read

### Core modules (`Core/`)
- `SelectionState` — standalone; fires `on_selection_changed` delegate
- `UndoRedoSystem` — `unique_ptr<Command>` history; supports grouping and `try_merge`
- `EditorCamera` — orbit/ortho; takes `EditorViewportInput`, no global input

### Commands (`Commands/`)
- `Command` base: `execute(IEditorAPI&)`, `undo(IEditorAPI&)`, `try_merge`
- `EntityCommands`: `SpawnEntityCommand`, `DestroyEntitiesCommand`, `DuplicateEntitiesCommand`
- `TransformCommand` — try_merge collapses rapid drags
- `SetPropertyCommand` — entity-snapshot-based; try_merge by handle + prop_path
- `HierarchyCommands`: `SetFolderCommand`, `MoveInHierarchyCommand`

### Tools (`Tools/`)
- `IEditorMode` — `tick(EditorViewportInput&, IEditorAPI&)`
- `SelectionMode`, `FoliagePaintTool`, `DecalStampTool`

### UI Panels (`UI/`)
- `EdPropertyGrid(SelectionState&, ICommandDispatcher&)` — auto-submits SetPropertyCommand
- `ObjectOutliner(SelectionState&, ICommandDispatcher&)`
- `UndoHistoryPanel(UndoRedoSystem&)` — click to jump_to()

## Integration Tests
See `Source/IntegrationTests/Tests/Editor/` for:
- `test_serialize.cpp` — round-trip save/load
- `test_editor_undo.cpp` — spawn+undo, command groups
- `test_editor_selection.cpp` — add/clear/validate selection
```

- [ ] **Step 5: Commit**

```
git commit -m "Delete old EditorDocLocal, Commands, SelectionState; update AGENTS.md for new structure"
```

---

## Task 18: Final build and full test run

- [ ] **Step 1: Run clang-format**

```bash
powershell.exe clang-format-all.ps1
```

Fix any formatting violations.

- [ ] **Step 2: Run all tests**

Run: `powershell.exe Scripts/build_and_test.ps1`

All of these must PASS:
- `editor/serialize_round_trip`
- `editor/undo_noop`
- `editor/spawn_undo`
- `editor/undo_empty_history`
- `editor/command_group`
- `editor/selection_add_clear`
- `editor/selection_validate`

Fix any failures before committing.

- [ ] **Step 3: Final commit**

```
git commit -m "Level editor redesign complete: IEditorAPI, modular commands, guaranteed undo, integration tests passing"
```

---

## Self-Review Checklist

**Spec coverage:**

| Spec requirement | Task(s) |
|---|---|
| ICommandDispatcher / ISceneQuery / IEntityMutator interfaces | Task 1 |
| Command base with execute(IEditorAPI&), undo(IEditorAPI&), try_merge | Task 1 |
| SelectionState standalone, on_selection_changed delegate | Task 2 |
| UndoRedoSystem: grouping, merging, 256-entry limit, history panel API | Task 3 |
| EditorCamera: orbit/ortho, takes EditorViewportInput | Task 4 |
| EntityCommands: Spawn, Destroy, Duplicate | Task 5 |
| TransformCommand with try_merge | Task 6 |
| SetPropertyCommand with try_merge | Task 7 |
| HierarchyCommands: SetFolder, MoveInHierarchy | Task 8 |
| IEditorMode enum + 3 tools | Task 9 |
| EdPropertyGrid(SelectionState&, ICommandDispatcher&) | Task 10 |
| ObjectOutliner(SelectionState&, ICommandDispatcher&) | Task 11 |
| UndoHistoryPanel(UndoRedoSystem&) with jump_to | Task 12 |
| EditorDoc thin orchestrator, implements both IEditorTool and IEditorAPI | Task 13 |
| Remove EditorDocLocal / old Commands | Task 17 |
| Integration tests: undo, selection, command groups | Tasks 15–16 |

**No placeholders:** All stub method bodies that say "port from X" reference a concrete existing file. The implementing agent must read that file and port the logic — not guess.

**Type consistency:** `SelectionState`, `UndoRedoSystem`, `IEditorAPI`, `Command`, `EditorViewportInput` are used consistently across all tasks. The `EntityPtr::handle` field and `Entity::get_instance_id()` exact names must be verified against `Source/Game/EntityPtr.h` and `Source/Game/Entity.h` in Task 2 (Step 2 note) — fix in all tasks if the name differs.

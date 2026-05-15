# REFLECT() Macro Flags

Reference for every keyword accepted inside a `REFLECT(...)` macro. The codegen
parser is the source of truth — see `Scripts/codegen_lib.py` `parse_reflect_macro`.

Two forms:
- **flag** — bare keyword, e.g. `REFLECT(transient)`.
- **kv** — `key = value`, e.g. `REFLECT(min = 0, max = 1)`. Value is a number or quoted string.

Multiple args are comma-separated. Multi-line `REFLECT(...)` is allowed.

## Identity / Type Overrides

| Keyword | Form | Effect |
|---|---|---|
| `name = "X"` | kv | Override emitted property name. Default = C++ identifier. Use when the serialized name must differ from the member. |
| `type = "X"` | kv | Sets `PropertyInfo::custom_type_str`. Drives custom editor widgets and asset-pointer typing (e.g. specifying which subclass of `IAsset` a `string` field refers to). |

## Serialization & Editor Flags

`get_flags()` translates these to `PROP_DEFAULT` / `PROP_EDITABLE` / `PROP_SERIALIZE` / `0`.

| Keyword | Form | Effect |
|---|---|---|
| `transient` | flag | Editable in editor, **not serialized** (emits `PROP_EDITABLE` only). |
| `hide` | flag | Serialized, **not editable**, **and not script-accessible** (emits `PROP_SERIALIZE`; clears `script_read`/`script_write`). |
| `edit_hide` | flag | Serialized, **not editable**, but scripts can still read/write. |

## Editor Widget Attributes (PropertyAttributes)

These populate the `PropertyAttributes` struct on `PropertyInfo` and are consumed by
`Source/Framework/PropertyEdWidgets.cpp` (DragFloat/DragInt clamping) and
`Source/Framework/PropertyEd.cpp` (hidden/readonly handling).

| Keyword | Form | Effect |
|---|---|---|
| `tooltip = "..."` | kv | Hover text shown in PropertyEd. |
| `category = "..."` | kv | Group label / folding section in PropertyEd. |
| `min = N` | kv | Lower clamp for int/float widgets. Default sentinel = `-FLT_MAX`. |
| `max = N` | kv | Upper clamp. Default sentinel = `+FLT_MAX`. |
| `step = N` | kv | Drag step. `0` disables stepping. |
| `hidden` | flag | PropertyEd skips drawing the widget. Property is still serialized and script-accessible. Lives in `PropertyAttributes.hidden`. |
| `readonly` | flag | PropertyEd draws the widget disabled. Serialized + scriptable as normal. Lives in `PropertyAttributes.readonly`. |
| `hint = "..."` | kv | **Legacy.** Packed hint string parsed by `parse_hint_str_for_property` (format like `D<def>,M<min>,X<max>,S<step>`). Prefer `min`/`max`/`step` for new code. |

`min`/`max` apply to both float and int properties — codegen stores them as `float` and
the int widget converts.

## Scripting (Lua Stub) Flags

Control what `Scripts/codegen_generate.py` emits into `Data/scripts/lua_stubs.lua`
and what the runtime allows scripts to do. See [[scripting_system]].

| Keyword | Form | Effect |
|---|---|---|
| `script_read` | flag | Scripts may read but not write. |
| `script_write` | flag | Scripts may write but not read. |
| `script_hide` | flag | Hide from scripting entirely (editor unaffected). |
| `lua_generic` | flag | Lua stub uses `---@generic T` for return/param type — for methods that return the same type they take (e.g. `cast<T>(T) -> T`). |
| `no_nil` | flag | Suppress the `\|nil` suffix on pointer return/arg/field types in the lua stub. Use when the value is guaranteed non-null (asserted / always-initialized). |
| `getter` | flag | Marks the REFLECT'd method as the getter half of a property pair (drives lua stub property emission). |
| `setter` | flag | Same, for the setter half. |

## Where This Is Implemented

- **Parser:** `Scripts/codegen_lib.py` — `parse_reflect_macro` (~L250-313). Adding a new flag means adding a branch here.
- **Property struct:** `Source/Framework/ReflectionProp.h` — `PropertyInfo`, `PropertyAttributes`.
- **REG_* helper macros + `apply_attrs()`:** `Source/Framework/ReflectionMacros.h`.
- **Attr consumers:**
  - Widget clamping/disabled state: `Source/Framework/PropertyEdWidgets.cpp`.
  - Hidden/readonly handling + group/category: `Source/Framework/PropertyEd.cpp`, `Source/Framework/PropertyEdGroup.cpp`.
- **Lua stub emitter:** `Scripts/codegen_generate.py` — `get_lua_type_string` (`no_nil`), `write_lua_class` (`lua_generic`, `getter`/`setter`).
- **Generated output:** `Source/.generated/MEGA.gen.cpp`.

## Example

```cpp
REFLECT()
float health = 100.f;

REFLECT(min = 0, max = 1, step = 0.01, tooltip = "Normalized")
float fuel = 1.f;

REFLECT(category = "Audio", readonly)
float current_db = 0.f;

REFLECT(transient)
float frame_accumulator = 0.f;     // editable, not saved

REFLECT(edit_hide, script_read)
int internal_state = 0;            // serialized, read-only from Lua, hidden from editor

REFLECT(no_nil)
Entity* owner = nullptr;           // lua stub: `Entity` instead of `Entity|nil`
```

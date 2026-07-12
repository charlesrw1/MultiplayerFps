# RmlUi integration

RmlUi 6.2 (`core` + `freetype` vcpkg features, no `[lua]` feature — this engine
binds RmlUi to Lua itself, not via RmlUi's own Lua plugin). The render
interface (`Source/Render/RmlUiRenderInterface.h/.cpp`) is implemented
entirely through `IGraphicsDevice` (textures/buffers/vertex-input/pipeline/
push-constants) — no raw graphics-API calls — so it is backend-portable, but
it's currently only instantiated by the OpenGL device; DX11 backend log-warns
and no-ops (`Source/Render/Dx11/Dx11Device.cpp`) since it isn't wired up
there yet.

Draw order per frame: 3D scene -> `Gui::`/`Canvas::` immediate-mode HUD
sprites -> RmlUi documents -> ImGui (editor, always on top). `Gui::`/`Canvas::`
(`Source/UI/Gui.h`, `Source/UI/GUISystemPublic.h`) is a separate lower-level
immediate-mode primitive layer, still used for dynamic/gameplay drawing
(minimap blips, damage numbers, debug overlays). Use RmlUi for anything with a
document/DOM shape (menus, inventories, dialogs, HUD panels with layout).

## Known v1 limitations

- **File-based image loading routes through the engine's asset system.**
  `RmlUiRenderInterface::LoadTexture` calls `Texture::load(source)` — the same
  synchronous, cached `g_assets`-backed load any 3D material texture uses (not
  `force_load_for_ui`, which forces nearest filtering; that's reserved for
  `AssetBrowser` thumbnails) — so `<img src="...">` / RCSS `background-image`
  paths resolve like any other `Data/`-relative texture asset, respecting the
  source's own `.tis` `nearest_filtering` setting instead of always forcing
  it. Straight-alpha source images are premultiplied at sample time in
  `RmlUiF.txt` (see `RmlUiFragPushConsts` in `Shaders/ShaderBufferShared.txt`)
  to match RmlUi core's premultiplied vertex colours and `PREMULT_BLEND`.
- RCSS `transform` (2D/3D, with interpolation/`transition`/`@keyframes`) is
  applied via `RmlUiRenderInterface::SetTransform`, which combines the
  element's `Rml::Matrix4f` with the ortho projection before each
  `RenderGeometry` call. `filter`/`backdrop-filter` and custom
  `Rml::Decorator`/shader effects still parse and animate correctly (RmlUi
  core handles that regardless of backend) but **do not visually apply** —
  the render interface doesn't implement `CompileFilter`/`RenderShader`/
  layer compositing yet. See `Source/Render/RmlUiRenderInterface.h` for the
  extension point if this is needed later (custom decorator ->
  `CompileShader`/`RenderShader` binding a custom GLSL program via
  `gfx().create_shader_vert_frag()`).
- Hot-reloading a changed `.rcss` reloads **every open document** (RmlUi has
  no in-place stylesheet-only reload API exposed on `Context`), not just the
  one stylesheet's dependents. Also requires `Rml::Factory::ClearStyleSheetCache()`
  before the reload - `Context::LoadDocument()` resolves `<link>` stylesheets
  through RmlUi's internal `StyleSheetFactory` cache keyed by source path, so
  re-loading the `.rml` alone silently reuses the stale pre-edit stylesheet
  (`RmlUiSystem::poll_hot_reload()` calls this already; a symptom of
  forgetting it is the `.rml` visibly reloading - e.g. layout/text edits
  apply - while style edits don't).
- **No fonts ship yet.** `RmlUiSystem::init()` loads every `.ttf`/`.otf`
  under `Data/ui/fonts/` via `Rml::LoadFontFace`, but that directory doesn't
  exist in this repo — add font files there for text to actually render.
  The engine's own bitmap `GuiFont` format (`Data/eng/fonts/*.fnt`, used by
  `Gui::`/`Canvas::`) is a different, incompatible format; RmlUi's freetype
  backend needs the real font file.
- At process shutdown, RmlUi logs `Resource was not properly shut down`
  (`ControlledLifetimeResource.h`) several times — a Win32-debug-only assert
  inside `Rml::Shutdown()` for some internal singleton(s) not confirmed
  cleanly released. Non-fatal (log-only, verified via integration test: no
  crash, all resources still functionally torn down), but not yet root-
  caused — a follow-up if it turns out to matter (e.g. leak detection noise).
- **`ElementDocument:Close()` does not synchronously tear down its data
  model** — actual document/model removal is deferred to the `Context`'s own
  `Update()` pass. Calling `ctx:OpenDataModel()` again with the same model
  name right after `Close()` (e.g. a fast open/close/reopen cycle) hits the
  still-registered old model: `Log::Message` warnings like `Data model name
  '...' already exists` / `Data model variable with name '...' already
  exists`, and the new Lua table never actually gets bound — the on-screen
  doc is left pointing at the OLD (about-to-be-destroyed) Lua table. Once
  that table's only Lua reference gets overwritten it's GC'd, and the next
  `data-for`/variable `Update()` (e.g. a bound button click doing
  `table.insert(model.cards, ...)`) dereferences a dead Lua stack slot and
  hard-crashes (`LuaTableDef::Size` → `lua_type` → `index2value`, `READ @
  0xFFFFFFFFFFFFFFFF`). **Fix: call `OpenDataModel`/`LoadDocument` exactly
  once per model name, ever.** Toggling a data-model-backed doc on/off
  afterward is `Show()`/`Hide()` only, never `Close()` + reopen.

## RCSS vs CSS — do not assume standard CSS

RCSS looks like CSS but is a distinct, smaller language. Do not guess
properties from web CSS knowledge; the most common hallucination points:

- **`display: flex` is supported** (RmlUi 6.x), including `flex-direction`,
  `flex-wrap`, `justify-content`, `align-items`/`align-content`,
  `flex-grow`/`flex-shrink`/`flex-basis`, `order`. **No `display: grid`** —
  for grid-like layouts use flexbox, CSS 2.1 `table`/`table-row`/
  `table-cell`, or absolute positioning with `left`/`top`/`right`/`bottom`.
  RmlUi 6.x also supports `transform` (with interpolation), `transition`/
  `@keyframes` animations, and media queries (`@media`).
- **`decorator` is RmlUi-specific**, not a real CSS property. Syntax:
  `decorator: <type>( <args> );`. Built-in types: `tiled-box`, `image`,
  `ninja-patch`, `gradient` — e.g.
  `decorator: gradient( horizontal #ff0000 #0000ff );`
  `decorator: image( sprite-name );` (references a `<sprite>` defined via
  `@spritesheet` at-rule). There is no `background: linear-gradient(...)` —
  that's `decorator: gradient(...)`.
- **`box-shadow` and `drop-shadow` are filters**, not decorators:
  `filter: drop-shadow(2px 2px 4px black);` — but per the limitation above,
  this currently has no visual effect until the render interface implements
  `CompileFilter`/`RenderShader`.
- Units: `px`, `%`, `em`, `rem`, `vw`/`vh`, `dp` (density-independent pixel).
  No `ch`, `fr`, `calc()`.
- Pseudo-classes: `:hover`, `:active`, `:focus`, `:checked`, `:disabled`,
  `:nth-child()`. No `:has()`, `:is()`, `:where()`.
- **`<div>` is NOT block by default** — unlike a browser's UA stylesheet,
  RmlUi's initial `display` value is `inline` for generic elements (matches
  the raw CSS spec, minus the browser HTML defaults layered on top). A
  `<div>` used purely for layout grouping (a "row" wrapper, etc.) needs an
  explicit `display: block;` or its children end up inline on the same line
  as sibling divs instead of stacking. `<body>`, `<button>`, `<input>`,
  `<progress>` etc. do get sensible built-in defaults since they're special
  elements (see the tag list below), but plain `<div>`/`<span>`-equivalents
  don't.
- `<x-only-defined-tags>` — RmlUi ships `<button>`, `<input>`, `<select>`,
  `<textarea>`, `<progress>`, `<tabset>`/`<panel>`, `<handle>` (drag handle)
  as special elements with built-in behavior beyond plain `<div>`.

## Data binding attributes

Bound via `data-model="model_name"` on a container element, then inside it:

- `data-value="field"` — two-way bind an `<input>`'s value to `field`.
- `{{ field }}` inside text content — one-way display binding.
- `data-for="row:array_field"` — repeats the element once per row in
  `array_field`; inside, `{{ row.some_key }}` reads a per-row field. Colon is
  literal RmlUi Core syntax, not `... in ...` (that's a common hallucination
  from web-JS `for...of`/Vue `v-for` habits) — `DataViewFor::Initialize`
  (`Source/Core/DataViewDefault.cpp` in the RmlUi source) splits on `:` into
  iterator-name(s) and container-name; getting this wrong doesn't error, it
  just silently produces zero rows (`Could not find variable name '<whole
  expression>' in data model` is the only symptom, easy to miss). An explicit
  index name is available via a comma before the colon —
  `data-for="row,row_index:array_field"` — but **`{{ }}` substitution only
  works in text content**, not inside a plain attribute like `id=`; only
  `data-*`-prefixed attributes get scanned for bindings at all
  (`ElementUtilities::ApplyDataViewsControllers`), and those take a raw
  `DataExpression` (numbers/`+`/`-`/comparisons/model variables), not `{{ }}`
  template text — `data-attr-id="row_index"` sets `id` to the numeric index,
  not `"row_2"`. Don't rely on generated per-row ids from a `data-for` loop
  unless you've confirmed the expression does what you think.
- `data-attr-<name>="expr"` — bind an element attribute.
- `data-class-<name>="expr"` — toggle a class based on a boolean expression.
- `data-if="expr"` — conditionally include the element.
- `data-event-<name>="key"` — binds to a **function-valued field on the
  data-model table itself** (the one passed to `OpenDataModel`), not a
  global Lua function of that name — `LuaDataModel.cpp`'s `BindVariable()`
  checks `lua_type(dataL, top) == LUA_TFUNCTION` on the table field and
  calls `BindEventCallback` for it. So `data-event-click="on_click"` needs
  `model.on_click = function(event) ... end` set on the table *before*
  `OpenDataModel`, not `function on_click(event) ... end` as a global.
  **Prefer `Element:AddEventListener("click", fn)` from Lua instead of
  this** (see below) — `data-event-<name>`'s callback closure captures a
  stack index into a private "shadow" `lua_State` (`dataL`) that the plugin
  uses to store bound model values, and that index is invalidated whenever
  another field on the same model (e.g. a `data-for`-bound array) gets a new
  element bound after the closure was created — `LuaScalarDef::Child`
  truncates the shared shadow stack on every subsequent array-element bind.
  Concretely: a `data-event-click` handler that does
  `table.insert(model.some_array, ...)` on its OWN model shifts the stack
  out from under its own captured slot — first click after that is a silent
  no-op, the next hard-crashes (`index2value`/`lua_pushvalue`, `READ @
  0xFFFFFFFFFFFFFFFF`, inside `DataExpressionInterface::EventCallback`).
  This is a bug in RmlUi's official Lua plugin itself, not an engine-side
  binding issue - avoid the whole class of failure by giving the element a
  plain `id` and wiring the click via `doc:GetElementById(id):
  AddEventListener("click", fn)` from Lua instead, which stores the Lua
  function via `luaL_ref` on the main `lua_State` (`LuaEventListener.cpp`) -
  a completely separate, stable mechanism unrelated to the data model's
  scratch stack.

## Lua API — RmlUi's official Lua plugin (`rmlui[lua]` vcpkg feature)

This engine used to expose a small hand-written `RmlUi.*` function table
(`Source/UI/RmlUi/RmlUiLua.{h,cpp}` — deleted). It's now RmlUi's own official
Lua bindings instead, initialised once in `RmlUiSystem::init()` via
`Rml::Lua::Initialise(ScriptManager::inst->get_lua_state())`, into the same
`lua_State` every other engine system uses (`ScriptManager`) — not a second
Lua VM. This gives Lua real `Context`/`ElementDocument`/`Element`/`Event`
objects instead of opaque int handles + a fixed function table, so anything
in RmlUi's Lua binding surface is available directly (method/attribute names
match RmlUi's own Lua documentation), not just what this engine hand-picked.

The plugin registers a global `rmlui` table on init; contexts are reached
through `rmlui.contexts[name]` (this engine creates exactly one, named
`"main"`, in `RmlUiSystem::init()`):

```lua
local ctx = rmlui.contexts["main"]
local doc = ctx:LoadDocument("ui/hud.rml")   -- path relative to Data/, real ElementDocument object (or nil on failure)
doc:Show()
doc:Hide()
doc:Close()

-- Elements: real objects, not selector strings re-resolved per call.
local el = doc:GetElementById("card_3")
el.style.left = "120px"            -- SetProperty under the hood
el:SetAttribute("data-count", "3")
el:AddEventListener("click", function(event)
    local target = event.target_element   -- the actual element that fired, not just doc-root - delegation works
    print("clicked", target:GetId())
end)

-- Data models: same data-for/{{ field }} RCSS-side story as before, but
-- constructed via RmlUi's own Lua data model API instead of this engine's
-- generic scalar/array shim. Context:OpenDataModel(name, table) - name must
-- match data-model="hud_model" in the .rml, table fields become RmlUi
-- variables, array-valued fields drive data-for.
--
-- IMPORTANT: capture and use OpenDataModel's RETURN VALUE, not the table
-- literal you passed in. `initial_table` above is walked ONCE (RmlUi's
-- OpenLuaDataModel copies each field's value into its own private state),
-- then discarded - it is NOT kept live. The return value is a separate
-- proxy object whose assignment (__newindex) is what actually marks a
-- field dirty for the next Context:Update() to re-run its bound views;
-- mutating the original table afterward is a silent no-op (no error, view
-- just never refreshes - see the "actual bug" callout below).
local hud = rmlui.contexts["main"]:OpenDataModel("hud_model", {score = 0, cards = {}})
hud.score = 1200                                      -- through the proxy: dirties "score"
local cards = hud.cards                               -- same underlying table RmlUi holds
table.insert(cards, {image = "ui/card1.png", text = "Draw 2"})
hud.cards = cards                                     -- re-assign through the proxy to dirty "cards"
```

Same underlying RmlUi constraint as before, Lua-plugin or not: a document's
`{{ field }}`/`data-for` bindings resolve once at `LoadDocument()` time, so
every field the `.rml` references needs to already exist on the initial
table passed to `OpenDataModel` *before* `LoadDocument()` is called for that
document — `{score = 0, cards = {}}` up front, not added later.

- **The actual bug this caused (display silently never updates):**
  `OpenLuaDataModel` (`Source/Lua/LuaDataModel.cpp`) walks the table you
  pass in with `lua_next` exactly once, copying each field's value into its
  own side-state via `BindVariable`'s `lua_xmove`, then returns a *separate*
  userdata proxy — its metatable's `__index`/`__newindex` are
  `lDataModelGet`/`lDataModelSet`, and it's specifically `lDataModelSet`
  (i.e. an assignment *through the proxy*) that calls
  `DataModelHandle::DirtyVariable(name)`. `DataModel::Update()`
  (`Source/Core/DataModel.cpp`) only re-runs views for variables in its
  `dirty_variables` set — so writing to the original table you passed to
  `OpenDataModel` (`hud.score = 5` where `hud` is that original table, not
  the proxy) changes a plain, now-disconnected Lua table: no crash, no
  warning, the click handler visibly "works" (the Lua value changes) and
  the `{{ }}`/`data-for` bound UI just never moves. For a table-valued field
  like an array, the proxy's `__index` does return the *same* underlying
  table reference RmlUi holds (tables are reference values in Lua), so
  in-place mutation (`table.insert`) does land in the right storage — but
  still doesn't mark it dirty on its own; re-assign it through the proxy
  afterward (`hud.cards = cards`, even though it's the identical reference)
  purely to trigger `DirtyVariable`.

See RmlUi's own Lua binding documentation for the full `Element`/`Document`/
`Event`/`Context`/data-model API surface — this engine no longer limits it to
a hand-picked subset.

- **`ElementFormControlInput`'s `.value`/`.min`/`.max`/`.step` Lua property
  getters return `nil` for a `type="range"` input**, confirmed via a
  throwaway `add_test()` (see `lua/smoke/rmlui_volume_popup` in
  `Data/scripts/tests/rmlui_examples.lua`) that printed them before/after
  both `Element:SetAttribute("value", ...)` and a manual
  `Element:DispatchEvent("change", ...)` — nil every time, despite
  `rmlui_lua_stubs.lua` declaring them (that stub documents RmlUi's intended
  Lua surface, not a verified-working one). Two paths work reliably instead:
  `el.attributes.value`/`.min`/`.max` (raw attribute values — read/write),
  and the `"change"` event's own `event.parameters.value`. Evidence
  `SetAttribute("value", ...)` alone synchronously fires `"change"` suggests
  the widget's internal drag/scroll/keyboard handling also goes through
  `SetAttribute`, so `attributes.value` should track live user interaction
  the same way. See `Data/scripts/demo/rmlui_controls_demo.lua`'s volume
  slider popup for the workaround in practice.

- **A range input's internal `slidertrack`/`sliderbar` elements are NOT
  exposed to the Lua DOM.** RCSS selectors like `input.range sliderbar` style
  them fine, but from Lua the input has *zero* children:
  `slider.first_child`, `slider:GetElementsByTagName("sliderbar")`, and
  `slider:QuerySelector("sliderbar")` all return nothing/empty (confirmed by
  dumping the subtree in `lua/smoke/rmlui_volume_popup`). So you cannot reach
  the thumb to reparent something under it — anything that must follow the
  thumb has to be positioned by Lua from the value (see below), not attached
  to the thumb element.

- **Update dynamic text via a `{{ }}` data-model binding, not `inner_rml`.**
  `Element.inner_rml` destroys and rebuilds the child text node on every
  write; a just-rebuilt `ElementText` renders blank for a frame or two before
  layout, so rewriting a *visible* label every frame (e.g. a value readout
  while dragging) flickers. Bind the text to a data-model variable instead and
  write the variable through the `OpenDataModel` proxy — RmlUi updates the
  existing text node in place, no rebuild, no flicker. The volume popup's `%`
  uses this.

- **`"change"` on a form control is dispatched from inside `Context::Update()`
  (i.e. at `rmlui_update`), not during the input pump.** Input events like
  `"mousemove"` fire synchronously from `ProcessMouseMove` in `frame_start`,
  *before* `Context::Update()`. This matters for anything that reacts to a
  control by writing a style/layout property: a write made in a `"change"`
  handler lands one `Context::Update` behind the control's own re-layout (a
  visible 1-frame trail), whereas the same write in a `"mousemove"` handler is
  pending in time for the *same* layout pass. The volume popup positions from
  `"mousemove"` (lag-free tracking) and only uses `"change"` for the text and
  for non-drag value moves (wheel/keyboard) where a 1-frame settle is
  imperceptible.

## Example: `.rml` + `.rcss` with a `data-for` card list

`Data/ui/cards_test.rml`:

```html
<rml>
<head>
	<link type="text/rcss" href="cards_test.rcss"/>
</head>
<body data-model="hud_model">
	<div id="score_label">Score: {{ score }}</div>
	<div id="card_row">
		<div data-for="card:cards" class="card">
			<img data-attr-src="card.image"/>
			<span>{{ card.text }}</span>
		</div>
	</div>
</body>
</rml>
```

`Data/ui/cards_test.rcss`:

```css
body { width: 100%; height: 100%; }
#score_label { font-size: 24px; color: #ffffff; }
#card_row { display: block; }
.card {
	display: inline-block;
	width: 120px; height: 160px;
	margin: 8px;
	decorator: tiled-box( card-bg );
}
.card:hover { decorator: tiled-box( card-bg-hover ); }
.card span { display: block; text-align: center; }
```

Corresponding Lua (`Data/scripts/`):

```lua
-- All fields the .rml references must already be in the initial table
-- before LoadDocument() (see the ordering note above). `hud` below is the
-- PROXY returned by OpenDataModel - keep using it, not the table literal,
-- for every later read/write (see the callout above for why).
local hud = rmlui.contexts["main"]:OpenDataModel("hud_model", {
    score = 0,
    cards = {
        {image = "ui/card1.png", text = "Draw 2"},
        {image = "ui/card2.png", text = "Skip"},
    },
})

local doc = rmlui.contexts["main"]:LoadDocument("ui/cards_test.rml")
doc:Show()
```

(`data-attr-src` now resolves through the asset system per the note above.
Text itself needs a `.ttf`/`.otf` under `Data/ui/fonts/` loaded via
`Rml::LoadFontFace` at `RmlUiSystem::init()` time — none ships yet, see the
limitation below.)

GAPS:
 Notable deviations/gaps flagged in the doc, not silently swept under the rug:
  - The plan's "delete old UI" list was mostly wrong — those files are live dependencies of Gui::/Canvas::, not dead code. Only two genuinely dead files were removed (confirmed with you first).
  - No fonts ship in the repo, so text won't render until a .ttf/.otf is added under Data/ui/fonts/.
  - `transform` now visually applies (`RmlUiRenderInterface::SetTransform`). Filters/custom shaders still parse but don't visually apply (optional RenderInterface hooks not implemented).
  - A benign Resource was not properly shut down RmlUi log warning at process exit, not yet root-caused (no crash, not a leak in practice).
  - Replaced the hand-rolled `RmlUi.*` Lua bridge (`Source/UI/RmlUi/RmlUiLua.{h,cpp}`,
    `RmlUiDataModel.{h,cpp}` — deleted) with RmlUi's official Lua plugin
    (`rmlui[lua]` vcpkg feature, `Rml::Lua::Initialise` in `RmlUiSystem::init()`).
    Any `Data/scripts/*.lua` written against the old `RmlUi.load_document`/
    `RmlUi.bind_event`/etc. function table needs rewriting against the `rmlui`
    global documented above — the two APIs don't overlap.
  - `Core.vcxproj`'s `MinUnityFiles` bumped 6→8 (Debug/Release/NoEditRelease)
    to compensate for removing 2 `.cpp` files from the unity-build file
    count — some unrelated `Source/Render/DrawLocal_*.cpp` files rely on
    `static ConfigVar`s only being visible because MSVC's unity build
    happens to merge them into the same translation unit as their users;
    changing the total file count reshuffles that grouping. Fragile
    pre-existing coupling, not something this change fixed — if adding/
    removing files from `Core.vcxproj` ever produces `undeclared identifier`
    errors in files you didn't touch, this is why; the previous fix for the
    same symptom (commit 896b2a18) was the same lever.
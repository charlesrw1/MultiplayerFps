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
  `RmlUiRenderInterface::LoadTexture` calls `Texture::force_load_for_ui(source)`
  (same synchronous immediate-load path `AssetBrowser` uses for thumbnails),
  so `<img src="...">` / RCSS `background-image` paths resolve like any other
  `Data/`-relative texture asset.
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
- `data-for="row in array_field"` — repeats the element once per row in
  `array_field`; inside, `{{ row.some_key }}` reads a per-row field.
- `data-attr-<name>="expr"` — bind an element attribute.
- `data-class-<name>="expr"` — toggle a class based on a boolean expression.
- `data-if="expr"` — conditionally include the element.
- `data-event-<name>="LuaFunctionName"` — now works, unlike the old
  hand-rolled bridge: the official plugin wires `data-model`'s
  `BindEventCallback` straight to a global Lua function of that name.

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
-- generic scalar/array shim. Context:OpenDataModel(name, table) binds a
-- plain Lua table directly - name must match data-model="hud_model" in the
-- .rml, table fields become RmlUi variables, array-valued fields drive
-- data-for. No separate get/set-value calls: read/write the table itself.
local hud = {score = 0, cards = {}}
rmlui.contexts["main"]:OpenDataModel("hud_model", hud)
hud.score = 1200
table.insert(hud.cards, {image = "ui/card1.png", text = "Draw 2"})
```

Same underlying RmlUi constraint as before, Lua-plugin or not: a document's
`{{ field }}`/`data-for` bindings resolve once at `LoadDocument()` time, so
every field the `.rml` references needs to already exist on the table passed
to `OpenDataModel` *before* `LoadDocument()` is called for that document —
`hud = {score = 0, cards = {}}` up front, not added later.

See RmlUi's own Lua binding documentation for the full `Element`/`Document`/
`Event`/`Context`/data-model API surface — this engine no longer limits it to
a hand-picked subset.

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
		<div data-for="card in cards" class="card" id="card_{{ card_index }}">
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
-- All fields the .rml references must already be on the table before
-- LoadDocument() (see the ordering note above).
local hud = {
    score = 0,
    cards = {
        {image = "ui/card1.png", text = "Draw 2"},
        {image = "ui/card2.png", text = "Skip"},
    },
}
rmlui.contexts["main"]:OpenDataModel("hud_model", hud)

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
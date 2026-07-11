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
- `data-event-<name>="lua_function_name(...)"` — RmlUi-native inline event
  binding through the data model's `BindEventCallback`. **Not used by this
  engine's bridge** — use `RmlUi.bind_event(...)` from Lua instead (below);
  it stores an arbitrary Lua function reference, not a data-model-scoped
  event name.

## Lua API (`Source/UI/RmlUi/RmlUiLua.h/.cpp`)

Hand-written `lua_CFunction`s on a global `RmlUi` table (not the
`ClassBase`/`REF` codegen path — that only marshals a fixed set of known C++
types, and data-model values are dynamically typed Lua values).

```lua
-- Documents
local doc = RmlUi.load_document("ui/hud.rml")  -- path relative to Data/, returns int handle (0 = failed)
RmlUi.show_document(doc)
RmlUi.hide_document(doc)
RmlUi.close_document(doc)  -- invalidates the handle

-- Data models: one Lua-created model = one Rml::DataModelConstructor,
-- backed by a generic dynamic (name -> value) store (Source/UI/RmlUi/RmlUiDataModel.h).
-- Every set_value/array_push call writes the value AND dirties the RmlUi
-- variable in the same call (push-then-pull-on-demand) -- next Context::Update()
-- (main loop, every frame) pulls the new value into the DOM. No manual dirty step needed.
--
-- ORDERING REQUIREMENT: fields bind lazily, on first set_value/array_push
-- call for that field name. RmlUi resolves a document's {{ field }} /
-- data-for bindings once, at load_document() time. So every field the RML
-- references must be set_value/array_push'd at least once BEFORE
-- load_document() is called for that document, even though the model
-- itself can be created any time before. A field only touched after
-- load_document() silently fails to bind (RmlUi logs "Could not find
-- variable name 'x' in data model" and that element's binding never
-- attaches, even though later set_value/DirtyVariable calls succeed).
local model = RmlUi.create_data_model("hud_model")  -- name must match data-model="hud_model" in the .rml

RmlUi.set_value(model, "score", 1200)         -- scalar field, any Lua bool/number/string
local v = RmlUi.get_value(model, "score")      -- reads current value back (e.g. after an <input> edit)

-- Arrays (data-for): each row is a flat {key=value, ...} table of scalars
RmlUi.array_push(model, "cards", {image = "card1.png", text = "Draw 2"})
RmlUi.array_set(model, "cards", 0, {image = "card2.png", text = "Skip"})  -- 0-based index
RmlUi.array_erase(model, "cards", 0)

-- Per-element property writes (procedural/spring-driven animation -- RmlUi
-- has no physics of its own; call this every tick from a Lua-side update)
RmlUi.set_element_property(doc, "#card_3", "left", "120px")  -- selector: #id or a CSS selector (QuerySelector)

-- Events. callback receives (event_type: string, mouse_x: number, mouse_y: number).
-- Covers click and RmlUi's native drag events (dragstart/drag/dragend/dragover/dragdrop).
RmlUi.bind_event(doc, "#card_3", "dragstart", function(event_type, x, y)
    print("drag started at", x, y)
end)
```

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
-- Bind every field the .rml references BEFORE load_document() (see the
-- ordering requirement above) - model creation can come earlier, but
-- score/cards must be set_value/array_push'd first.
local model = RmlUi.create_data_model("hud_model")
RmlUi.set_value(model, "score", 0)
RmlUi.array_push(model, "cards", {image = "ui/card1.png", text = "Draw 2"})
RmlUi.array_push(model, "cards", {image = "ui/card2.png", text = "Skip"})

local doc = RmlUi.load_document("ui/cards_test.rml")
RmlUi.show_document(doc)
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
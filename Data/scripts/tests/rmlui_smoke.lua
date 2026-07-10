-- RmlUi Lua bridge smoke test: document load/show, data model scalar +
-- array binding, per-element property write, event binding. Exercises the
-- full push-then-pull-on-demand contract described in
-- docs/ui/rmlui_agent_guide.md.

add_test("lua/smoke/rmlui_basic", function()
    -- Every field a document's RML references (via {{ field }} / data-for)
    -- must be BOUND into the model - via set_value/array_push, which bind
    -- lazily on first use - before that document is loaded. RmlUi resolves
    -- data bindings once per element at document-construction time; a field
    -- bound only after load_document() never attaches, even though the
    -- model itself already existed.
    local model = RmlUi.create_data_model("smoke_test_model")
    assert(model ~= nil, "RmlUi.create_data_model failed")

    RmlUi.set_value(model, "score", 42)
    assert(RmlUi.get_value(model, "score") == 42, "set_value/get_value round-trip failed")

    RmlUi.array_push(model, "cards", {text = "Card A"})
    RmlUi.array_push(model, "cards", {text = "Card B"})
    RmlUi.array_set(model, "cards", 0, {text = "Card A2"})
    RmlUi.array_erase(model, "cards", 1)

    local doc = RmlUi.load_document("ui/rmlui_smoke_test.rml")
    assert(doc ~= 0, "RmlUi.load_document failed")

    RmlUi.show_document(doc)

    -- let a frame (or a few) tick so Context::Update()/Render() actually
    -- pull the bound values into the DOM and RmlUiRenderInterface_GL draws
    -- them; a coroutine yield in the lua test harness advances real frames.
    coroutine.yield(0.05)

    local got_event = false
    RmlUi.bind_event(doc, "#score_label", "click", function(event_type, x, y)
        got_event = true
    end)

    RmlUi.set_element_property(doc, "#score_label", "color", "#ff0000")

    RmlUi.hide_document(doc)
    RmlUi.close_document(doc)
end)

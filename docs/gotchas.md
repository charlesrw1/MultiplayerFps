- Project has a unit tests project, already use it. It links all projects in solution.

- must call set_call_init_in_editor(true) if you want components to call start() in editor mode. must call set_ticking(true) to tick. editor_start() is called on all components. set_editor_model() should only be used as a shorthand way for an editor visualization.

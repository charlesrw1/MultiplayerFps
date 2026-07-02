- Project has a unit tests project, already use it. It links all projects in solution.

- must call set_call_init_in_editor(true) if you want components to call start() in editor mode. must call set_ticking(true) to tick. editor_start() is called on all components. set_editor_model() should only be used as a shorthand way for an editor visualization.

- renderer for opengl uses reverse-Z and infinite Z projection matrix

- gpu driven rendering (occlusion, lod, frustum) for opaque objects. Occlusion culling splits the fast-path draw into two separate GPU-culled batches

- RenderWindow's ortho projection is built from get_vp_rect().get_size() only, with no offset — meaning window.draw()
  rect coordinates are viewport-local (0,0 = top-left of the scene viewport), not window-local

- animation import's trailing-duplicate-frame strip (ModelCompile_Animation.cpp) must be a single near-bit-exact
  check, not a loop with a loose tolerance — a loose/looped check chews into real frames on eased/decelerating
  loops (e.g. idle sway), truncating the clip and causing a snap at the loop point
  
 - shader cache. if you change a header that is included in a shder, engine doesnt pick up on that and loads stale shader from cache. it does pick up on the source file changes, but not includes.
#pragma once
// Flax-Engine-style profiler window (Overall/CPU/GPU tabs) on top of
// Source/Framework/Profiler.h. Gated by the stat.profiler ConfigVar --
// toggle it from the console, or via Tools > Profiler in the editor menu
// (see LevelEditor/EditorDocViewport.cpp).
namespace prof_ui {
// Call once per frame (see EngineMain_Loop.cpp); no-ops unless stat.profiler is set.
void draw();
}

class ConfigVar;
extern ConfigVar stat_profiler;

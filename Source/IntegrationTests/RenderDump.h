// Source/IntegrationTests/RenderDump.h
#pragma once

// Dumps all named render target textures as PNGs to TestFiles/debug/<test_name>/.
// Call this when a screenshot comparison fails to diagnose which stage is broken.
void dump_render_targets(const char* test_name);

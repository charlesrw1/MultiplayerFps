// Source/IntegrationTests/StateDump.h
#pragma once

// Prints engine state (cvars + level entities) to stderr.
// Safe to call at any time including from the assert hook.
void print_engine_state(const char* label);

// Prints a resolved stack trace to stderr using dbghelp.
// Call from the assert hook before abort.
void print_stack_trace();

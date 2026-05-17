#pragma once
// SDL3 provides backward-compat SDL2 names via SDL_oldnames.h, gated on
// the SDL_ENABLE_OLD_NAMES preprocessor define (set project-wide in the
// vcxproj). SDL_oldnames.h is only auto-included via the umbrella SDL.h —
// pull it in here so callers that include this header (or InputSystem.h)
// get the SDL_CONTROLLER_* / SDL_GameController* aliases too.
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_oldnames.h>

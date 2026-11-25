/*
 *   SdlNk.h
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Nuklear configuration of the SDL frontend

#include <SDL3/SDL.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear.h"
#include "nuksdl3.h"

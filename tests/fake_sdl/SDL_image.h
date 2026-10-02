#pragma once
#include "SDL.h"
#define IMG_INIT_PNG 2
int IMG_Init(int); void IMG_Quit(); SDL_Texture* IMG_LoadTexture(SDL_Renderer*, const char*); const char* IMG_GetError();

#pragma once
#include "SDL.h"
#define MIX_DEFAULT_FORMAT 0x8010
#define MIX_MAX_VOLUME 128
struct Mix_Music; struct Mix_Chunk;
int Mix_OpenAudio(int, Uint32, int, int); void Mix_CloseAudio(); int Mix_AllocateChannels(int);
Mix_Music* Mix_LoadMUS(const char*); void Mix_FreeMusic(Mix_Music*); const char* Mix_GetError();
int Mix_PlayMusic(Mix_Music*, int); int Mix_VolumeMusic(int); int Mix_HaltMusic(); int Mix_PlayingMusic(); void Mix_PauseMusic(); void Mix_ResumeMusic();
Mix_Chunk* Mix_LoadWAV(const char*); void Mix_FreeChunk(Mix_Chunk*); int Mix_VolumeChunk(Mix_Chunk*, int); int Mix_PlayChannel(int, Mix_Chunk*, int);

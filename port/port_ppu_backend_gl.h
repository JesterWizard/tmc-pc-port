#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

enum class PresentMode {
    NearestRaw = 0,
    XbrzLinear,
    XbrzNearest,
    LinearRaw,
    Count
};

bool Port_PPU_OpenGL_Init(SDL_Window* window, bool vsyncEnabled);
void Port_PPU_OpenGL_Present(SDL_Window* window, PresentMode mode,
                             const uint32_t* lowResPixels,
                             const uint32_t* hiResPixels,
                             int hiResW, int hiResH);
void Port_PPU_OpenGL_SetVSync(bool enabled);
void Port_PPU_OpenGL_Shutdown(void);

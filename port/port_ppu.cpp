#include "port_ppu.h"
#include "port_gba_mem.h"
#include "port_hdma.h"
#include "port_ppu_backend_gl.h"
#include "port_runtime_config.h"
#include "port_upscale.h"

#include <cpu/mode1.h>
#include <virtuappu.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum class RenderBackend {
    None,
    OpenGL,
    Renderer,
    Surface,
};

static const int kHiResW = 960;
static const int kHiResH = 640;

static RenderBackend sBackend = RenderBackend::None;
static SDL_Renderer* sRenderer = nullptr;
static SDL_Texture* sLowResTexture = nullptr;
static SDL_Texture* sHiResTexture = nullptr;
static SDL_Window* sWindow = nullptr;
static SDL_Surface* sFrameSurface = nullptr;
static PresentMode sPresentMode = PresentMode::NearestRaw;
static uint32_t* sUpscale2xBuf = nullptr;
static uint32_t* sUpscale4xBuf = nullptr;
static bool sVSyncEnabled = true;

static void Port_PPU_LoadConfig(void) {
    const char* method = Port_Config_UpscaleMethod();
    if (std::strcmp(method, "nearest") == 0) {
        sPresentMode = PresentMode::NearestRaw;
    } else if (std::strcmp(method, "linear") == 0) {
        sPresentMode = PresentMode::LinearRaw;
    } else if (std::strcmp(method, "xbrz_nearest") == 0) {
        sPresentMode = PresentMode::XbrzNearest;
    } else {
        sPresentMode = PresentMode::XbrzLinear;
    }
}

static const char* Port_PPU_MethodForMode(PresentMode mode) {
    switch (mode) {
        case PresentMode::NearestRaw:
            return "nearest";
        case PresentMode::LinearRaw:
            return "linear";
        case PresentMode::XbrzNearest:
            return "xbrz_nearest";
        case PresentMode::XbrzLinear:
        default:
            return "xbrz_linear";
    }
}

extern "C" const char* Port_PPU_PresentationModeName(void) {
    static const char* const kNames[] = {
        "nearest",
        "xBRZ smooth",
        "xBRZ sharp",
        "linear",
    };
    return kNames[(int)sPresentMode];
}

static void Port_PPU_ComputeFitRect(int w, int h, int* outX, int* outY, int* outW, int* outH) {
    int rw;
    int rh;
    if (w * 160 >= h * 240) {
        rh = h;
        rw = (h * 240) / 160;
    } else {
        rw = w;
        rh = (w * 160) / 240;
    }
    *outX = (w - rw) / 2;
    *outY = (h - rh) / 2;
    *outW = rw;
    *outH = rh;
}

static bool Port_PPU_EnsureUpscaleBuffers(void) {
    if (sUpscale2xBuf == nullptr) {
        sUpscale2xBuf = (uint32_t*)std::malloc((size_t)480 * 320 * sizeof(uint32_t));
    }
    if (sUpscale4xBuf == nullptr) {
        sUpscale4xBuf = (uint32_t*)std::malloc((size_t)kHiResW * kHiResH * sizeof(uint32_t));
    }
    return sUpscale2xBuf != nullptr && sUpscale4xBuf != nullptr;
}

static void Port_PPU_PresentSurfaceFrame(void) {
    SDL_Surface* windowSurface = SDL_GetWindowSurface(sWindow);
    int x;
    int y;
    int w;
    int h;
    SDL_Rect dstRect;

    if (!windowSurface) {
        return;
    }

    Port_PPU_ComputeFitRect(windowSurface->w, windowSurface->h, &x, &y, &w, &h);
    dstRect = {x, y, w, h};
    SDL_FillSurfaceRect(windowSurface, nullptr, 0);
    SDL_BlitSurfaceScaled(sFrameSurface, nullptr, windowSurface, &dstRect, SDL_SCALEMODE_NEAREST);
    SDL_UpdateWindowSurface(sWindow);
}

static bool Port_PPU_InitRendererBackend(void) {
    sRenderer = SDL_CreateRenderer(sWindow, nullptr);
    if (sRenderer == nullptr) {
        std::printf("Port_PPU_Init: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderVSync(sRenderer, sVSyncEnabled ? 1 : 0)) {
        std::printf("Port_PPU_Init: SDL_SetRenderVSync failed: %s\n", SDL_GetError());
    }
    sLowResTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ABGR8888,
                                       SDL_TEXTUREACCESS_STREAMING, 240, 160);
    sHiResTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ABGR8888,
                                      SDL_TEXTUREACCESS_STREAMING, kHiResW, kHiResH);
    if (sLowResTexture == nullptr || sHiResTexture == nullptr) {
        std::printf("Port_PPU_Init: SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    sBackend = RenderBackend::Renderer;
    return Port_PPU_EnsureUpscaleBuffers();
}

static bool Port_PPU_InitSurfaceBackend(void) {
    sFrameSurface = SDL_CreateSurfaceFrom(
        240,
        160,
        SDL_PIXELFORMAT_ABGR8888,
        virtuappu_frame_buffer,
        240 * static_cast<int>(sizeof(uint32_t)));
    if (!sFrameSurface) {
        std::printf("Port_PPU_Init: SDL_CreateSurfaceFrom failed: %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_SetWindowSurfaceVSync(sWindow, sVSyncEnabled ? 1 : 0)) {
        std::printf("Port_PPU_Init: SDL_SetWindowSurfaceVSync failed: %s\n", SDL_GetError());
    }

    sBackend = RenderBackend::Surface;
    SDL_ShowWindow(sWindow);
    SDL_RaiseWindow(sWindow);
    SDL_SyncWindow(sWindow);
    Port_PPU_PresentSurfaceFrame();
    return true;
}

static bool Port_PPU_InitPreferredBackend(void) {
    const char* backendPref = Port_Config_VideoBackend();
    const bool forceSoftware = std::strcmp(backendPref, "software") == 0;
    const bool forceOpenGL = std::strcmp(backendPref, "opengl") == 0;
    const bool windowSupportsOpenGL = sWindow != nullptr &&
        (SDL_GetWindowFlags(sWindow) & SDL_WINDOW_OPENGL) != 0;

    if (!forceSoftware) {
        if (!windowSupportsOpenGL) {
            std::printf("PPU OpenGL backend skipped: window is not OpenGL-capable.\n");
        } else if (Port_PPU_EnsureUpscaleBuffers() &&
                   Port_PPU_OpenGL_Init(sWindow, sVSyncEnabled)) {
            sBackend = RenderBackend::OpenGL;
            std::printf("PPU initialized with OpenGL backend.\n");
            return true;
        }
    }
    if (forceOpenGL) {
        std::printf("PPU OpenGL backend forced but unavailable, trying software fallback.\n");
    } else {
        std::printf("PPU OpenGL backend unavailable, trying software fallback.\n");
    }

    if (Port_PPU_InitRendererBackend()) {
        std::printf("PPU initialized with SDL renderer backend.\n");
        return true;
    }
    if (Port_PPU_InitSurfaceBackend()) {
        std::printf("PPU initialized with SDL window surface fallback.\n");
        return true;
    }
    return false;
}

static void Port_PPU_PrepareFrameBuffers(void) {
    if (sPresentMode == PresentMode::XbrzLinear || sPresentMode == PresentMode::XbrzNearest) {
        if (Port_PPU_EnsureUpscaleBuffers()) {
            Port_Upscale_xBRZ_4x(virtuappu_frame_buffer, 240, 160, sUpscale2xBuf, sUpscale4xBuf);
        }
    }
}

extern "C" void Port_PPU_SetVSync(bool enabled) {
    if (sVSyncEnabled == enabled) {
        return;
    }
    sVSyncEnabled = enabled;

    if (sBackend == RenderBackend::OpenGL) {
        Port_PPU_OpenGL_SetVSync(enabled);
    } else if (sRenderer != nullptr) {
        SDL_SetRenderVSync(sRenderer, enabled ? 1 : 0);
    }
}

extern "C" void Port_PPU_Init(SDL_Window* window) {
    sWindow = window;
    Port_PPU_LoadConfig();

    {
        VirtuaPPUMode1GbaMemory memory = {
            gIoMem,
            gVram,
            gBgPltt,
            gObjPltt,
            gOamMem,
        };
        virtuappu_mode1_bind_gba_memory(&memory);
    }

    virtuappu_mode1_pre_line_callback = port_hdma_step_line;
    virtuappu_registers.frame_width = 240;
    virtuappu_registers.mode = 1;

    if (!Port_PPU_InitPreferredBackend()) {
        std::printf("Port_PPU_Init: no presentation backend available.\n");
    }
}

extern "C" void Port_PPU_PresentFrame(void) {
    uint16_t dispcnt;
    uint8_t gbaMode;

    if (sBackend == RenderBackend::None) {
        return;
    }

    dispcnt = (uint16_t)(gIoMem[0x00] | (gIoMem[0x01] << 8));
    gbaMode = (uint8_t)(dispcnt & 0x07);
    switch (gbaMode) {
        case 0:
            virtuappu_registers.mode = 1;
            break;
        case 1:
        case 2:
            virtuappu_registers.mode = 2;
            break;
        default:
            virtuappu_registers.mode = 1;
            break;
    }

    virtuappu_render_frame();
    Port_PPU_PrepareFrameBuffers();

    if (sBackend == RenderBackend::OpenGL) {
        Port_PPU_OpenGL_Present(sWindow, sPresentMode, virtuappu_frame_buffer,
                                sUpscale4xBuf, kHiResW, kHiResH);
        return;
    }

    if (sBackend == RenderBackend::Renderer) {
        int outW = 0;
        int outH = 0;
        SDL_GetCurrentRenderOutputSize(sRenderer, &outW, &outH);
        int x;
        int y;
        int w;
        int h;
        Port_PPU_ComputeFitRect(outW, outH, &x, &y, &w, &h);
        SDL_FRect dst = {(float)x, (float)y, (float)w, (float)h};

        SDL_Texture* tex = sLowResTexture;
        SDL_ScaleMode scale = SDL_SCALEMODE_NEAREST;
        switch (sPresentMode) {
            case PresentMode::XbrzLinear:
            case PresentMode::XbrzNearest:
                SDL_UpdateTexture(sHiResTexture, nullptr, sUpscale4xBuf,
                                  kHiResW * (int)sizeof(uint32_t));
                tex = sHiResTexture;
                scale = (sPresentMode == PresentMode::XbrzLinear) ?
                    SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
                break;
            case PresentMode::LinearRaw:
                SDL_UpdateTexture(sLowResTexture, nullptr, virtuappu_frame_buffer,
                                  240 * (int)sizeof(uint32_t));
                scale = SDL_SCALEMODE_LINEAR;
                break;
            case PresentMode::NearestRaw:
            default:
                SDL_UpdateTexture(sLowResTexture, nullptr, virtuappu_frame_buffer,
                                  240 * (int)sizeof(uint32_t));
                break;
        }
        SDL_SetTextureScaleMode(tex, scale);
        SDL_SetRenderDrawColor(sRenderer, 0, 0, 0, 255);
        SDL_RenderClear(sRenderer);
        SDL_RenderTexture(sRenderer, tex, nullptr, &dst);
        SDL_RenderPresent(sRenderer);
        return;
    }

    Port_PPU_PresentSurfaceFrame();
}

extern "C" void Port_PPU_SetWindowTitle(const char* title) {
    if (!sWindow || !title) {
        return;
    }
    SDL_SetWindowTitle(sWindow, title);
}

extern "C" void Port_PPU_ToggleFullscreen(void) {
    if (!sWindow) {
        return;
    }
    SDL_WindowFlags flags = SDL_GetWindowFlags(sWindow);
    bool wantFullscreen = (flags & SDL_WINDOW_FULLSCREEN) == 0;
    SDL_SetWindowFullscreen(sWindow, wantFullscreen);
    SDL_SyncWindow(sWindow);
}

extern "C" bool Port_PPU_IsFullscreen(void) {
    if (!sWindow) {
        return false;
    }
    return (SDL_GetWindowFlags(sWindow) & SDL_WINDOW_FULLSCREEN) != 0;
}

extern "C" unsigned char Port_PPU_WindowScale(void) {
    return Port_Config_WindowScale();
}

extern "C" void Port_PPU_CycleWindowScale(int direction) {
    u8 scale = Port_Config_WindowScale();
    if (direction < 0) {
        scale = scale <= 1 ? 10 : (u8)(scale - 1);
    } else {
        scale = scale >= 10 ? 1 : (u8)(scale + 1);
    }
    Port_Config_SetWindowScale(scale);
    if (sWindow && !Port_PPU_IsFullscreen()) {
        SDL_SetWindowSize(sWindow, 240 * scale, 160 * scale);
        SDL_SyncWindow(sWindow);
    }
}

extern "C" void Port_PPU_CyclePresentationMode(int direction) {
    int next = (int)sPresentMode + (direction < 0 ? -1 : 1);
    if (next < 0) {
        next = (int)PresentMode::Count - 1;
    } else if (next >= (int)PresentMode::Count) {
        next = 0;
    }
    sPresentMode = (PresentMode)next;
    Port_Config_SetUpscaleMethod(Port_PPU_MethodForMode(sPresentMode));
    std::fprintf(stderr, "PPU upscale: %s\n", Port_PPU_PresentationModeName());
}

extern "C" void Port_PPU_ToggleSmoothing(void) {
    Port_PPU_CyclePresentationMode(1);
}

extern "C" void Port_PPU_Shutdown(void) {
    if (sBackend == RenderBackend::OpenGL) {
        Port_PPU_OpenGL_Shutdown();
    }
    if (sWindow && sBackend == RenderBackend::Surface) {
        SDL_DestroyWindowSurface(sWindow);
    }
    if (sFrameSurface) {
        SDL_DestroySurface(sFrameSurface);
        sFrameSurface = nullptr;
    }
    if (sLowResTexture) {
        SDL_DestroyTexture(sLowResTexture);
        sLowResTexture = nullptr;
    }
    if (sHiResTexture) {
        SDL_DestroyTexture(sHiResTexture);
        sHiResTexture = nullptr;
    }
    if (sUpscale2xBuf) {
        std::free(sUpscale2xBuf);
        sUpscale2xBuf = nullptr;
    }
    if (sUpscale4xBuf) {
        std::free(sUpscale4xBuf);
        sUpscale4xBuf = nullptr;
    }
    if (sRenderer) {
        SDL_DestroyRenderer(sRenderer);
        sRenderer = nullptr;
    }
    sBackend = RenderBackend::None;
    sWindow = nullptr;
}

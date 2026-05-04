#include "port_ppu_backend_gl.h"

#include <SDL3/SDL_opengl.h>

#include <cstdio>
#include <cstring>

namespace {

struct GLState {
    SDL_GLContext context = nullptr;
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint lowResTexture = 0;
    GLuint hiResTexture = 0;
    GLint samplerLocation = -1;
};

static GLState sGl;

using PFNGLGENVERTEXARRAYSPROC = void (APIENTRYP)(GLsizei, GLuint*);
using PFNGLBINDVERTEXARRAYPROC = void (APIENTRYP)(GLuint);
using PFNGLDELETEVERTEXARRAYSPROC = void (APIENTRYP)(GLsizei, const GLuint*);
using PFNGLGENBUFFERSPROC = void (APIENTRYP)(GLsizei, GLuint*);
using PFNGLBINDBUFFERPROC = void (APIENTRYP)(GLenum, GLuint);
using PFNGLBUFFERDATAPROC = void (APIENTRYP)(GLenum, GLsizeiptr, const void*, GLenum);
using PFNGLDELETEBUFFERSPROC = void (APIENTRYP)(GLsizei, const GLuint*);
using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRYP)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRYP)(GLuint);
using PFNGLCREATESHADERPROC = GLuint (APIENTRYP)(GLenum);
using PFNGLSHADERSOURCEPROC = void (APIENTRYP)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using PFNGLCOMPILESHADERPROC = void (APIENTRYP)(GLuint);
using PFNGLGETSHADERIVPROC = void (APIENTRYP)(GLuint, GLenum, GLint*);
using PFNGLGETSHADERINFOLOGPROC = void (APIENTRYP)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFNGLDELETESHADERPROC = void (APIENTRYP)(GLuint);
using PFNGLCREATEPROGRAMPROC = GLuint (APIENTRYP)(void);
using PFNGLATTACHSHADERPROC = void (APIENTRYP)(GLuint, GLuint);
using PFNGLLINKPROGRAMPROC = void (APIENTRYP)(GLuint);
using PFNGLGETPROGRAMIVPROC = void (APIENTRYP)(GLuint, GLenum, GLint*);
using PFNGLGETPROGRAMINFOLOGPROC = void (APIENTRYP)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFNGLDELETEPROGRAMPROC = void (APIENTRYP)(GLuint);
using PFNGLUSEPROGRAMPROC = void (APIENTRYP)(GLuint);
using PFNGLGETUNIFORMLOCATIONPROC = GLint (APIENTRYP)(GLuint, const GLchar*);
using PFNGLUNIFORM1IPROC = void (APIENTRYP)(GLint, GLint);
using PFNGLACTIVETEXTUREPROC = void (APIENTRYP)(GLenum);
using PFNGLGENTEXTURESPROC = void (APIENTRYP)(GLsizei, GLuint*);
using PFNGLDELETETEXTURESPROC = void (APIENTRYP)(GLsizei, const GLuint*);
using PFNGLBINDTEXTUREPROC = void (APIENTRYP)(GLenum, GLuint);
using PFNGLTEXPARAMETERIPROC = void (APIENTRYP)(GLenum, GLenum, GLint);
using PFNGLPIXELSTOREIPROC = void (APIENTRYP)(GLenum, GLint);
using PFNGLTEXIMAGE2DPROC = void (APIENTRYP)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using PFNGLGETERRORPROC = GLenum (APIENTRYP)(void);
using PFNGLTEXSUBIMAGE2DPROC = void (APIENTRYP)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
using PFNGLVIEWPORTPROC = void (APIENTRYP)(GLint, GLint, GLsizei, GLsizei);
using PFNGLCLEARCOLORPROC = void (APIENTRYP)(GLfloat, GLfloat, GLfloat, GLfloat);
using PFNGLCLEARPROC = void (APIENTRYP)(GLbitfield);
using PFNGLDRAWARRAYSPROC = void (APIENTRYP)(GLenum, GLint, GLsizei);

static PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
static PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
static PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
static PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
static PFNGLBUFFERDATAPROC pglBufferData = nullptr;
static PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
static PFNGLCREATESHADERPROC pglCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC pglShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC pglCompileShader = nullptr;
static PFNGLGETSHADERIVPROC pglGetShaderiv = nullptr;
static PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = nullptr;
static PFNGLDELETESHADERPROC pglDeleteShader = nullptr;
static PFNGLCREATEPROGRAMPROC pglCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC pglAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC pglLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC pglGetProgramiv = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = nullptr;
static PFNGLDELETEPROGRAMPROC pglDeleteProgram = nullptr;
static PFNGLUSEPROGRAMPROC pglUseProgram = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = nullptr;
static PFNGLUNIFORM1IPROC pglUniform1i = nullptr;
static PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;
static PFNGLGENTEXTURESPROC pglGenTextures = nullptr;
static PFNGLDELETETEXTURESPROC pglDeleteTextures = nullptr;
static PFNGLBINDTEXTUREPROC pglBindTexture = nullptr;
static PFNGLTEXPARAMETERIPROC pglTexParameteri = nullptr;
static PFNGLPIXELSTOREIPROC pglPixelStorei = nullptr;
static PFNGLTEXIMAGE2DPROC pglTexImage2D = nullptr;
static PFNGLGETERRORPROC pglGetError = nullptr;
static PFNGLTEXSUBIMAGE2DPROC pglTexSubImage2D = nullptr;
static PFNGLVIEWPORTPROC pglViewport = nullptr;
static PFNGLCLEARCOLORPROC pglClearColor = nullptr;
static PFNGLCLEARPROC pglClear = nullptr;
static PFNGLDRAWARRAYSPROC pglDrawArrays = nullptr;

template <typename T>
bool LoadProc(T* out, const char* name) {
    *out = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    if (*out == nullptr) {
        std::printf("OpenGL: failed to load %s\n", name);
        return false;
    }
    return true;
}

bool LoadRequiredProcs(void) {
    return LoadProc(&pglGenVertexArrays, "glGenVertexArrays") &&
           LoadProc(&pglBindVertexArray, "glBindVertexArray") &&
           LoadProc(&pglDeleteVertexArrays, "glDeleteVertexArrays") &&
           LoadProc(&pglGenBuffers, "glGenBuffers") &&
           LoadProc(&pglBindBuffer, "glBindBuffer") &&
           LoadProc(&pglBufferData, "glBufferData") &&
           LoadProc(&pglDeleteBuffers, "glDeleteBuffers") &&
           LoadProc(&pglVertexAttribPointer, "glVertexAttribPointer") &&
           LoadProc(&pglEnableVertexAttribArray, "glEnableVertexAttribArray") &&
           LoadProc(&pglCreateShader, "glCreateShader") &&
           LoadProc(&pglShaderSource, "glShaderSource") &&
           LoadProc(&pglCompileShader, "glCompileShader") &&
           LoadProc(&pglGetShaderiv, "glGetShaderiv") &&
           LoadProc(&pglGetShaderInfoLog, "glGetShaderInfoLog") &&
           LoadProc(&pglDeleteShader, "glDeleteShader") &&
           LoadProc(&pglCreateProgram, "glCreateProgram") &&
           LoadProc(&pglAttachShader, "glAttachShader") &&
           LoadProc(&pglLinkProgram, "glLinkProgram") &&
           LoadProc(&pglGetProgramiv, "glGetProgramiv") &&
           LoadProc(&pglGetProgramInfoLog, "glGetProgramInfoLog") &&
           LoadProc(&pglDeleteProgram, "glDeleteProgram") &&
           LoadProc(&pglUseProgram, "glUseProgram") &&
           LoadProc(&pglGetUniformLocation, "glGetUniformLocation") &&
           LoadProc(&pglUniform1i, "glUniform1i") &&
           LoadProc(&pglActiveTexture, "glActiveTexture") &&
           LoadProc(&pglGenTextures, "glGenTextures") &&
           LoadProc(&pglDeleteTextures, "glDeleteTextures") &&
           LoadProc(&pglBindTexture, "glBindTexture") &&
           LoadProc(&pglTexParameteri, "glTexParameteri") &&
           LoadProc(&pglPixelStorei, "glPixelStorei") &&
           LoadProc(&pglTexImage2D, "glTexImage2D") &&
           LoadProc(&pglGetError, "glGetError") &&
           LoadProc(&pglTexSubImage2D, "glTexSubImage2D") &&
           LoadProc(&pglViewport, "glViewport") &&
           LoadProc(&pglClearColor, "glClearColor") &&
           LoadProc(&pglClear, "glClear") &&
           LoadProc(&pglDrawArrays, "glDrawArrays");
}

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = pglCreateShader(type);
    if (shader == 0) {
        return 0;
    }

    pglShaderSource(shader, 1, &source, nullptr);
    pglCompileShader(shader);

    GLint ok = GL_FALSE;
    pglGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) {
        return shader;
    }

    char log[1024];
    GLsizei logLen = 0;
    pglGetShaderInfoLog(shader, sizeof(log), &logLen, log);
    std::printf("OpenGL shader compile failed: %.*s\n", (int)logLen, log);
    pglDeleteShader(shader);
    return 0;
}

GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = pglCreateProgram();
    if (program == 0) {
        return 0;
    }

    pglAttachShader(program, vertexShader);
    pglAttachShader(program, fragmentShader);
    pglLinkProgram(program);

    GLint ok = GL_FALSE;
    pglGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE) {
        return program;
    }

    char log[1024];
    GLsizei logLen = 0;
    pglGetProgramInfoLog(program, sizeof(log), &logLen, log);
    std::printf("OpenGL program link failed: %.*s\n", (int)logLen, log);
    pglDeleteProgram(program);
    return 0;
}

bool CreateProgram(void) {
    static const char* kVertexShaderSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec2 inPos;\n"
        "layout(location = 1) in vec2 inUv;\n"
        "out vec2 fragUv;\n"
        "void main() {\n"
        "    fragUv = inUv;\n"
        "    gl_Position = vec4(inPos, 0.0, 1.0);\n"
        "}\n";

    static const char* kFragmentShaderSrc =
        "#version 330 core\n"
        "in vec2 fragUv;\n"
        "uniform sampler2D frameTexture;\n"
        "out vec4 outColor;\n"
        "void main() {\n"
        "    outColor = texture(frameTexture, fragUv);\n"
        "}\n";

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            pglDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            pglDeleteShader(fragmentShader);
        }
        return false;
    }

    sGl.program = LinkProgram(vertexShader, fragmentShader);
    pglDeleteShader(vertexShader);
    pglDeleteShader(fragmentShader);
    if (sGl.program == 0) {
        return false;
    }

    sGl.samplerLocation = pglGetUniformLocation(sGl.program, "frameTexture");
    return sGl.samplerLocation >= 0;
}

bool CreateGeometry(void) {
    static const GLfloat kQuadVertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };

    pglGenVertexArrays(1, &sGl.vao);
    pglGenBuffers(1, &sGl.vbo);
    if (sGl.vao == 0 || sGl.vbo == 0) {
        return false;
    }

    pglBindVertexArray(sGl.vao);
    pglBindBuffer(GL_ARRAY_BUFFER, sGl.vbo);
    pglBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    pglVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(GLfloat), (void*)0);
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(GLfloat),
                           (void*)(2 * sizeof(GLfloat)));
    pglEnableVertexAttribArray(1);
    pglBindBuffer(GL_ARRAY_BUFFER, 0);
    pglBindVertexArray(0);
    return true;
}

bool CreateTexture(GLuint* outTexture, int width, int height) {
    pglGenTextures(1, outTexture);
    if (*outTexture == 0) {
        return false;
    }
    pglBindTexture(GL_TEXTURE_2D, *outTexture);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    return pglGetError() == GL_NO_ERROR;
}

void SetTextureFiltering(GLuint texture, bool linear) {
    pglBindTexture(GL_TEXTURE_2D, texture);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
}

void ComputeFitRect(int w, int h, int* outX, int* outY, int* outW, int* outH) {
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

void ResetState(void) {
    sGl = GLState{};
}

}  // namespace

bool Port_PPU_OpenGL_Init(SDL_Window* window, bool vsyncEnabled) {
    ResetState();

    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    sGl.context = SDL_GL_CreateContext(window);
    if (sGl.context == nullptr) {
        std::printf("OpenGL: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_GL_MakeCurrent(window, sGl.context)) {
        std::printf("OpenGL: SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        Port_PPU_OpenGL_Shutdown();
        return false;
    }
    if (!LoadRequiredProcs()) {
        Port_PPU_OpenGL_Shutdown();
        return false;
    }
    if (!CreateProgram() || !CreateGeometry()) {
        Port_PPU_OpenGL_Shutdown();
        return false;
    }
    if (!CreateTexture(&sGl.lowResTexture, 240, 160) ||
        !CreateTexture(&sGl.hiResTexture, 960, 640)) {
        std::printf("OpenGL: texture allocation failed\n");
        Port_PPU_OpenGL_Shutdown();
        return false;
    }

    pglUseProgram(sGl.program);
    pglUniform1i(sGl.samplerLocation, 0);
    pglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Port_PPU_OpenGL_SetVSync(vsyncEnabled);
    std::printf("OpenGL: initialized 3.3 core backend.\n");
    return true;
}

void Port_PPU_OpenGL_Present(SDL_Window* window, PresentMode mode,
                             const uint32_t* lowResPixels,
                             const uint32_t* hiResPixels,
                             int hiResW, int hiResH) {
    if (sGl.context == nullptr || window == nullptr) {
        return;
    }

    if (!SDL_GL_MakeCurrent(window, sGl.context)) {
        return;
    }

    const bool useHiRes = mode == PresentMode::XbrzLinear || mode == PresentMode::XbrzNearest;
    const bool linear = mode == PresentMode::LinearRaw || mode == PresentMode::XbrzLinear;
    const GLuint texture = useHiRes ? sGl.hiResTexture : sGl.lowResTexture;

    pglBindTexture(GL_TEXTURE_2D, texture);
    pglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                     useHiRes ? hiResW : 240,
                     useHiRes ? hiResH : 160,
                     GL_RGBA, GL_UNSIGNED_BYTE,
                     useHiRes ? hiResPixels : lowResPixels);
    SetTextureFiltering(texture, linear);

    int outW = 0;
    int outH = 0;
    SDL_GetWindowSize(window, &outW, &outH);
    int x;
    int y;
    int w;
    int h;
    ComputeFitRect(outW, outH, &x, &y, &w, &h);

    pglViewport(0, 0, outW, outH);
    pglClear(GL_COLOR_BUFFER_BIT);
    pglViewport(x, y, w, h);

    pglUseProgram(sGl.program);
    pglActiveTexture(GL_TEXTURE0);
    pglBindTexture(GL_TEXTURE_2D, texture);
    pglBindVertexArray(sGl.vao);
    pglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    pglBindVertexArray(0);
    SDL_GL_SwapWindow(window);
}

void Port_PPU_OpenGL_SetVSync(bool enabled) {
    if (!SDL_GL_SetSwapInterval(enabled ? 1 : 0)) {
        std::printf("OpenGL: SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError());
    }
}

void Port_PPU_OpenGL_Shutdown(void) {
    if (sGl.lowResTexture != 0) {
        pglDeleteTextures(1, &sGl.lowResTexture);
    }
    if (sGl.hiResTexture != 0) {
        pglDeleteTextures(1, &sGl.hiResTexture);
    }
    if (sGl.vbo != 0 && pglDeleteBuffers != nullptr) {
        pglDeleteBuffers(1, &sGl.vbo);
    }
    if (sGl.vao != 0 && pglDeleteVertexArrays != nullptr) {
        pglDeleteVertexArrays(1, &sGl.vao);
    }
    if (sGl.program != 0 && pglDeleteProgram != nullptr) {
        pglDeleteProgram(sGl.program);
    }
    if (sGl.context != nullptr) {
        SDL_GL_DestroyContext(sGl.context);
    }
    ResetState();
}

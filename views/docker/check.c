// Smoke test of the image: an SDL 3 window with an OpenGL ES 3.0 context
// created through ANGLE. Exits with a non-zero status on failure.
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <string.h>

typedef const GLubyte* (*GetString)(GLenum);
typedef void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*Clear)(GLbitfield);
typedef void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                           void*);

int main(int argc, char** argv) {
  const char* angle = argc > 1 ? argv[1] : "/opt/angle/lib";
  char egl[512], gles[512];
  snprintf(egl, sizeof(egl), "%s/libEGL.so", angle);
  snprintf(gles, sizeof(gles), "%s/libGLESv2.so", angle);
  // Use EGL (not GLX) and load ANGLE instead of the system libraries.
  SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
  SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
  SDL_SetHint(SDL_HINT_EGL_LIBRARY, egl);
  SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, gles);
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_Window* window = SDL_CreateWindow(
      "check", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window || !SDL_GL_CreateContext(window)) {
    fprintf(stderr, "window: %s\n", SDL_GetError());
    return 1;
  }
  GetString getString = (GetString)SDL_GL_GetProcAddress("glGetString");
  ClearColor clearColor = (ClearColor)SDL_GL_GetProcAddress("glClearColor");
  Clear clear = (Clear)SDL_GL_GetProcAddress("glClear");
  ReadPixels readPixels = (ReadPixels)SDL_GL_GetProcAddress("glReadPixels");
  const char* version = (const char*)getString(GL_VERSION);
  printf("SDL %d.%d.%d, video driver %s\n", SDL_MAJOR_VERSION,
         SDL_MINOR_VERSION, SDL_MICRO_VERSION, SDL_GetCurrentVideoDriver());
  printf("%s\n%s\n", version, getString(GL_RENDERER));
  printf("pixel density %.2f, display scale %.2f\n",
         SDL_GetWindowPixelDensity(window), SDL_GetWindowDisplayScale(window));
  clearColor(0.0f, 0.0f, 1.0f, 1.0f);
  clear(GL_COLOR_BUFFER_BIT);
  unsigned char pixel[4];
  readPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  SDL_GL_SwapWindow(window);
  SDL_Quit();
  if (!strstr(version, "OpenGL ES 3") || !strstr(version, "ANGLE")) {
    fprintf(stderr, "not an ANGLE OpenGL ES 3 context\n");
    return 1;
  }
  if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 255) {
    fprintf(stderr, "unexpected pixel %d %d %d\n", pixel[0], pixel[1],
            pixel[2]);
    return 1;
  }
  printf("ok\n");
  return 0;
}

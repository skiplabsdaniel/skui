# Native platform layer: SDL and ANGLE

Status: design proposal, nothing implemented yet. This document describes how
to replace the X11/GLX native layer with SDL (windows and events) and ANGLE
(OpenGL ES 3 on the platform graphics API), to run skui natively on Linux,
macOS and Windows with a single code base.

## 1. Why

- **macOS**: X11 is not part of macOS (XQuartz is a minimal community port
  without modern OpenGL), and Apple deprecated OpenGL in 2018: it is frozen
  at 4.1 core and does not accept the `#version 300 es` shaders of skui.
- **Windows**: there is no native layer at all.
- **Linux**: the X11 layer works, but not on Wayland-only sessions, and it
  reimplements what portable libraries already do (keyboard mapping, input
  methods, clipboard, cursors, pixel ratio).
- **Shaders**: the web build uses WebGL 2, which is OpenGL ES 3.0. Using
  OpenGL ES 3.0 natively too keeps one set of shaders everywhere.

## 2. Current state

```
framework (Skip) ──── SKIP_SKUIX11_* / SKIP_FWK_* ───► x11gl/extra (Skip)
       │                                                     │
       │ drawables                                           │ events
       ▼                                                     │
opengl (Skip) ─── SKIP_GL_* (77 functions) ──► opengl/native/GL.cpp (GLEW, desktop GL)
                                                             ▲
x11gl/native: GLWManager.cpp, Supplier.cpp ─────────────────┘
  X11 window, GLX context, event loop, XIM input, clipboard, cursors,
  Xft.dpi pixel ratio
```

- `opengl/native/src/GL.cpp` implements the GL calls of the Skip `GL`
  module with desktop OpenGL through GLEW. A few calls are not in OpenGL ES:
  `glEnable(GL_TEXTURE_2D)`, `glGetTexImage` (texture copies),
  `glDrawBuffer`, and `glCreateFramebuffers` (OpenGL 4.5).
- Shaders are a mix of `#version 300 es` (MSDF, bilinear images) and GLSL
  1.00 ES (`attribute`, `varying`, `texture2D`, `gl_FragColor`).
- `x11gl/native` (about 700 lines of C++) owns the window, the GLX context,
  the event loop, input methods, clipboard and cursors. It calls the
  framework through `EventManager` (`resize`, `performFrame`,
  `manageMouseLocation`, `manageMouseButton`, `manageKeyPressed`,
  `manageKeyReleased`, `paste`, `requestClose`).
- `x11gl/extra` (Skip) converts X11 key codes and keysyms to the web key
  codes and key names the framework uses (`X11Key.sk`, 3000 lines of
  tables), and forwards the events.
- `opengl/native/src/Offscreen.cpp` renders without a window, with EGL on
  the Mesa surfaceless platform, for the rendering tests.

## 3. Target

```
framework (Skip) ──── SKIP_SKUINative_* ───► native/extra (Skip)
       │                                         │ web key codes
       ▼                                         │
opengl (Skip) ─── SKIP_GL_* ──► opengl/native/GL.cpp (OpenGL ES 3.0)
                                     │
                         ┌───────────┴──────────────┐
                         │ ANGLE (libEGL, libGLESv2) │  or system GLES
                         └───────────┬──────────────┘
                    Metal (macOS) │ Direct3D 11 (Windows) │ Vulkan or GL (Linux)

native/src (C++): SDL window, events, text input, clipboard, cursors,
                  pixel ratio, frame loop
```

### 3.1 Rendering: OpenGL ES 3.0 through ANGLE

ANGLE implements OpenGL ES 3.0 (and EGL) on top of Metal, Direct3D 11,
Vulkan or desktop OpenGL. It is the layer Chrome and Firefox use for WebGL,
maintained by Google.

Changes in `opengl`:

1. `GL.cpp` targets OpenGL ES 3.0: `GLES3/gl3.h` instead of GLEW, no
   `GL_TEXTURE_2D` enable, `glGenFramebuffers` instead of
   `glCreateFramebuffers`, `glDrawBuffers` instead of `glDrawBuffer`.
   Texture copies (`glGetTexImage`, missing in ES) read the texture through
   a framebuffer with `glReadPixels`, or copy with `glBlitFramebuffer`.
2. All shaders move to `#version 300 es`. The GLSL 1.00 ES ones would also
   work in ES 3.0, but one version is simpler and matches WebGL 2.
3. `Offscreen.cpp` uses EGL with an OpenGL ES 3 context. With ANGLE, its
   null or SwiftShader back end renders without a GPU, which keeps the
   rendering tests working in containers and CI.

On Linux, ANGLE is optional: Mesa provides OpenGL ES 3 natively (including
llvmpipe in containers), so the system `libGLESv2` can be used at first.

### 3.2 Windows and events: SDL

SDL creates the window and the graphics surface, and delivers events in the
same way on every platform. SDL 3 is the current version; SDL 2 is in the
Ubuntu 24.04 packages (2.30). SDL can create an OpenGL ES context through
ANGLE's EGL (`SDL_HINT_VIDEO_EGL_DRIVER`, `SDL_HINT_OPENGL_ES_DRIVER`).

| skui need | Today (X11) | SDL |
|---|---|---|
| Window, resize | `XCreateWindow`, `ConfigureNotify` | `SDL_CreateWindow`, `SDL_EVENT_WINDOW_RESIZED` |
| GL context, swap | GLX | `SDL_GL_CreateContext` (ES 3 via ANGLE), `SDL_GL_SwapWindow` |
| Pixel ratio | `Xft.dpi / 96`, read once | `SDL_GetWindowPixelDensity`, `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` (follows the screen the window is on) |
| Mouse | `MotionNotify`, `ButtonPress` | `SDL_EVENT_MOUSE_MOTION`, `SDL_EVENT_MOUSE_BUTTON_*`, `SDL_EVENT_MOUSE_WHEEL` |
| Keys | X11 key codes and keysyms, 3000 lines of tables | `SDL_Scancode` (physical, maps to web `code`) and `SDL_Keycode` (layout, maps to web `key`): two small tables |
| Text input, accents, IME | XIM, `Xutf8LookupString`, dead keys table | `SDL_EVENT_TEXT_INPUT`, `SDL_EVENT_TEXT_EDITING` (composition) |
| Clipboard | X selections, `SelectionRequest` | `SDL_GetClipboardText`, `SDL_SetClipboardText` |
| Cursors | Xcursor | `SDL_CreateSystemCursor` |
| Frame loop | timer and X client messages | `SDL_PollEvent` loop with vsync, or `SDL_AddTimer` |
| Messages from other threads | X client messages | `SDL_PushEvent` with a user event type |
| Close | `WM_DELETE_WINDOW` | `SDL_EVENT_WINDOW_CLOSE_REQUESTED` |

Text input is the main improvement: SDL uses the platform input methods
(IBus/Fcitx on Linux, the macOS text input system, TSF on Windows), which
gives accents, compose keys and Asian input methods everywhere.

### 3.3 Package layout

- `native/` (new package, replaces `x11gl/native` and `x11gl/extra`):
  - `native/src/*.cpp`: SDL window manager and event loop, implementing the
    same `EventManager` interface as today, without X11 types.
  - `native/extra/*.sk`: forwards events to the framework, with the
    `SDL_Scancode` to web `code` and `SDL_Keycode` to web `key` tables.
- `opengl/native/src/GL.cpp`: OpenGL ES 3.0.
- `views` and applications link the `native` package instead of `x11gl`.
- `x11gl` stays until the SDL layer is complete on Linux, then is removed.

The framework side does not change: it already receives logical pixel
sizes, mouse locations, web key codes and the pixel ratio at draw time.

## 4. Build and distribution

| Platform | SDL | OpenGL ES |
|---|---|---|
| Linux | system package (`libsdl2-dev`, or SDL 3) | system Mesa `libGLESv2`/`libEGL`, ANGLE optional |
| macOS | Homebrew (`sdl3`) or built from source | ANGLE built from source, or prebuilt (`libEGL.dylib`, `libGLESv2.dylib`) shipped with the application |
| Windows | prebuilt release | ANGLE prebuilt DLLs |

ANGLE is the main build question: it uses Google's build tools (GN, ninja,
depot_tools), so it should be built once per platform and its binaries
shipped with the application rather than built by skargo. Some projects
publish prebuilt ANGLE binaries; this needs checking. The skargo build
script (`build.sk`, `skui_build__links`) only needs the include
directories and the libraries to link.

## 5. Steps

Each step keeps the existing tests passing.

1. **OpenGL ES 3.0 in `GL.cpp`** on Linux with Mesa: replace GLEW and the
   desktop-only calls, move all shaders to `#version 300 es`, and switch
   `Offscreen.cpp` to an ES 3 context. The offscreen rendering tests check
   that the rendering is unchanged.
2. **SDL layer on Linux**: `native` package with the window, events,
   clipboard, cursors and pixel ratio, next to `x11gl`. Check with the views,
   through VNC in a container (see VNC.md).
3. **Switch `views` to SDL**, then remove `x11gl`.
4. **macOS**: SDL with ANGLE on Metal; package the libraries with the
   application.
5. **Windows**: SDL with ANGLE on Direct3D 11.

Steps 1 and 2 can be done and tested entirely in a Linux container.

## 6. Open questions

- SDL 2 (packaged everywhere today) or SDL 3 (current API, better pixel
  density support)?
- How to get ANGLE binaries for macOS and Windows: build them in CI, or use
  prebuilt ones?
- Should applications ship ANGLE on Linux too, for identical rendering on
  every platform, or use the system OpenGL ES?
- Multiple windows: the X11 layer supports several windows (`wId`); SDL does
  too, but the event loop must dispatch by window id.

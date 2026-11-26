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
3. `Offscreen.cpp` uses EGL with an OpenGL ES 3 context, created through
   ANGLE like the windows, so the rendering tests run the same code path as
   the applications.

#### ANGLE on Linux

ANGLE is used on Linux too, so that every platform runs the same
implementation (same GLSL compiler, same ES 3.0 behavior) and the rendering
tests check what macOS and Windows users see. This was checked in a Linux
aarch64 container without GPU (Mesa llvmpipe), with the prebuilt ANGLE of
[sge-angle-natives](https://github.com/kubuszok/sge-angle-natives) (release
`chromium-7151`):

| Test | ANGLE back end | Result |
|---|---|---|
| Surfaceless EGL display, pbuffer, ES 3 context | OpenGL | works: `OpenGL ES 3.0.0 (ANGLE 2.1.25345)` on `llvmpipe` |
| Same | OpenGL ES (Mesa ES 3.2) | works |
| X11 window under Xvfb, window surface, swap | OpenGL | works: the cleared color is read back from the window |
| Surfaceless | Vulkan | fails: no Vulkan driver in the container |

The display is created with `eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE,
...)` and `EGL_PLATFORM_ANGLE_TYPE_ANGLE`; for surfaceless rendering,
`EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE` is
`EGL_PLATFORM_SURFACELESS_MESA`.

- The archive contains only `libEGL.so`, `libGLESv2.so` and the headers
  (`EGL`, `GLES2`, `GLES3`, `KHR`, with `eglext_angle.h`). The libraries
  depend on nothing but libc: ANGLE loads the system `libGL.so.1`,
  `libEGL.so.1` or `libvulkan.so` at run time for its back end.
- Back end on Linux: OpenGL (over Mesa or the GPU driver) works everywhere
  today. Vulkan is ANGLE's preferred Linux back end and is worth trying on
  real GPUs; in containers it needs a CPU Vulkan driver (Mesa lavapipe,
  `mesa-vulkan-drivers`, or SwiftShader), not tested yet. The back end can be
  forced with `ANGLE_DEFAULT_PLATFORM` (`gl`, `vulkan`, `swiftshader`) or the
  `EGL_PLATFORM_ANGLE_TYPE_ANGLE` attribute.
- The same project publishes Linux x86_64 and aarch64, macOS x86_64 and
  aarch64, Windows x86_64 and aarch64 builds (BSD-3 license), all from the
  same ANGLE revision.

### 3.2 Windows and events: SDL

SDL creates the window and the graphics surface, and delivers events in the
same way on every platform. SDL creates the OpenGL ES context through
ANGLE's EGL with four hints, set before `SDL_Init` (checked with SDL 3.4.20
on X11, see `views/docker/check.c`):

- `SDL_HINT_VIDEO_FORCE_EGL`: on X11, SDL otherwise starts with GLX, loading
  the library of `SDL_HINT_OPENGL_LIBRARY` as a GLX library, which fails
  with ANGLE ("Could not retrieve OpenGL functions");
- `SDL_HINT_OPENGL_ES_DRIVER`: load an EGL/GLES library instead of asking
  the desktop GL driver for an ES profile;
- `SDL_HINT_EGL_LIBRARY` and `SDL_HINT_OPENGL_LIBRARY`: the paths of
  ANGLE's `libEGL` and `libGLESv2`.

Without them, SDL silently uses the system driver instead of ANGLE.

skui uses SDL 3 (see 3.3); the table below uses SDL 3 names.

| skui need | Today (X11) | SDL |
|---|---|---|
| Window, resize | `XCreateWindow`, `ConfigureNotify` | `SDL_CreateWindow`, `SDL_EVENT_WINDOW_RESIZED` |
| GL context, swap | GLX | `SDL_GL_CreateContext` (ES 3 via ANGLE), `SDL_GL_SwapWindow` |
| Pixel ratio | `Xft.dpi / 96`, read once | `SDL_GetWindowPixelDensity` × `SDL_GetWindowDisplayScale`, `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` and `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` (follow the screen the window is on) |
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

The pixel ratio skui needs is the number of device pixels per logical
pixel: the product of the two SDL 3 values. On macOS and Wayland, the window
size is in logical pixels and the pixel density gives the ratio (2 on a
Retina screen). On X11, windows are in device pixels: the pixel density
stays 1 and the display scale carries the ratio (checked under Xvfb:
`GDK_SCALE=2` or `SDL_VIDEO_X11_SCALING_FACTOR=2` give a display scale of 2
and a pixel density of 1). On X11, skui must then also convert the window
size and mouse positions to logical pixels, as it does today.

### 3.3 SDL 3, not SDL 2

Decision: skui uses SDL 3 only. The comparison below is the reason.

| | SDL 2 | SDL 3 |
|---|---|---|
| Status | maintenance only | current version (3.2, January 2025) |
| Ubuntu 24.04 | `libsdl2-dev` 2.30 | not packaged (Ubuntu 25.04 and later, Debian 13); build from source with CMake |
| macOS, Windows | Homebrew, prebuilt releases | Homebrew (`sdl3`), prebuilt releases |
| Pixel ratio | `SDL_WINDOW_ALLOW_HIGHDPI` and `SDL_GL_GetDrawableSize` / `SDL_GetWindowSize`: works on macOS, but on X11 the ratio is always 1 (skui must keep reading `Xft.dpi`) | `SDL_GetWindowPixelDensity` and `SDL_GetWindowDisplayScale`, including on X11 (`Xft.dpi`, `GDK_SCALE`) and Wayland, with change events |
| Wayland | supported, X11 preferred by default | preferred by default when available |
| Text input | global `SDL_StartTextInput()` | per window, `SDL_StartTextInput(window)`, with IME candidate area |

#### Cost of supporting both

The two APIs are close: SDL 3 mostly renames things. The scancode values are
the same in both versions, and the keycodes of printable keys are their
Unicode characters in both, so the key tables in Skip are shared. The
differences are confined to the C++ layer:

- event names and fields: `SDL_WINDOWEVENT` with a `window.event`
  sub-type (SDL 2) instead of one `SDL_EVENT_WINDOW_*` type per event;
  `event.key.keysym.scancode` instead of `event.key.scancode`; integer
  mouse positions instead of floats;
- return values: `0` / negative error codes instead of `bool`;
- the pixel ratio: computed by skui on SDL 2 (drawable size over window
  size, and `Xft.dpi` on X11, as today), given by SDL 3;
- text input start and stop, a few renamed constants (system cursors,
  hints).

With a `SKUI_SDL2` define and a small header of wrappers
(`sdlPollEvent`, `sdlPixelRatio`, `sdlStartText`, ...), the event loop
would be written once and the conditional code would be about 100 to 150
lines out of an estimated 600 to 800 for the layer. The main cost is not
the code but keeping both working: two builds to compile and test (two CI
jobs), and the SDL 2 pixel ratio fallback, which is exactly the X11-specific
code this design wants to remove.

SDL 3 is the version that solves the pixel ratio on every platform, and
SDL 2 receives no new features. On Linux distributions without the package,
SDL 3 is built from source once (CMake, a few minutes) or shipped with the
application like ANGLE. The SDL calls still go through the few wrappers
above, so that SDL 2 support can be added later behind the define if a
target needs it.

### 3.4 Package layout

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
| Linux | SDL 3 package, or built from source | prebuilt ANGLE (`libEGL.so`, `libGLESv2.so`) |
| macOS | Homebrew (`sdl3`) or built from source | prebuilt ANGLE (`libEGL.dylib`, `libGLESv2.dylib`) |
| Windows | prebuilt release | prebuilt ANGLE DLLs |

ANGLE uses Google's build tools (GN, ninja, depot_tools), so it is not built
by skargo: the prebuilt archives (see 3.1) are downloaded once per platform,
pinned to one release, and shipped with the application, with an rpath (or
`@rpath` on macOS) next to the binary. The skargo build script (`build.sk`,
`skui_build__links`) only needs the include directories and the libraries
to link. Building ANGLE in CI remains possible if the prebuilt project stops
publishing releases.

### Docker image

`views/docker/Dockerfile` builds an Ubuntu 24.04 image with SDL 3 (built from
source, in `/opt/sdl3`) and ANGLE (prebuilt, in `/opt/angle`), with Xvfb and
the VNC servers of VNC.md. Its build fails if SDL 3 cannot open a window
with an ANGLE OpenGL ES 3 context (`views/docker/check.c`, run under Xvfb).
It can be built on top of the Skip toolchain image with `--build-arg BASE`.

## 5. Steps

Each step keeps the existing tests passing.

1. **OpenGL ES 3.0 in `GL.cpp`** on Linux with ANGLE: replace GLEW and the
   desktop-only calls, move all shaders to `#version 300 es`, and switch
   `Offscreen.cpp` to an ANGLE ES 3 context. The offscreen rendering tests
   check that the rendering is unchanged.
2. **SDL layer on Linux**: `native` package with the window, events,
   clipboard, cursors and pixel ratio, next to `x11gl`. Check with the views,
   through VNC in a container (see VNC.md).
3. **Switch `views` to SDL**, then remove `x11gl`.
4. **macOS**: SDL with ANGLE on Metal; package the libraries with the
   application.
5. **Windows**: SDL with ANGLE on Direct3D 11.

Steps 1 and 2 can be done and tested entirely in a Linux container.

## 6. Open questions

- ANGLE back end on Linux: OpenGL (works today) or Vulkan (needs a Vulkan
  driver, or lavapipe/SwiftShader in containers)?
- ANGLE binaries: rely on a third-party prebuilt project, or build them in
  our own CI?
- Multiple windows: the X11 layer supports several windows (`wId`); SDL does
  too, but the event loop must dispatch by window id.

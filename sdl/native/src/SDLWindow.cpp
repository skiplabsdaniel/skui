// Windows and events with SDL 3, and OpenGL ES 3.0 through ANGLE.
#include "SDLWindow.h"
#include "GLUtils.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace skui {

namespace {

std::map<uint32_t, Window *> gWindows;
uint32_t gUserEvent = 0;
SDL_Cursor *gCursors[28] = {nullptr};

// The directory of ANGLE's libEGL and libGLESv2: SKUI_ANGLE_DIR at run time,
// or the one the application was built with.
std::string angleLibDir() {
  const char *dir = SDL_getenv("SKUI_ANGLE_DIR");
#ifdef SKUI_ANGLE_DIR
  std::string base = dir != nullptr ? dir : SKUI_ANGLE_DIR;
#else
  std::string base = dir != nullptr ? dir : "/opt/angle";
#endif
  return base + "/lib";
}

#if defined(__APPLE__)
const char *kEGL = "/libEGL.dylib";
const char *kGLES = "/libGLESv2.dylib";
#elif defined(_WIN32)
const char *kEGL = "/libEGL.dll";
const char *kGLES = "/libGLESv2.dll";
#else
const char *kEGL = "/libEGL.so";
const char *kGLES = "/libGLESv2.so";
#endif

SDL_Cursor *systemCursor(uint32_t cursor) {
  // Ui.Cursor codes (framework/src/Cursor.sk).
  static const SDL_SystemCursor kCursors[28] = {
      SDL_SYSTEM_CURSOR_DEFAULT,     // NoneCursor (hidden, see setCursor)
      SDL_SYSTEM_CURSOR_DEFAULT,     // DefaultCursor
      SDL_SYSTEM_CURSOR_POINTER,     // HandCursor
      SDL_SYSTEM_CURSOR_POINTER,     // OpenHandCursor
      SDL_SYSTEM_CURSOR_POINTER,     // ClosedHandCursor
      SDL_SYSTEM_CURSOR_CROSSHAIR,   // CrosshairCursor
      SDL_SYSTEM_CURSOR_DEFAULT,     // DisappearCursor
      SDL_SYSTEM_CURSOR_MOVE,        // MoveCursor
      SDL_SYSTEM_CURSOR_N_RESIZE,    // NorthResizeCursor
      SDL_SYSTEM_CURSOR_NE_RESIZE,   // NorthEastResizeCursor
      SDL_SYSTEM_CURSOR_E_RESIZE,    // EastResizeCursor
      SDL_SYSTEM_CURSOR_SE_RESIZE,   // SouthEastResizeCursor
      SDL_SYSTEM_CURSOR_S_RESIZE,    // SouthResizeCursor
      SDL_SYSTEM_CURSOR_SW_RESIZE,   // SouthWestResizeCursor
      SDL_SYSTEM_CURSOR_W_RESIZE,    // WestResizeCursor
      SDL_SYSTEM_CURSOR_NW_RESIZE,   // NorthWestResizeCursor
      SDL_SYSTEM_CURSOR_EW_RESIZE,   // HorizontalResizeCursor
      SDL_SYSTEM_CURSOR_NS_RESIZE,   // VerticalResizeCursor
      SDL_SYSTEM_CURSOR_TEXT,        // TextCursor
      SDL_SYSTEM_CURSOR_WAIT,        // WaitCursor
      SDL_SYSTEM_CURSOR_NOT_ALLOWED, // ForbiddenCursor
      SDL_SYSTEM_CURSOR_CROSSHAIR,   // PlusCursor
      SDL_SYSTEM_CURSOR_PROGRESS,    // QuestionCursor
      SDL_SYSTEM_CURSOR_DEFAULT,     // DnDCopyCursor
      SDL_SYSTEM_CURSOR_DEFAULT,     // DnDLinkCursor
      SDL_SYSTEM_CURSOR_MOVE,        // DnDMoveCursor
      SDL_SYSTEM_CURSOR_NOT_ALLOWED, // DnDNoneCursor
      SDL_SYSTEM_CURSOR_DEFAULT,     // DnDAskCursor
  };
  if (cursor >= 28) {
    cursor = 1;
  }
  if (gCursors[cursor] == nullptr) {
    gCursors[cursor] = SDL_CreateSystemCursor(kCursors[cursor]);
  }
  return gCursors[cursor];
}

Window *windowOf(SDL_WindowID id) {
  for (auto &it : gWindows) {
    if (!it.second->isClosed() && it.second->sdlId() == id) {
      return it.second;
    }
  }
  return nullptr;
}

} // namespace

void initialize() {
  if (gUserEvent != 0) {
    return;
  }
  // OpenGL ES through ANGLE's EGL, not GLX nor the system GLES driver (see
  // views/NATIVE.md).
  std::string dir = angleLibDir();
  SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
  SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
  SDL_SetHint(SDL_HINT_EGL_LIBRARY, (dir + kEGL).c_str());
  SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, (dir + kGLES).c_str());
  // Keys that type characters are reported in the current layout.
  SDL_SetHint(SDL_HINT_KEYCODE_OPTIONS, "");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  gUserEvent = SDL_RegisterEvents(1);
}

Window::Window(const WindowOptions &options, EventHandler *handler)
    : options(options), handler(handler) {
  initialize();
  this->createWindow();
  gWindows[options.wId] = this;
}

Window::~Window() {
  this->close();
  gWindows.erase(options.wId);
}

void Window::createWindow() {
  int width = options.width;
  int height = options.height;
  // Fitting the screen: the display bounds are in window coordinates, like
  // the window size once scaled below, so they are converted to logical
  // pixels first.
  if (width == 0 || height == 0) {
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    SDL_Rect bounds;
    if (SDL_GetDisplayUsableBounds(display, &bounds)) {
      float displayScale = SDL_GetDisplayContentScale(display);
      if (displayScale <= 0.0) {
        displayScale = 1.0;
      }
      width = (int)(std::min(bounds.w, 1920) / displayScale);
      height = (int)(std::max(bounds.h - 100, 600) / displayScale);
    } else {
      width = 1280;
      height = 800;
    }
  }
  window = SDL_CreateWindow(options.title.c_str(), width, height,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == nullptr) {
    throw std::runtime_error(std::string("SDL_CreateWindow: ") +
                             SDL_GetError());
  }
  // The size is logical. Where window coordinates are device pixels (X11,
  // Windows), the pixel density is 1 and the display scale is the ratio.
  float scale = SDL_GetWindowDisplayScale(window);
  if (SDL_GetWindowPixelDensity(window) == 1.0 && scale > 1.0) {
    SDL_SetWindowSize(window, (int)std::round(width * scale),
                      (int)std::round(height * scale));
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);
  }
  context = SDL_GL_CreateContext(window);
  if (context == nullptr) {
    throw std::runtime_error(std::string("SDL_GL_CreateContext: ") +
                             SDL_GetError());
  }
  SDL_GL_MakeCurrent(window, context);
  SDL_GL_SetSwapInterval(0);
  GL_init();
  SDL_StartTextInput(window);
}

void Window::close() {
  if (window == nullptr) {
    return;
  }
  SDL_StopTextInput(window);
  SDL_GL_DestroyContext(context);
  SDL_DestroyWindow(window);
  context = nullptr;
  window = nullptr;
}

// On macOS and Wayland, window coordinates are logical and the pixel
// density is the ratio. On X11 and Windows, they are device pixels and the
// display scale carries the ratio. The product is right in both cases.
float Window::pixelRatio() const {
  if (window == nullptr) {
    return 1.0;
  }
  float ratio = SDL_GetWindowDisplayScale(window);
  return ratio > 0.0 ? ratio : 1.0;
}

uint32_t Window::ratioPercent() const {
  return (uint32_t)std::round(this->pixelRatio() * 100.0);
}

void Window::size(uint32_t &width, uint32_t &height) const {
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  float ratio = this->pixelRatio();
  width = (uint32_t)std::floor(w / ratio);
  height = (uint32_t)std::floor(h / ratio);
}

void Window::requestClose() {
  if (window != nullptr && handler->requestClose(options.wId)) {
    this->close();
  }
}

void Window::setCursor(uint32_t cursor) {
  if (cursor == 0) {
    SDL_HideCursor();
  } else {
    SDL_ShowCursor();
    SDL_SetCursor(systemCursor(cursor));
  }
}

void Window::frame(uint64_t frames) {
  if (window == nullptr || !frameRequested) {
    return;
  }
  SDL_GL_MakeCurrent(window, context);
  uint32_t width, height;
  this->size(width, height);
  handler->performFrame(options.wId, width, height, this->ratioPercent(),
                        (uint32_t)frames);
  SDL_GL_SwapWindow(window);
}

namespace {

bool isCharacter(SDL_Keycode key) {
  return key != SDLK_UNKNOWN &&
         (key & (SDLK_SCANCODE_MASK | SDLK_EXTENDED_MASK)) == 0;
}

// The name of the key in the current layout: the letter and digit keys are
// named after the character they type (on an AZERTY keyboard, the key at
// the place of Q is KeyA), the other keys after their physical place.
std::string layoutSymbol(const SDL_KeyboardEvent &key) {
  const char *name = webKey(key.key);
  if (*name != '\0') {
    return name;
  }
  if (key.key >= 'a' && key.key <= 'z') {
    return std::string("Key") + (char)(key.key - 'a' + 'A');
  }
  if (key.key >= '0' && key.key <= '9') {
    return std::string("Digit") + (char)key.key;
  }
  return webCode(key.scancode);
}

} // namespace

void Window::sendKeyPressed(const SDL_KeyboardEvent &key,
                            const std::string &typed) {
  const char *code = webCode(key.scancode);
  if (*code == '\0') {
    // A key SDL does not know (or a key synthesized by an input method).
    if (!typed.empty()) {
      handler->textInput(options.wId, typed);
    }
    return;
  }
  // Without a text input event (with Control or Alt), the key reports the
  // character it types without modifiers.
  std::string text = typed;
  if (text.empty() && *webKey(key.key) == '\0' && isCharacter(key.key)) {
    char buffer[8];
    char *end = SDL_UCS4ToUTF8(key.key, buffer);
    text = std::string(buffer, end - buffer);
  }
  handler->keyPressed(options.wId, code, layoutSymbol(key), text, key.mod);
}

void Window::flushKey() {
  if (pendingKey) {
    pendingKey = false;
    this->sendKeyPressed(pending, "");
  }
}

void Window::handleEvent(const SDL_Event &event) {
  float ratio = this->pixelRatio();
  // Mouse coordinates are in window coordinates: device pixels on X11.
  float toLogical = ratio / SDL_GetWindowPixelDensity(window);
  if (event.type != SDL_EVENT_TEXT_INPUT) {
    this->flushKey();
  }
  switch (event.type) {
  case SDL_EVENT_WINDOW_RESIZED:
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
  case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
    uint32_t width, height;
    this->size(width, height);
    handler->resize(options.wId, width, height, this->ratioPercent());
    frameRequested = true;
    break;
  }
  case SDL_EVENT_WINDOW_EXPOSED:
    frameRequested = true;
    break;
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    this->requestClose();
    break;
  case SDL_EVENT_MOUSE_MOTION:
    handler->mouseLocation(options.wId, event.motion.x / toLogical,
                           event.motion.y / toLogical);
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    handler->mouseLocation(options.wId, event.button.x / toLogical,
                           event.button.y / toLogical);
    uint32_t button = event.button.button == SDL_BUTTON_LEFT     ? 1
                      : event.button.button == SDL_BUTTON_MIDDLE ? 2
                      : event.button.button == SDL_BUTTON_RIGHT  ? 3
                                                                 : 0;
    if (button != 0) {
      handler->mouseButton(options.wId, button, event.button.down);
    }
    break;
  }
  case SDL_EVENT_MOUSE_WHEEL: {
    // A wheel notch is a press and release of the X11 wheel buttons 4 and
    // 5, as the framework expects.
    int steps = event.wheel.integer_y;
    if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
      steps = -steps;
    }
    uint32_t button = steps > 0 ? 4 : 5;
    for (int i = 0; i < std::abs(steps); i++) {
      handler->mouseButton(options.wId, button, true);
      handler->mouseButton(options.wId, button, false);
    }
    break;
  }
  case SDL_EVENT_KEY_DOWN:
    if (SDL_TextInputActive(window) && *webKey(event.key.key) == '\0') {
      // The text input event that may follow gives the typed text, with
      // the modifiers, the layout and dead keys applied.
      pendingKey = true;
      pending = event.key;
    } else {
      this->sendKeyPressed(event.key, "");
    }
    break;
  case SDL_EVENT_KEY_UP: {
    const char *code = webCode(event.key.scancode);
    if (*code != '\0') {
      handler->keyReleased(options.wId, code);
    }
    break;
  }
  case SDL_EVENT_TEXT_INPUT:
    if (pendingKey) {
      pendingKey = false;
      this->sendKeyPressed(pending, event.text.text);
    } else {
      handler->textInput(options.wId, event.text.text);
    }
    break;
  default:
    break;
  }
}

void run(std::function<void(uint32_t, uint32_t, uint32_t)> onUser) {
  uint64_t frames = 0;
  // The frame rate of the fastest window.
  uint32_t fps = 1;
  for (auto &it : gWindows) {
    fps = std::max(fps, it.second->framePerSecond());
  }
  auto period = std::chrono::microseconds(1000000 / fps);
  auto next = std::chrono::steady_clock::now();
  while (true) {
    bool open = false;
    for (auto &it : gWindows) {
      open = open || !it.second->isClosed();
    }
    if (!open) {
      break;
    }
    auto now = std::chrono::steady_clock::now();
    int timeout = (int)std::max<int64_t>(
        0, std::chrono::duration_cast<std::chrono::milliseconds>(next - now)
               .count());
    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, timeout)) {
      do {
        if (event.type == gUserEvent) {
          onUser(event.user.code, (uint32_t)(uintptr_t)event.user.data1,
                 (uint32_t)(uintptr_t)event.user.data2);
        } else if (event.type == SDL_EVENT_QUIT) {
          for (auto &it : gWindows) {
            it.second->requestClose();
          }
        } else {
          SDL_Window *sdlWindow = SDL_GetWindowFromEvent(&event);
          if (sdlWindow != nullptr) {
            Window *window = windowOf(SDL_GetWindowID(sdlWindow));
            if (window != nullptr) {
              window->handleEvent(event);
            }
          }
        }
      } while (SDL_PollEvent(&event));
      for (auto &it : gWindows) {
        it.second->flushKey();
      }
    }
    now = std::chrono::steady_clock::now();
    if (now >= next) {
      frames++;
      for (auto &it : gWindows) {
        it.second->frame(frames);
      }
      GL_checkTextureToDelete();
      next += period;
      if (next < now) {
        next = now + period;
      }
    }
  }
  SDL_Quit();
}

void pushUserEvent(uint32_t type, uint32_t param1, uint32_t param2) {
  SDL_Event event;
  SDL_zero(event);
  event.type = gUserEvent;
  event.user.code = (Sint32)type;
  event.user.data1 = (void *)(uintptr_t)param1;
  event.user.data2 = (void *)(uintptr_t)param2;
  SDL_PushEvent(&event);
}

Window *findWindow(uint32_t wId) {
  auto it = gWindows.find(wId);
  return it == gWindows.end() ? nullptr : it->second;
}

std::string clipboardText() {
  char *text = SDL_GetClipboardText();
  std::string result = text != nullptr ? text : "";
  SDL_free(text);
  return result;
}

void setClipboardText(const std::string &text) {
  SDL_SetClipboardText(text.c_str());
}

} // namespace skui

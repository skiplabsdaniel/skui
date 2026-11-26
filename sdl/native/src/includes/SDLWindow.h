#ifndef __SKUI_SDL_WINDOW_H__
#define __SKUI_SDL_WINDOW_H__

#include <SDL3/SDL.h>
#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace skui {

// The events of a window, in logical pixels (device pixels divided by the
// pixel ratio).
class EventHandler {
public:
  virtual ~EventHandler() {}
  // Draws a frame of width x height logical pixels at the given pixel ratio
  // (in hundredths); the GL context is current.
  virtual void performFrame(uint32_t wId, uint32_t width, uint32_t height,
                            uint32_t ratio, uint32_t frames) = 0;
  virtual void resize(uint32_t /*wId*/, uint32_t /*width*/,
                      uint32_t /*height*/, uint32_t /*ratio*/) {}
  // Returns true to close the window.
  virtual bool requestClose(uint32_t /*wId*/) { return true; }
  virtual void mouseLocation(uint32_t /*wId*/, float /*x*/, float /*y*/) {}
  // button: 1 primary, 2 middle, 3 secondary, 4 wheel up, 5 wheel down.
  virtual void mouseButton(uint32_t /*wId*/, uint32_t /*button*/,
                           bool /*pressed*/) {}
  // code: the web code of the physical key (KeyboardEvent.code); symbol: the
  // web name of the key in the current layout; text: the text the key types,
  // if any.
  virtual void keyPressed(uint32_t /*wId*/, const std::string & /*code*/,
                          const std::string & /*symbol*/,
                          const std::string & /*text*/, SDL_Keymod /*mod*/) {}
  virtual void keyReleased(uint32_t /*wId*/, const std::string & /*code*/) {}
  // Text typed with an input method or a dead key, not attached to a key.
  virtual void textInput(uint32_t /*wId*/, const std::string & /*text*/) {}
};

struct WindowOptions {
  uint32_t wId = 0;
  std::string title;
  // Logical size, or 0 to fit the screen.
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t framePerSecond = 60;
};

// An SDL window with an OpenGL ES 3.0 context created through ANGLE, and
// its event loop. Several windows can share the loop (see run).
class Window {
public:
  Window(const WindowOptions &options, EventHandler *handler);
  ~Window();

  uint32_t id() const { return options.wId; }
  SDL_WindowID sdlId() const { return SDL_GetWindowID(window); }
  // Device pixels per logical pixel.
  float pixelRatio() const;
  // The pixel ratio in hundredths.
  uint32_t ratioPercent() const;
  // The size of the window in logical pixels.
  void size(uint32_t &width, uint32_t &height) const;
  uint32_t framePerSecond() const { return options.framePerSecond; }
  void setCursor(uint32_t cursor);
  // Asks the handler, and closes the window if it agrees.
  void requestClose();
  void requestFrame() { frameRequested = true; }
  void close();
  bool isClosed() const { return window == nullptr; }

  void handleEvent(const SDL_Event &event);
  // Sends the key press waiting for its text, if any.
  void flushKey();
  void frame(uint64_t frames);

private:
  void createWindow();
  void sendKeyPressed(const SDL_KeyboardEvent &key, const std::string &typed);

  WindowOptions options;
  EventHandler *handler;
  SDL_Window *window = nullptr;
  SDL_GLContext context = nullptr;
  bool frameRequested = true;
  // A key press whose text may follow in a SDL_EVENT_TEXT_INPUT event.
  bool pendingKey = false;
  SDL_KeyboardEvent pending;
};

// Initializes SDL video and the ANGLE OpenGL ES 3.0 attributes. Throws a
// std::runtime_error on failure.
void initialize();

// Runs the event loop until every window is closed. onUser is called on the
// loop thread for the events pushed with pushUserEvent.
void run(std::function<void(uint32_t, uint32_t, uint32_t)> onUser);

// Thread safe: wakes up the event loop with a user event (type, param1,
// param2).
void pushUserEvent(uint32_t type, uint32_t param1, uint32_t param2);

Window *findWindow(uint32_t wId);

// The web code (KeyboardEvent.code) of an SDL scancode, or "" if unknown.
const char *webCode(SDL_Scancode scancode);
// The web key name of an SDL keycode that does not type a character, or "".
const char *webKey(SDL_Keycode key);

// The clipboard, as UTF-8 text.
std::string clipboardText();
void setClipboardText(const std::string &text);

} // namespace skui

#endif

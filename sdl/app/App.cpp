// The window of a framework application, with SDL 3 (see sdl/extra for the
// Skip side). Compiled with the application, which links the framework.
#include "SDLWindow.h"
#include "Timer.h"
#include <atomic>
#include <iostream>
#include <map>
#include <mutex>

#define TXT(from, to) std::string to(from, SKIP_String_byteSize(from))

namespace {

// User event types of the event loop.
enum UserEvent : uint32_t {
  kUpdate = 1,
  kMessage = 2,
};

std::atomic<uint32_t> nextMsgId{0};
std::mutex messagesMutex;
std::map<uint32_t, std::string> messages;
std::map<uint32_t, skui::Timer> timers;

} // namespace

extern "C" {
uint32_t SKIP_String_byteSize(char *str);
char *SKIP_new_Obstack();
void SKIP_destroy_Obstack(char *saved);
char *sk_string_create(const char *buffer, uint32_t size);
void SKIP_SKUISDL_manageKeyPressed(uint64_t wId, char *code, char *symbol,
                                   char *text, bool capsLock, bool numLock);
void SKIP_SKUISDL_manageKeyReleased(uint64_t wId, char *code);
void SKIP_SKUISDL_manageTextInput(uint64_t wId, char *text);
void SKIP_SKUISDL_manageMouseButton(uint64_t wId, uint64_t button,
                                    bool pressed);
void SKIP_SKUISDL_manageMouseLocation(uint64_t wId, double x, double y);
bool SKIP_SKUISDL_manageRequestClose(uint64_t wId);
void SKIP_SKUISDL_manageResize(uint64_t wId, uint64_t w, uint64_t h,
                               uint64_t r);
void SKIP_SKUISDL_windowInitialized(uint32_t wId);
void SKIP_SKUIGL_performFrame(uint32_t ctxId, uint32_t width, uint32_t height,
                              uint32_t ratio, uint32_t frames);
void SKIP_SKUIGL_setWindowContext(uint32_t wId, uint32_t ctxId);
void SKIP_SKUIMessage_manage(char *message);
void SKUI_update();

void SKIP_FWK_applyCursor(uint32_t wId, uint32_t cursor) {
  skui::Window *window = skui::findWindow(wId);
  if (window != nullptr) {
    window->setCursor(cursor);
  }
}

// May be called from any thread.
void SKIP_FWK_update(uint32_t wId) {
  skui::pushUserEvent(kUpdate, wId, 0);
}

void SKIP_SKUISDL_sendMessage(uint64_t wId, char *message, bool,
                              uint32_t delay) {
  TXT(message, strMessage);
  auto msgId = nextMsgId++;
  {
    std::lock_guard<std::mutex> lock(messagesMutex);
    messages[msgId] = strMessage;
  }
  if (delay > 0) {
    std::lock_guard<std::mutex> lock(messagesMutex);
    timers[msgId].setTimeout(
        [wId, msgId]() {
          skui::pushUserEvent(kMessage, (uint32_t)wId, msgId);
        },
        delay);
  } else {
    skui::pushUserEvent(kMessage, (uint32_t)wId, msgId);
  }
}
}

namespace {

void manageMessage(uint32_t msgId) {
  std::string message;
  {
    std::lock_guard<std::mutex> lock(messagesMutex);
    auto it = messages.find(msgId);
    if (it == messages.end()) {
      return;
    }
    message = it->second;
    messages.erase(it);
    timers.erase(msgId);
  }
  auto obstack = SKIP_new_Obstack();
  SKIP_SKUIMessage_manage(sk_string_create(message.c_str(), message.size()));
  SKIP_destroy_Obstack(obstack);
}

char *skString(const std::string &value) {
  return sk_string_create(value.c_str(), value.size());
}

class AppHandler : public skui::EventHandler {
public:
  void performFrame(uint32_t wId, uint32_t width, uint32_t height,
                    uint32_t ratio, uint32_t frames) override {
    SKIP_SKUIGL_performFrame(wId, width, height, ratio, frames);
  }

  void resize(uint32_t wId, uint32_t width, uint32_t height,
              uint32_t ratio) override {
    SKIP_SKUISDL_manageResize(wId, width, height, ratio);
  }

  bool requestClose(uint32_t wId) override {
    return SKIP_SKUISDL_manageRequestClose(wId);
  }

  void mouseLocation(uint32_t wId, float x, float y) override {
    SKIP_SKUISDL_manageMouseLocation(wId, x, y);
  }

  void mouseButton(uint32_t wId, uint32_t button, bool pressed) override {
    SKIP_SKUISDL_manageMouseButton(wId, button, pressed);
  }

  void keyPressed(uint32_t wId, const std::string &code,
                  const std::string &symbol, const std::string &text,
                  SDL_Keymod mod) override {
    auto obstack = SKIP_new_Obstack();
    SKIP_SKUISDL_manageKeyPressed(wId, skString(code), skString(symbol),
                                  skString(text), (mod & SDL_KMOD_CAPS) != 0,
                                  (mod & SDL_KMOD_NUM) != 0);
    SKIP_destroy_Obstack(obstack);
  }

  void keyReleased(uint32_t wId, const std::string &code) override {
    auto obstack = SKIP_new_Obstack();
    SKIP_SKUISDL_manageKeyReleased(wId, skString(code));
    SKIP_destroy_Obstack(obstack);
  }

  void textInput(uint32_t wId, const std::string &text) override {
    auto obstack = SKIP_new_Obstack();
    SKIP_SKUISDL_manageTextInput(wId, skString(text));
    SKIP_destroy_Obstack(obstack);
  }
};

} // namespace

extern "C" {

void SKIP_SKUISDL_launch(uint64_t wId, char *skTitle,
                         uint64_t framePerSecond) {
  TXT(skTitle, title);
  try {
    AppHandler handler;
    skui::WindowOptions options;
    options.wId = (uint32_t)wId;
    options.title = title;
    options.framePerSecond = (uint32_t)framePerSecond;
    skui::Window window(options, &handler);
    SKIP_SKUIGL_setWindowContext(wId, wId);
    uint32_t width, height;
    window.size(width, height);
    SKIP_SKUISDL_manageResize(wId, width, height, window.ratioPercent());
    SKIP_SKUISDL_windowInitialized(wId);
    skui::run([](uint32_t type, uint32_t, uint32_t param) {
      switch (type) {
      case kUpdate:
        SKUI_update();
        break;
      case kMessage:
        manageMessage(param);
        break;
      default:
        break;
      }
    });
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
}

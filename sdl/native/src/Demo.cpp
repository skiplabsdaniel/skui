// The window of the GL drawables viewer (skuisdl), without the framework.
#include "SDLWindow.h"
#include <iostream>

#define TXT(from, to) std::string to(from, SKIP_String_byteSize(from))

extern "C" {
uint32_t SKIP_String_byteSize(char *str);
void *SKIP_SkuiSdl_performFrame(void *environment, uint32_t ctxId,
                                void *userContext, uint32_t width,
                                uint32_t height, uint32_t ratio,
                                uint32_t frames);
}

namespace {

class DemoHandler : public skui::EventHandler {
public:
  explicit DemoHandler(void *environment) : environment(environment) {}

  void performFrame(uint32_t wId, uint32_t width, uint32_t height,
                    uint32_t ratio, uint32_t frames) override {
    userContext = SKIP_SkuiSdl_performFrame(environment, wId, userContext,
                                            width, height, ratio, frames);
  }

private:
  void *environment;
  void *userContext = nullptr;
};

} // namespace

extern "C" {
void SKIP_SkuiSdl_run(void *environment, uint64_t wId, uint64_t width,
                      uint64_t height, char *skTitle,
                      uint64_t framePerSecond) {
  TXT(skTitle, title);
  try {
    DemoHandler handler(environment);
    skui::WindowOptions options;
    options.wId = (uint32_t)wId;
    options.title = title;
    options.width = (uint32_t)width;
    options.height = (uint32_t)height;
    options.framePerSecond = (uint32_t)framePerSecond;
    skui::Window window(options, &handler);
    skui::run([](uint32_t, uint32_t, uint32_t) {});
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
}

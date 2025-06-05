#include <functional>
#include <iostream>
#include <optional>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>

extern "C" {

typedef struct {
  int64_t status;
  const char *headers;
  const char *body;
  const char *error;
} fetch_result_t;

fetch_result_t sk_inner_fetch(const char *, const char *, const char *,
                              const char *, int64_t);
void *SKIP_new_Obstack();
void SKIP_destroy_Obstack(void *saved);
uint32_t SKIP_String_byteSize(const char *str);
const char *sk_string_create(const char *buffer, uint32_t size);
void SKUI_runInMain(uint64_t wId, std::function<void()> runnable);
void skui_fetch_result(const char *, const char *, const char *, const char *,
                       const char *, int64_t, void *);
void *sk_create_result(int64_t, const char *, const char *, const char *);

#define TXT(from, to) std::string to(from, SKIP_String_byteSize(from))
#define STR(text) sk_string_create(text.c_str(), text.size())
#define CSTR(text)                                                             \
  text != nullptr ? sk_string_create(text, strlen(text)) : nullptr

void skui_fetch(uint64_t wId, const char *url, const char *method,
                const char *headers, const char *body, int64_t timeout_s,
                const char *executor, int64_t time) {
  TXT(url, strUrl);
  TXT(method, strMethod);
  TXT(headers, strHeader);
  std::optional<std::string> oStrBody;
  if (body != nullptr) {
    TXT(body, strBody);
    oStrBody.emplace(strBody);
  }
  TXT(executor, strExecutor);
  std::thread t([wId, strUrl, strMethod, strHeader, oStrBody, strExecutor,
                 timeout_s, time]() {
    const char *skbody = oStrBody.has_value() ? STR((*oStrBody)) : nullptr;
    void *obstack = SKIP_new_Obstack();
    fetch_result_t result = sk_inner_fetch(STR(strUrl), STR(strMethod),
                                           STR(strHeader), skbody, timeout_s);
    fetch_result_t copied = {result.status,
                             result.headers ? strdup(result.headers) : nullptr,
                             result.body ? strdup(result.body) : nullptr,
                             result.error ? strdup(result.error) : nullptr};
    SKUI_runInMain(wId, [time, strUrl, strMethod, strHeader, oStrBody,
                         strExecutor, copied]() {
      void *obstack = SKIP_new_Obstack();
      const char *skurl = STR(strUrl);
      const char *skmethod = STR(strMethod);
      const char *skheader = STR(strHeader);
      const char *skbody = oStrBody.has_value() ? STR((*oStrBody)) : nullptr;
      const char *skexecutor = STR(strExecutor);
      const char *rHeader = CSTR(copied.headers);
      const char *rBody = CSTR(copied.body);
      const char *rError = CSTR(copied.error);
      void *result = sk_create_result(copied.status, rHeader, rBody, rError);
      skui_fetch_result(skurl, skmethod, skheader, skbody, skexecutor, time,
                        result);
      SKIP_destroy_Obstack(obstack);
    });
    SKIP_destroy_Obstack(obstack);
  });
  t.detach();
}
} // extern "C"
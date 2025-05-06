#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int64_t status;
  const char *headers;
  const char *body;
  const char *error;
} fetch_result_t;

fetch_result_t rs_fetch(const char *, const char *, const char *, const char *,
                        int64_t);
void rs_free_fetch_result(fetch_result_t);
const char *sk_string_create(const char *buffer, uint32_t size);
void *sk_create_result(int64_t, const char *, const char *, const char *);

void *sk_fetch(const char *url, const char *method, const char *headers,
               const char *body, int64_t timeout_s) {
  fetch_result_t result = rs_fetch(url, method, headers, body, timeout_s);
  const char *rheaders =
      result.headers ? sk_string_create(result.headers, strlen(result.headers))
                     : NULL;
  const char *rbody =
      result.body ? sk_string_create(result.body, strlen(result.body)) : NULL;
  const char *error = result.error
                          ? sk_string_create(result.error, strlen(result.error))
                          : NULL;
  rs_free_fetch_result(result);
  return sk_create_result(result.status, rheaders, rbody, error);
}
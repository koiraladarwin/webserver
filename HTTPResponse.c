#include "HTTPResponse.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CANARY_VALUE 0xDEADBEEFCAFEBABEUL

#define CHECK(stage)                                                           \
  do {                                                                         \
    printf("[CHECK %-25s] canary=0x%lx\n", stage, canary);                     \
    printf("  res=%p\n", (void *)res);                                         \
    printf("  res_buffer=%p (cap=%zu size=%zu)\n", (void *)res->res_buffer,    \
           res->res_buffer_capacity, res->res_buffer_size);                    \
    printf("  status_line=%p\n", (void *)res->status_line);                    \
    printf("  headers=%p\n", (void *)res->headers);                            \
    printf("  body=%p (len=%zu)\n", (void *)body, body_len);                   \
    fflush(stdout);                                                            \
  } while (0)

int rw_write_status_code(HTTPResponseWriter *res, int code) {
  snprintf(res->status_line, sizeof(res->status_line), "HTTP/1.1 %d OK\r\n",
           code);
  return 0;
}

int rw_write_header(HTTPResponseWriter *res, const char *key,
                    const char *value) {
  int n = snprintf(res->headers + strlen(res->headers),
                   sizeof(res->headers) - strlen(res->headers), "%s: %s\r\n",
                   key, value);
  return n;
}

// -2 is try again later
// -1 is error
// 0 is fully flushed
// 2 partial flush
int rw_flush(HTTPResponseWriter *res) {
  if (res->res_buffer_written == res->res_buffer_size) {
    return 0;
  }

  int n = write(res->client_fd, res->res_buffer + res->res_buffer_written,
                res->res_buffer_size - res->res_buffer_written);

  if (n == 0) {
    return -2;
  }

  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return -2; // try again later
    }
    return -1; // real error
  }

  res->res_buffer_written += n;
  if (res->res_buffer_written == res->res_buffer_size) {
    return 0;
  }
  return 2;
}

int rw_write_body(HTTPResponseWriter *res, const char *body, size_t body_len) {
  static unsigned long canary = CANARY_VALUE;

  printf("\n========== ENTER rw_write_body ==========\n");
  CHECK("entry");

  /* ---- write Content-Length header ---- */
  char content_length_str[32];
  snprintf(content_length_str, sizeof(content_length_str), "%zu", body_len);

  printf("[INFO] calling write_header(Content-Length)\n");
  res->write_header(res, "Content-Length", content_length_str);
  CHECK("after write_header");

  /* ---- calculate sizes ---- */
  size_t status_len = strlen(res->status_line);
  size_t headers_len = strlen(res->headers);

  size_t total_size = status_len + headers_len + 2 + body_len;

  printf("[INFO] status_len=%zu headers_len=%zu body_len=%zu total=%zu\n",
         status_len, headers_len, body_len, total_size);

  /* ---- ensure buffer capacity ---- */
  if (res->res_buffer_capacity < total_size) {
    printf("[INFO] realloc needed (old cap=%zu)\n", res->res_buffer_capacity);

    res->res_buffer_capacity = total_size;

    char *tmp = realloc(res->res_buffer, res->res_buffer_capacity);
    printf("[INFO] realloc returned %p\n", (void *)tmp);

    if (!tmp) {
      printf("[ERROR] realloc failed\n");
      return -1;
    }

    res->res_buffer = tmp;
  }

  CHECK("after realloc");

  /* ---- build response buffer ---- */
  size_t offset = 0;

  printf("[COPY] status line\n");
  memcpy(res->res_buffer + offset, res->status_line, status_len);
  offset += status_len;
  CHECK("after status memcpy");

  printf("[COPY] headers\n");
  memcpy(res->res_buffer + offset, res->headers, headers_len);
  offset += headers_len;
  CHECK("after headers memcpy");

  printf("[COPY] CRLF\n");
  memcpy(res->res_buffer + offset, "\r\n", 2);
  offset += 2;
  CHECK("after CRLF memcpy");

  printf("[COPY] body\n");
  memcpy(res->res_buffer + offset, body, body_len);
  offset += body_len;
  CHECK("after body memcpy");

  /* ---- finalize ---- */
  res->res_buffer_size = offset;

  printf("========== EXIT rw_write_body ==========\n");
  fflush(stdout);

  return 0;
}
HTTPResponseWriter *make_http_response_writer(int client_fd) {
  HTTPResponseWriter *res = malloc(sizeof(HTTPResponseWriter));
  res->client_fd = client_fd;

  res->headers[0] = '\0';

  res->status_line[0] = '\0';

  res->write_status_code = rw_write_status_code;
  res->write_header = rw_write_header;
  res->write_body = rw_write_body;
  res->res_buffer_capacity = 1024;
  res->res_buffer_size = 0;
  res->res_buffer = malloc(res->res_buffer_capacity);

  return res;
}

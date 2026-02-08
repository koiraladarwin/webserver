#include "../includes/HTTPResponse.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int rw_write_status_code(HTTPResponseWriter *res, int code) {
  snprintf(res->status_line, sizeof(res->status_line), "HTTP/1.1 %d OK\r\n",
           code);
  return 0;
}

int rw_write_header(HTTPResponseWriter *res, const char *key,
                    const char *value) {
  size_t needed =
      strlen(key) + 2 + strlen(value) + 2; 

  if (res->headers_size + needed >= res->headers_capacity) {
    size_t new_capacity =
        (res->headers_capacity * 2 > res->headers_size + needed)
            ? res->headers_capacity * 2
            : res->headers_size + needed + 1;

    char *tmp = realloc(res->headers, new_capacity);
    if (!tmp) {
      fprintf(stderr, "[ERROR] realloc failed in rw_write_header\n");
      return -1;
    }

    res->headers = tmp;
    res->headers_capacity = new_capacity;
  }

  int n = snprintf(res->headers + res->headers_size,
                   res->headers_capacity - res->headers_size, "%s: %s\r\n", key,
                   value);

  if (n < 0) {
    return -1; // encoding error
  }

  res->headers_size += n;

  return 0;
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
  char content_length_str[32];
  snprintf(content_length_str, sizeof(content_length_str), "%zu", body_len);

  res->write_header(res, "Content-Length", content_length_str);

  size_t status_len = strlen(res->status_line);
  size_t headers_len = strlen(res->headers);

  size_t total_size = status_len + headers_len + 2 + body_len;

  if (res->res_buffer_capacity < total_size) {
    res->res_buffer_capacity = total_size;

    char *tmp = realloc(res->res_buffer, res->res_buffer_capacity);

    if (!tmp) {
      return -1;
    }

    res->res_buffer = tmp;
  }

  size_t offset = 0;

  memcpy(res->res_buffer + offset, res->status_line, status_len);
  offset += status_len;

  memcpy(res->res_buffer + offset, res->headers, headers_len);
  offset += headers_len;

  memcpy(res->res_buffer + offset, "\r\n", 2);
  offset += 2;

  memcpy(res->res_buffer + offset, body, body_len);
  offset += body_len;

  res->res_buffer_size = offset;

  return 0;
}

HTTPResponseWriter *make_http_response_writer(int client_fd) {
  HTTPResponseWriter *res = malloc(sizeof(HTTPResponseWriter));
  res->client_fd = client_fd;

  res->headers_capacity = 1024;
  res->headers_size = 0;
  res->headers = malloc(res->headers_capacity);

  res->status_line[0] = '\0';

  res->write_status_code = rw_write_status_code;
  res->write_header = rw_write_header;
  res->write_body = rw_write_body;
  res->res_buffer_capacity = 1024;
  res->res_buffer_size = 0;
  res->res_buffer = malloc(res->res_buffer_capacity);
  res->res_buffer_written = 0;

  return res;
}

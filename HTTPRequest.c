#include "HTTPRequest.h"
#include "Helper.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#define MAX_HTTP_VERSION_SIZE 8

void queries_free(HTTPQuery *query, size_t len) {
  for (int i = 0; i < len; i++) {
    free(query->key);
    free(query->value);
  }
}

void parse_queries(HTTPRequest *req) {
  req->query_count = 0;

  // unnessary now since uri is a duped string
  /*char *temp_uri = malloc(req->URI_len);*/
  /*memcpy(temp_uri, req->URI, req->URI_len);*/

  char *query_start_pos = strchr(req->URI, '?');
  if (!query_start_pos)
    return; // no query

  char *query_str = query_start_pos + 1; // skip '?'
  char *rest = query_str;
  char *token;

  while ((token = strsep(&rest, "&")) != NULL &&
         req->query_count < MAX_QUERY_PARAMS) {
    char *equals = strchr(token, '=');
    if (equals) {
      *equals = '\0';
      req->queries[req->query_count].key = strdup(token);
      req->queries[req->query_count].value = strdup(equals + 1);
    } else {
      req->queries[req->query_count].key = strdup(token);
      req->queries[req->query_count].value = NULL;
    }
    req->query_count++;
  }

  *query_start_pos = '\0';
}

char *get_header(HTTPHeaders *h, char *key) {
  for (size_t i = 0; i < h->size; i++) {
    if (h->header[i].name) {
      printf("%zu) name: %s\n",i,h->header[i].name);
      if (strncasecmp(h->header[i].name, key, strlen(key)) == 0) {
        return h->header[i].value;
      }
    }
  }

  return NULL;
}

HTTPHeaders *headers_init() {
  HTTPHeaders *h = malloc(sizeof(HTTPHeaders));
  if (!h)
    return NULL;

  h->size = 0;
  h->capacity = 20;
  h->header = malloc(sizeof(HTTPHeader) * h->capacity);
  return h;
}

void headers_add(HTTPHeaders *h, char *name, char *value) {

  if (h->size >= h->capacity) {
    h->capacity *= 2;

    HTTPHeader *tmp = realloc(h->header, sizeof(HTTPHeader) * h->capacity);
    if (!tmp) {
      // freeing intentionally
      free(name);
      free(value);
      return;
    }

    h->header = tmp;
  }

  h->header[h->size].name = name;
  h->header[h->size].value = value;
  h->size++;
}

void headers_free(HTTPHeaders *h) {
  if (!h)
    return;

  for (size_t i = 0; i < h->size; i++) {
    free(h->header[i].name);
    free(h->header[i].value);
  }

  free(h->header);
  free(h);
}

int parse_headers(char *req, size_t req_len, HTTPRequest *out) {
  if (!req || !out)
    return -1;
  memset(out, 0, sizeof(*out));
  char *headers_terminator = memmem(req, req_len, "\r\n\r\n", 4);
  if (!headers_terminator)
    return -2;

  if ((headers_terminator - req) + 4 <= req_len) {
    out->body = headers_terminator + 4;
    out->body_len = req_len - ((headers_terminator - req) + 4);
  } else {
    out->body = NULL;
    out->body_len = 0;
  }
  char *req_line_end = memmem(req, req_len, "\r\n", 2);
  if (!req_line_end || req_line_end > headers_terminator)
    return -1;

  char *sp1 = memmem(req, req_line_end - req, " ", 1);
  if (!sp1 || sp1 > headers_terminator)
    return -1;
  char *sp2 = memmem(sp1 + 1, req_line_end - (sp1 + 1), " ", 1);
  if (!sp2 || sp2 > headers_terminator)
    return -1;

  char *method = req;
  size_t method_len = sp1 - req;

  if (method_len == 3 && strncmp(method, "GET", method_len) == 0) {
    out->method = GET;
  } else if (method_len == 4 && strncmp(method, "POST", method_len) == 0) {
    out->method = POST;
  } else if (method_len == 7 && strncmp(method, "OPTIONS", method_len) == 0) {
    out->method = OPTIONS;
  } else if (method_len == 3 && strncmp(method, "PUT", method_len) == 0) {
    out->method = PUT;
  } else if (method_len == 6 && strncmp(method, "DELETE", method_len) == 0) {
    out->method = DELETE;
  }

  char *uri = sp1 + 1;
  size_t uri_len = sp2 - uri;
  if (!uri || uri > headers_terminator) {
    return -1;
  }

  out->URI = malloc(uri_len + 1);
  if (!out->URI)
    return -1;
  memcpy(out->URI, uri, uri_len);
  out->URI[uri_len] = '\0';
  out->URI_len = uri_len;

  char *ver = sp2 + 1;
  size_t ver_len = req_line_end - ver;
  if (!ver || ver > headers_terminator) {
  }
  if (ver_len > MAX_HTTP_VERSION_SIZE) {
    return -1;
  }
  char ver_temp[MAX_HTTP_VERSION_SIZE + 1];
  size_t ver_copy_len =
      ver_len > MAX_HTTP_VERSION_SIZE ? MAX_HTTP_VERSION_SIZE : ver_len;
  memcpy(ver_temp, ver, ver_copy_len);
  ver_temp[ver_copy_len] = '\0';

  float version_float;
  if (sscanf(ver_temp, "HTTP/%f", &version_float) != 1) {
    return -1;
  }

  out->version = version_float;

  char *header_start = req_line_end + 2;
  char *header_end;

  if (!out->headers) {
    out->headers = headers_init();
  }
  int i = 0;
  while (
      (header_end = memmem(header_start, headers_terminator + 2 - header_start,
                           "\r\n", 2))) {
    char *colon = memmem(header_start, header_end - header_start, ":", 1);
    if (colon) {
      char *name = strndup(header_start, colon - header_start);
      char *value_start = colon + 1;
      while (value_start < header_end &&
             (*value_start == ' ' || *value_start == '\t'))
        value_start++;

      char *value = strndup(value_start, header_end - value_start);

      headers_add(out->headers, name, value);
    }

    header_start = header_end + 2;
  }
  return (headers_terminator + 4) - req;
}

int ParseRequest(char *request, HTTPRequest *out) {
  if (!request || !out)
    return -1;

  memset(out, 0, sizeof(HTTPRequest));

  char *req = strdup(request);
  if (!req)
    return -1;

  // body first before strtok_r
  char *body = strstr(req, "\r\n\r\n");
  if (body) {
    *body = '\0';
    body += 4; // skip \r\n\r\n
    if (*body != '\0')
      out->body = strdup(body);
  }

  // parsing request line
  char *save_line;
  char *line = strtok_r(req, "\r\n", &save_line);
  if (!line) {
    free(req);
    return -1;
  }

  char *save_word_pointer;
  char *method = strtok_r(line, " ", &save_word_pointer);
  char *uri = strtok_r(NULL, " ", &save_word_pointer);
  char *ver = strtok_r(NULL, " ", &save_word_pointer);

  if (!method || !uri || !ver) {
    free(req);
    return -1;
  }

  if (!strcmp(method, "GET"))
    out->method = GET;
  else if (!strcmp(method, "POST"))
    out->method = POST;
  else if (!strcmp(method, "PUT"))
    out->method = PUT;
  else if (!strcmp(method, "DELETE"))
    out->method = DELETE;
  else if (!strcmp(method, "OPTIONS"))
    out->method = OPTIONS;
  else {
    free(req);
    return -1;
  }

  out->URI = strdup(uri);

  if (sscanf(ver, "HTTP/%f", &out->version) != 1) {
    free(req);
    return -1;
  }

  // parsing headers
  out->headers = headers_init();
  if (!out->headers) {
    free(req);
    return -1;
  }

  while ((line = strtok_r(NULL, "\r\n", &save_line))) {
    if (*line == '\0')
      continue;

    char *colon = strchr(line, ':');
    if (!colon)
      continue;

    *colon = '\0';
    char *name = line;
    char *value = colon + 1;

    while (*value == ' ')
      value++;

    headers_add(out->headers, name, value);
  }

  free(req);
  return 0;
}

const char *method_to_string(int method) {
  switch (method) {
  case 0:
    return "GET";
  case 1:
    return "POST";
  case 2:
    return "OPTIONS";
  case 3:
    return "PUT";
  case 4:
    return "DELETE";
  default:
    return "UNKNOWN";
  }
}

void PrintHTTPRequest(const HTTPRequest *req) {
  if (!req) {
    printf("HTTPRequest: NULL\n");
    return;
  }

  printf("====== HTTP REQUEST ======\n");

  printf("Method  : %s (%d)\n", method_to_string(req->method), req->method);
  printf("URI     : %s\n", req->URI ? req->URI : "(null)");
  printf("Version : HTTP/%.1f\n", req->version);

  printf("\n--- Headers ---\n");
  if (req->headers && req->headers->size > 0) {
    for (size_t i = 0; i < req->headers->size; i++) {
      printf("%s: %s\n", req->headers->header[i].name,
             req->headers->header[i].value);
    }
  } else {
    printf("(no headers)\n");
  }

  printf("\n--- Body ---\n");
  if (req->body) {
    printf("%s\n", req->body);
  } else {
    printf("(no body)\n");
  }

  printf("===========================\n");
}

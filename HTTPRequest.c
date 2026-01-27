#include "HTTPRequest.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *get_header(HTTPHeaders *h, char *key) {
  for (size_t i = 0; i < h->size; h++) {
    if (strcmp(key, h->headers[i].name) == 0) {
      return h->headers[i].value;
    }
  }
  return NULL;
}

HTTPHeaders *headers_create() {
  HTTPHeaders *h = malloc(sizeof(HTTPHeaders));
  if (!h)
    return NULL;

  h->size = 0;
  h->capacity = 8;
  h->headers = malloc(sizeof(HTTPHeader) * h->capacity);
  return h;
}

void headers_add(HTTPHeaders *h, char *name, char *value) {
  if (h->size >= h->capacity) {
    h->capacity *= 2;
    h->headers = realloc(h->headers, sizeof(HTTPHeader) * h->capacity);
  }

  h->headers[h->size].name = strdup(name);
  h->headers[h->size].value = strdup(value);
  h->size++;
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
        body += 4;          // skip \r\n\r\n
        if (*body != '\0')
            out->body = strdup(body);
    }

    //parsing request line
    char *save_line;
    char *line = strtok_r(req, "\r\n", &save_line);
    if (!line) {
        free(req);
        return -1;
    }

    char *save_word_pointer;
    char *method = strtok_r(line, " ", &save_word_pointer);
    char *uri    = strtok_r(NULL, " ", &save_word_pointer);
    char *ver    = strtok_r(NULL, " ", &save_word_pointer);

    if (!method || !uri || !ver) {
        free(req);
        return -1;
    }

    if      (!strcmp(method, "GET"))     out->method = GET;
    else if (!strcmp(method, "POST"))    out->method = POST;
    else if (!strcmp(method, "PUT"))     out->method = PUT;
    else if (!strcmp(method, "DELETE"))  out->method = DELETE;
    else if (!strcmp(method, "OPTIONS")) out->method = OPTIONS;
    else {
        free(req);
        return -1;
    }

    out->URI = strdup(uri);

    if (sscanf(ver, "HTTP/%f", &out->version) != 1) {
        free(req);
        return -1;
    }

    //parsing headers
    out->header = headers_create();
    if (!out->header) {
        free(req);
        return -1;
    }

    while ((line = strtok_r(NULL, "\r\n", &save_line))) {
        if (*line == '\0') continue; 

        char *colon = strchr(line, ':');
        if (!colon) continue;

        *colon = '\0';
        char *name  = line;
        char *value = colon + 1;

        while (*value == ' ')
            value++;

        headers_add(out->header, name, value);
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
  if (req->header && req->header->size > 0) {
    for (size_t i = 0; i < req->header->size; i++) {
      printf("%s: %s\n", req->header->headers[i].name,
             req->header->headers[i].value);
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

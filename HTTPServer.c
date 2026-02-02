#include "HTTPServer.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "Helper.h"
#include "Server.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

HTTPHandler *route_match_handler(HTTPServer *server, HTTPRequest *req,
                                 char **param_out) {
  *param_out = NULL;
  for (size_t i = 0; i < server->handlers_count; i++) {
    HTTPHandler *h = &server->handlers[i];
    switch (h->route_type) {
    case ROUTE_EXACT:
      if (strncmp(req->URI, h->route, req->URI_len) == 0)
        return h;
      break;
    case ROUTE_PREFIX:
      // route ends with /*, strip last 2 chars
      if (strncmp(req->URI, h->route, strlen(h->route) - 2) == 0)
        return h;
      break;
    case ROUTE_PARAM: {
      size_t prefix_len = 0;
      while (h->route[prefix_len] && h->route[prefix_len] != ':')
        prefix_len++;
      if (strncmp(req->URI, h->route, prefix_len) == 0) {
        size_t param_len = req->URI_len - prefix_len;
        *param_out = malloc(param_len + 1);
        memcpy(*param_out, req->URI + prefix_len, param_len);
        (*param_out)[param_len] = '\0';
        return h;
      }
      break;
    }
    }
  }
  return NULL;
}

void on_client(int client_fd, void *context) {
  set_read_timeout(client_fd, 3);
  HTTPRequest req;
  req.headers = NULL;
  size_t buffersize = 1000;
  char *buffer = malloc(buffersize);
  ssize_t buffer_read = 0;

  int final_headers_size = 0;

  if (!buffer) {
    close(client_fd);
    return; // sanity check
  }

  while (1) {

    if (buffersize <= buffer_read) {
      buffersize += 1;
      char *temp = realloc(buffer, buffersize);
      if (!temp) {
        break;
      }
      buffer = temp;
    }

    ssize_t n =
        read(client_fd, buffer + buffer_read, buffersize - (buffer_read));
    buffer_read += n;

    // http close signal
    if (n <= 0) {
      break;
    }

    if (!final_headers_size) {
      int header_len = parse_headers(buffer, buffersize, &req);

      // error parsing
      if (header_len == -1) {
        break;
      };

      // 2 means headers not complete
      if (header_len == -2) {
        if (buffersize <= buffer_read) {
          buffersize += 3;
          char *temp = realloc(buffer, buffersize);
          if (!temp) {
            break;
          }
          buffer = temp;
        }
        continue;
      }

      parse_queries(&req);
      final_headers_size = header_len;
    }
    int int_content_length = 0;
    // only done after sucessfully all headers are parsed
    char *content_length = get_header(req.headers, "content-length");
    if (content_length != NULL) {
      int_content_length = strtol(content_length, NULL, 10);
    }

    if (buffer_read < final_headers_size + int_content_length) {
      if (buffersize <= buffer_read) {
        buffersize += 1;
        char *temp = realloc(buffer, buffersize);
        if (!temp) {
          break;
        }
        buffer = temp;
      }
      continue;
    }

    req.body = &buffer[final_headers_size];// make sure that buffer is not reallocated again when req.body is still accessed
    req.body_len = buffer_read - final_headers_size;
    HTTPServer *http_server = context; // refrensing ourself (kinda like this in
                                       // obj oriented language)
    HTTPResponseWriter res = make_http_response_writer(client_fd);

    char *connection_header = NULL;
    char *h = get_header(req.headers, "connection");
    if (h) {
      connection_header = strdup(h);
    };
    if (connection_header) {
      str_to_lower(connection_header);
      if (strcmp(connection_header, "keep-alive") == 0) {
        res.write_header(&res, "Connection", "keep-alive");
      };
    }

    char *param = NULL;
    HTTPHandler *matched_handler =
        route_match_handler(http_server, &req, &param);

    if (matched_handler) {
      req.param = param;
      matched_handler->handler_func(&req, &res);
    } else {
      res.write_status_code(&res, 404);
      res.write_body(&res, "<h1>NOT FOUND</h1>");
    }

    if (connection_header) {
      fflush(0);

      str_to_lower(connection_header);
      if (strcmp(connection_header, "keep-alive") == 0) {
        buffer_read = 0;
        final_headers_size = 0;

        headers_free(req.headers);
        req.headers = NULL;

        free(req.URI);
        req.URI = NULL;

        queries_free(req.queries, req.query_count);
        req.query_count = 0;

        free(req.param);
        req.param = NULL;

        continue;
      };
    }

    headers_free(req.headers);
    req.headers = NULL;

    free(req.URI);
    req.URI = NULL;

    queries_free(req.queries, req.query_count);
    req.query_count = 0;

    free(req.param);
    req.param = NULL;

    break;
  }

  fflush(0);

  free(buffer);
  close(client_fd);
}

void http_listen_and_server(HTTPServer *http_server) {
  struct Server server =
      server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, http_server->port,
                         1024, &on_client, http_server);
  server_loop(&server);
}

HTTPServer http_server_constructor(uint16_t port) {
  HTTPServer http_server;
  http_server.handlers_count = 0;
  http_server.handlers_capacity = 1;
  http_server.port = port;

  http_server.handlers = malloc(sizeof(HTTPHandler) * 1);
  if (http_server.handlers == NULL) {
    perror("Failed to allocate memery to a handler");
    exit(1);
  }
  return http_server;
}

int add_handler(HTTPServer *http_server, HTTPHandler handler) {
  if (http_server->handlers_capacity <= http_server->handlers_count) {

    http_server->handlers_capacity = 1 + (http_server->handlers_capacity * 2);
    HTTPHandler *temp =
        realloc(http_server->handlers,
                (http_server->handlers_capacity) * sizeof(HTTPHandler));

    if (temp == NULL) {
      return 1;
    }
    http_server->handlers = temp;
  }

  http_server->handlers[http_server->handlers_count++] = handler;

  return 0;
}

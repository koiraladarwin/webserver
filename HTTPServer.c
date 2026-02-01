#include "HTTPServer.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "Server.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// parses queries from uri
void parse_queries(HTTPRequest *req) {
  req->query_count = 0;
  char* temp_uri = malloc(req->URI_len);  
  memcpy(temp_uri, req->URI, req->URI_len);
  char *query_start_pos = strchr(temp_uri, '?');
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
      // e.g., route "/user/:id"
      size_t prefix_len = 0;
      while (h->route[prefix_len] && h->route[prefix_len] != ':')
        prefix_len++;
      if (strncmp(req->URI, h->route, prefix_len) == 0) {
        *param_out = req->URI + prefix_len;
        return h;
      }
      break;
    }
    }
  }
  return NULL;
}

void on_client(int client_fd, void *context) {
  size_t buffersize = 30000;
  char *buffer = malloc(buffersize);
  memset(buffer, 0, buffersize);
  if (!buffer)
    return; // sanity check

  ssize_t n = read(client_fd, buffer, buffersize);
  if (n <= 0) {
    free(buffer);
    close(client_fd);
    return;
  }
  HTTPRequest req;
  parse_headers(buffer, buffersize, &req);
  parse_queries(&req);
  HTTPServer *http_server = context;
  HTTPResponseWriter res = make_http_response_writer(client_fd);

  char *param = NULL;
  HTTPHandler *matched_handler = route_match_handler(http_server, &req, &param);

  if (matched_handler) {
    req.param = param;
    matched_handler->handler_func(&req, &res);
  } else {
    res.write_status_code(&res, 404);
    res.write_body(&res, "<h1>NOT FOUND</h1>");
  }

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

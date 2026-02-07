#ifndef HTTPServer_h
#define HTTPServer_h

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include <stdint.h>

static int client_count = 0;
static int free_client_count = 0;

typedef enum { ROUTE_EXACT, ROUTE_PREFIX, ROUTE_PARAM } RouteType;

typedef struct {
  const char *route;
  RouteType route_type;
  void (*handler_func)(HTTPRequest *req, HTTPResponseWriter *res);
} HTTPHandler;

typedef struct {
  uint16_t port;
  HTTPHandler *handlers;
  size_t handlers_count;
  size_t handlers_capacity;
} HTTPServer;

HTTPServer http_server_constructor(uint16_t port);
void http_listen_and_server(HTTPServer *http_server);
int add_handler(HTTPServer *http_server, HTTPHandler handler);
HTTPHandler *route_match_handler(HTTPServer *server, HTTPRequest *req,
                                 char **param_out);
#endif

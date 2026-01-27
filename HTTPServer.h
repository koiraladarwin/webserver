#ifndef HTTPServer_h
#define HTTPServer_h

#include "HTTPRequest.h"
#include <stdint.h>

typedef struct ResponseWriter{
  int (*write_status_code)(int);
  int (*write_status_message)(char *);
  int (*write_message_type)(char *);
  int (*write_body)(char *);
}ResponseWriter;

typedef struct{
  const char* route;
  void (*handler_func)(HTTPRequest* req,ResponseWriter* res);
}HTTPHandler;

typedef struct  {
  uint16_t port;
  HTTPHandler* handlers;
  size_t handlers_count;
  size_t handlers_capacity;
}HTTPServer;

HTTPServer http_server_constructor(uint16_t port);
void http_listen_and_server(HTTPServer* http_server);
int add_handler(HTTPServer* http_server,HTTPHandler handler);
#endif

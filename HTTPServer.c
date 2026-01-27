#include "HTTPServer.h"
#include "HTTPRequest.h"
#include "Server.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void on_client(struct Server *server) {
  size_t buffersize = 30000;
  char *buffer = malloc(buffersize);
  while (1) {
    socklen_t addr_len = sizeof(server->address);
    int client_socket_fd = accept(
        server->socket_fd, (struct sockaddr *)&server->address, &addr_len);

    read(client_socket_fd, buffer, buffersize);
    HTTPRequest h;
    ParseRequest(buffer, &h);
    PrintHTTPRequest(&h);
    char *response = "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "\r\n"
                     "{\"name\":\"batman\"}";
    write(client_socket_fd, response, strlen(response));
    close(client_socket_fd);
  }
}

void http_listen_and_server(HTTPServer *http_server) {
  struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY,
                                            80, 10, &on_client, http_server);
  server.on_client(&server);
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

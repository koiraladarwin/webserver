#include "HTTPRequest.h"
#include "Server.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void on_client(int client_fd, void *context) {
  size_t buffersize = 30000;
  char *buffer = malloc(buffersize);

  read(client_fd, buffer, buffersize);
  HTTPRequest h;
  ParseRequest(buffer, &h);
  printf("%s", h.URI);
  char response[4096];
  snprintf(response, sizeof(response),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "\r\n"
           "{\"uri\":\"%s\"}",
           h.URI);

  write(client_fd, response, strlen(response));
  close(client_fd);
}

int main() {
  int context = 1;
  struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY,
                                            80, 10, on_client, &context);
  server_loop(&server);
}

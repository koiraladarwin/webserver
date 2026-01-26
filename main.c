#include "Server.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void launch(struct Server *server) {
  char buffer[300000];
  while(1){
  printf("========Waiting for Connection========]\n");
  int addr_len = sizeof(server->address);
  int client_socket_fd = accept(server->socket_fd, (struct sockaddr *)&server->address,
                          (socklen_t *)&addr_len);

  read(client_socket_fd, buffer, sizeof(buffer));
  printf("%s\n", buffer);
  char *response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Content-Length: 48\r\n"
                   "\r\n"
                   "<html><body><h1>Hello, World!</h1></body></html>";
  write(client_socket_fd, response, strlen(response));
  close(client_socket_fd);
  }
}
int main() {
  struct Server server =
      server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, 80, 10, &launch);
  server.launch(&server);
}

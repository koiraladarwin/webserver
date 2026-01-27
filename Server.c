#include "Server.h"
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct Server server_constructor(int domain, int service, int protocol,
                                 u_long interface, int port, int backlog,
                                 ClientCallback on_client, void *context) {

  struct Server server;
  server.domain = domain;
  server.service = service;
  server.protocol = protocol;
  server.interface = interface;
  server.port = port;
  server.backlog = backlog;

  server.address.sin_family = domain;
  server.address.sin_port = htons(port);
  server.address.sin_addr.s_addr = htons(server.interface);

  server.socket_fd = socket(server.domain, server.service, server.protocol);
  if (server.socket_fd == 0) {
    perror("Failed to connect to a server");
    exit(0);
  }

  int opt = 1;
  setsockopt(server.socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  int socket_length = sizeof(server.address);
  int ret = bind(server.socket_fd, (struct sockaddr *)&server.address,
                 (socklen_t)socket_length);

  if (ret < 0) {
    perror("failed to bind socket");
    exit(1);
  }

  ret = listen(server.socket_fd, server.backlog);
  if (ret < 0) {
    perror("failed to listen to socket");
    exit(1);
  }

  server.on_client = on_client;
  server.context = context;

  return server;
}

typedef struct {
  int client_fd;
  struct Server *server;
} ThreadArg;

void *client_thread(void *args) {
  ThreadArg *targ = (ThreadArg *)args;
  targ->server->on_client(targ->client_fd, targ->server->context);
  free(targ);
  return NULL;
}

void server_loop(struct Server *server) {
  while (1) {
    socklen_t addr_len = sizeof(server->address);
    int client_fd = accept(server->socket_fd,
                           (struct sockaddr *)&server->address, &addr_len);
    if (client_fd < 0)
      continue;

    ThreadArg *targ = malloc(sizeof(ThreadArg));
    targ->client_fd = client_fd;
    targ->server = server;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, client_thread, targ);
  }
}

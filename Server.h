#ifndef Server_h
#define Server_h

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef void (*ClientCallback)(int client_fd, void *context);

struct Server {
  int domain;
  int service;
  int protocol;
  u_long interface;
  int port;
  int backlog;

  struct sockaddr_in address;

  ClientCallback on_client;
  void *context;

  int socket_fd;
};

struct Server server_constructor(int domain, int service, int protocol,
                                 u_long interface, int port, int backlog,
                                 ClientCallback on_client, void *context);

void server_loop(struct Server *server);

#endif

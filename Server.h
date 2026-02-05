#ifndef Server_h
#define Server_h

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef struct {
  int fd;

  char *req_buffer;
  size_t req_buffer_capacity;
  size_t req_buffer_read;

  size_t final_headers_size;

} Client;
Client* client_constructor(int fd);

typedef void (*ClientCallback)(int epoll_fd,void *context);

struct Server {
  int domain;
  int service;
  int protocol;
  u_long interface;
  int port;
  int backlog;

  struct sockaddr_in address;

  ClientCallback on_clients;
  void *context;

  int socket_fd;
};
void set_read_timeout(int sockfd, int seconds);
struct Server server_constructor(int domain, int service, int protocol,
                                 u_long interface, int port, int backlog,
                                 ClientCallback on_client, void *context);

void server_loop(struct Server *server);

#endif

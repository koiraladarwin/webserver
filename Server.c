#include "Server.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HTTPServer.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

Client *client_head = NULL;
void remove_client(Client *c) {
  Client **cur = &client_head;
  while (*cur) {
    if (*cur == c) {
      *cur = (Client *)c->next; // unlink
      break;
    }
    cur = (Client **)&(*cur)->next;
  }
}

Client *client_constructor(int fd) {
  Client *client = malloc(sizeof(Client));
  if (!client)
    return NULL;

  client->fd = fd;
  client->req_buffer_capacity = 1024;
  client->req_buffer_read = 0;
  client->final_headers_size = 0;
  client->req_buffer = malloc(client->req_buffer_capacity);
  client->mode = 1;

  client->res = make_http_response_writer(fd);
  client->req = malloc(sizeof(HTTPRequest));
  client->last_activity = time(NULL);
  client->closed = 0;
  return client;
}

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

  server.on_clients = on_client;
  server.context = context;

  return server;
}

void set_read_timeout(int sockfd, int seconds) {
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror("setsockopt");
  }
}
void make_socket_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

typedef struct {
  int epoll_fd;
  void *context;
  void (*on_clients)(int, void *);
} EpollThreadCtx;

void *epoll_thread_main(void *arg) {
  EpollThreadCtx *ctx = arg;

  ctx->on_clients(ctx->epoll_fd, ctx->context);
  free(ctx);
  return NULL;
}

void server_loop(struct Server *server) {
  int epoll_fd = epoll_create1(0);

  pthread_t epoll_thread;
  EpollThreadCtx *ctx = malloc(sizeof *ctx);
  ctx->epoll_fd = epoll_fd;
  ctx->context = server->context;
  ctx->on_clients = server->on_clients;

  pthread_create(&epoll_thread, NULL, epoll_thread_main, ctx);
  while (1) {
    socklen_t addr_len = sizeof(server->address);
    int client_fd = accept(server->socket_fd,
                           (struct sockaddr *)&server->address, &addr_len);


    if (client_fd < 0) {
      perror("accept");
      exit(1);
    }
    int opt = 1;
    int idle = 75;
    int interval = 5;
    int probes = 3;
    setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval,
               sizeof(interval));
    setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes));

    make_socket_nonblocking(client_fd);

    Client *client = client_constructor(client_fd);

    client->next = client_head;
    client_head = client;

    if (!client)
      continue;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ev.data.ptr = client;
    printf("client added\n");
    fflush(0);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
  }
}

#include "HTTPServer.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "Helper.h"
#include "Server.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENTS 10

struct epoll_event events[MAX_EVENTS];

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

void on_clients(int epoll_fd, void *context) {
  while (1) {

    int n_event = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    if (n_event == -1) {
      if (errno == EINTR)
        continue; // signal interrupted
      perror("epoll_wait");
      break;
    }

    for (int i = 0; i < n_event; i++) {

      Client *client = events[i].data.ptr;

      if (!client->req_buffer) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);
        return;
      }

      if (client->mode == 2) {
        goto write_mode;
      }

      if (client->req_buffer_capacity <= client->req_buffer_read) {
        client->req_buffer_capacity *= 2;
        char *temp = realloc(client->req_buffer, client->req_buffer_capacity);

        if (!temp) {
          break; // break main loop
        }
        client->req_buffer = temp;
      }

      ssize_t n = read(client->fd, client->req_buffer + client->req_buffer_read,
                       client->req_buffer_capacity - (client->req_buffer_read));

      // no data in the buffer
      if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        continue;
      }

      // http close signal
      if (n <= 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);

        goto free_memory;
      }

      client->req_buffer_read += n;

      // headers not parsed completely
      if (!client->final_headers_size) {
        int header_len = parse_headers(
            client->req_buffer, client->req_buffer_capacity, client->req);

        // error parsing
        if (header_len == -1) {
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
          close(client->fd);
          goto free_memory;
        };

        // 2 means headers not complete
        if (header_len == -2) {
          if (client->req_buffer_capacity <= client->req_buffer_read) {
            client->req_buffer_capacity += 3;
            char *temp =
                realloc(client->req_buffer, client->req_buffer_capacity);
            if (!temp) {
              break;
            }
            client->req_buffer = temp;
          }
          continue; // read more header complete
        }

        parse_queries(client->req);
        client->final_headers_size = header_len;
      }

      if (client->final_headers_size <= 0)
        continue;
      // only done after sucessfully all headers are parsed
      int int_content_length = 0;
      printf("1 get header\n");
      printf("headers prt%p\n", client->req->headers);
      fflush(0);
      char *content_length = get_header(client->req->headers, "content-length");
      if (content_length) {
        int_content_length = strtol(content_length, NULL, 10);
      }
      free(content_length);
      if (client->req_buffer_read <
          client->final_headers_size + int_content_length) {
        if (client->req_buffer_capacity <= client->req_buffer_read) {
          client->req_buffer_capacity *= 2;
          char *temp = realloc(client->req_buffer, client->req_buffer_capacity);
          if (!temp) {
            break;
          }
          client->req_buffer = temp;
        }
        continue; // read more (body incomplete)
      }
      //--------------------------------------------------------------------------------------------
      // this part is not i/o blocked
      client->req->body = &client->req_buffer[client->final_headers_size];
      client->req->body_len =
          client->req_buffer_read - client->final_headers_size;
      HTTPServer *http_server = context;

      char *a;

      printf("2 get header\n");
      printf("headers prt%p\n", client->req->headers);
      fflush(0);

      char *h = get_header(client->req->headers, "connection");

      if (h) {
        a = strdup(h);
      };
      if (a) {
        str_to_lower(a);
        if (strcmp(a, "keep-alive") == 0) {
          client->res->write_header(client->res, "Connection", "keep-alive");
        };

        free(a);
      }
      printf("headers prt 2 %p\n", client->req->headers);
      fflush(0);

      char *param = NULL;
      HTTPHandler *matched_handler =
          route_match_handler(http_server, client->req, &param);

      if (matched_handler) {
        printf("headers prt 3%p\n", client->req->headers);
        fflush(0);
        client->req->param = param;
        matched_handler->handler_func(client->req, client->res);
      } else {
        client->res->write_status_code(client->res, 404);
        char body[] = "<h1>NOT FOUND</h1>";

        printf("headers prt 3%p\n", client->req->headers);
        fflush(0);

        rw_write_body(client->res, body, strlen(body));

        printf("headers prt 4%p\n", client->req->headers);
        fflush(0);
      }

      printf("headers prt 5%p\n", client->req->headers);
      fflush(0);
      struct epoll_event oev = {0};
      oev.events = EPOLLOUT;
      oev.data.ptr = events[i].data.ptr;

      epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &oev);
      client->mode = 2; // write mode

      continue;
      //--------------------------------------------------------------------------------------------
      // here we need to write all the writer stuff to the client_fd with non
      // blocking epoll wait (we do this later not now)
    write_mode:
      int nwrite = rw_flush(client->res);
      if (nwrite == -2 || nwrite == 2) {
        continue;
      }

      if (nwrite == -1) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);
        goto free_memory;
      }

      if (nwrite == 0) {
      }
      char *c = NULL;
      printf("3 get header\n");
      printf("headers prt%p\n", client->req->headers);
      fflush(0);
      char *b = get_header(client->req->headers, "connection");
      if (b) {
        c = strdup(b);
      };
      if (c) {
        str_to_lower(c);
        if (strcmp(c, "keep-alive") == 0) {

          client->res->res_buffer_written = 0;
          client->res->res_buffer_size = 0;
          client->req->query_count = 0;
          client->req->headers->size = 0;
          client->res->mode = 1;
          struct epoll_event iev = {0};
          oev.events = EPOLLIN;
          oev.data.ptr = events[i].data.ptr;

          epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &iev);
          client->mode = 1; // read mode

          continue; // if keep alive reset all and read again from header
        };
      }
    free_memory:
      printf("------------------------\nfreeing\n-------------------\n");
      fflush(0);
      client->res->res_buffer_written = 0;
      client->res->res_buffer_size = 0;

      free(client->res->res_buffer);
      // headers_free(client->req->headers);

      client->req->URI = NULL;
      free(client->req->URI);

      client->req->query_count = 0;
      queries_free(client->req->queries, client->req->query_count);

      client->req->param = NULL;
      free(client->req->param);

      // free(client->req_buffer);
      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
      close(client->fd);
    }
  }
}

void http_listen_and_server(HTTPServer *http_server) {
  struct Server server =
      server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, http_server->port,
                         1024, &on_clients, http_server);
  server_loop(&server);
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
      size_t prefix_len = 0;
      while (h->route[prefix_len] && h->route[prefix_len] != ':')
        prefix_len++;
      if (strncmp(req->URI, h->route, prefix_len) == 0) {
        size_t param_len = req->URI_len - prefix_len;
        *param_out = malloc(param_len + 1);
        memcpy(*param_out, req->URI + prefix_len, param_len);
        (*param_out)[param_len] = '\0';
        return h;
      }
      break;
    }
    }
  }
  return NULL;
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

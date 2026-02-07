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
#include <time.h>
#include <unistd.h>

#define MAX_EVENTS 100

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
    time_t now = time(NULL);
    int n_event = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);

    if (n_event == -1) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait");
      break;
    }

    for (Client *client = client_head; client;) {
      Client *next = client->next;

      if (!client->closed && now - client->last_activity > 3) {
        client->closed = 1;
        free_client_count++;
        free(client->res->headers);
        free(client->req_buffer);
        client->res->res_buffer_written = 0;
        client->res->res_buffer_size = 0;

        free(client->res->res_buffer);
        headers_free(client->req->headers);

        client->req->URI = NULL;
        free(client->req->URI);

        client->req->query_count = 0;
        queries_free(client->req->queries, client->req->query_count);

        client->req->param = NULL;
        free(client->req->param);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);
        remove_client(client);
        free(client);
      }
      client = next;
    }
    for (int i = 0; i < n_event; i++) {
      Client *client = events[i].data.ptr;
      if (!client)
        continue;
      // because linux can queue 2 event at once
      if (client->closed) {
        continue;
      }

      if (events[i].events & (EPOLLHUP | EPOLLERR)) {
        goto free_memory;
      }

      if (now - client->last_activity > 10) {
        goto free_memory;
      }

      if (!(events[i].events & EPOLLOUT) && !(events[i].events & EPOLLIN)) {
        goto free_memory;
      }

      if (client->mode == 2 && events[i].events & EPOLLOUT) {
        client->last_activity = now;
        goto write_mode;
      }

      client->last_activity = now;
      if (client->req_buffer_capacity <= client->req_buffer_read) {
        printf("req buffer realloced\n");
        fflush(0);
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
        goto free_memory;
      }

      client->req_buffer_read += n;

      // headers not parsed completely
      if (!client->final_headers_size) {
        int header_len = parse_headers(
            client->req_buffer, client->req_buffer_capacity, client->req);

        // error parsing
        if (header_len == -1) {
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

      char *a = NULL;

      char *h = get_header(client->req->headers, "connection");
      if (h) {
        a = strdup(h);
      };

      if (a) {
        str_to_lower(a);
        if (strcmp(a, "keep-alive") == 0) {
          char connection[] = "Connection";
          char keep_alive[] = "keep-alive";
          client->res->write_header(client->res, connection, keep_alive);
        };

        free(a);
      }

      char *param = NULL;
      HTTPHandler *matched_handler =
          route_match_handler(http_server, client->req, &param);

      if (matched_handler) {
        client->req->param = param;
        matched_handler->handler_func(client->req, client->res);
      } else {
        client->res->write_status_code(client->res, 404);
        char body[] = "<h1>NOT FOUND</h1>";

        rw_write_body(client->res, body, strlen(body));
      }

      struct epoll_event oev = {0};
      oev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
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
        goto free_memory;
      }

      if (nwrite == 0) {
      }
      char *c = NULL;
      char *b = get_header(client->req->headers, "connection");
      if (b) {
        c = strdup(b);
      };
      if (c) {
        str_to_lower(c);
        if (strcmp(c, "keep-alive") == 0) {
          client->res->res_buffer_written = 0;
          client->res->res_buffer_size = 0;
          client->res->headers_size = 0;
          client->res->mode = 1;

          client->req->query_count = 0;
          client->req->headers->size = 0;
          client->req->body_len = 0;
          client->req_buffer_read = 0;
          client->final_headers_size = 0;

          free(client->req->URI);
          free(client->req->param);
          headers_free(client->req->headers);
          struct epoll_event iev = {0};

          iev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
          iev.data.ptr = events[i].data.ptr;
          epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &iev);
          client->mode = 1; // read mode
          continue; // if keep alive reset all and read again from header
        };
      }
    free_memory:
      client->closed = 1;
      free_client_count++;
      free(client->res->headers);
      free(client->req_buffer);
      client->res->res_buffer_written = 0;
      client->res->res_buffer_size = 0;

      free(client->res->res_buffer);
      headers_free(client->req->headers);

      client->req->URI = NULL;
      free(client->req->URI);

      client->req->query_count = 0;
      queries_free(client->req->queries, client->req->query_count);

      client->req->param = NULL;
      free(client->req->param);
      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
      close(client->fd);
      remove_client(client);
      free(client);
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

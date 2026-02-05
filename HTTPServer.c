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
      printf("got here");
      fflush(0);
      // refrence the client ptr
      Client *client = events[i].data.ptr;
      if (!client->req_buffer) {

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);
        return;
      }

      HTTPRequest req;
      req.headers = NULL;

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

      // http close signal(check for if this close or no data)
      if (n <= 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);
        continue;
      }

      client->req_buffer_read += n;

      // headers not parsed completely
      if (!client->final_headers_size) {
        int header_len = parse_headers(client->req_buffer,
                                       client->req_buffer_capacity, &req);

        // error parsing
        if (header_len == -1) {
          break;
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

        parse_queries(&req);
        client->final_headers_size = header_len;
      }

      if (client->final_headers_size <= 0)
        continue;

      // only done after sucessfully all headers are parsed
      int int_content_length = 0;
      char *content_length = get_header(req.headers, "content-length");
      if (content_length) {
        int_content_length = strtol(content_length, NULL, 10);
      }

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
      req.body = &client->req_buffer[client->final_headers_size];
      req.body_len = client->req_buffer_read - client->final_headers_size;
      HTTPServer *http_server = context;

      // this is blocking for now (make this flush at end so its not blocking)
      HTTPResponseWriter res = make_http_response_writer(client->fd);

      char *connection_header = NULL;
      char *h = get_header(req.headers, "connection");
      if (h) {
        connection_header = strdup(h);
      };
      if (connection_header) {
        str_to_lower(connection_header);
        if (strcmp(connection_header, "keep-alive") == 0) {
          res.write_header(&res, "Connection", "keep-alive");
        };
      }

      char *param = NULL;
      HTTPHandler *matched_handler =
          route_match_handler(http_server, &req, &param);

      if (matched_handler) {
        req.param = param;
        matched_handler->handler_func(&req, &res);
      } else {
        res.write_status_code(&res, 404);
        res.write_body(&res, "<h1>NOT FOUND</h1>");
      }
      //--------------------------------------------------------------------------------------------

      // here we need to write all the writer stuff to the client_fd with non
      // blocking epoll wait (we do this later not now)

      if (connection_header) {
        str_to_lower(connection_header);
        if (strcmp(connection_header, "keep-alive") == 0) {
          client->req_buffer_read = 0;
          client->final_headers_size = 0;

          headers_free(req.headers);
          req.headers = NULL;

          free(req.URI);
          req.URI = NULL;

          queries_free(req.queries, req.query_count);
          req.query_count = 0;

          free(req.param);
          req.param = NULL;
          continue; // if keep alive reset all and read again from header
        };
      }
      // if not keep-alive then this is triggered after one cycle
      headers_free(req.headers);
      req.headers = NULL;

      free(req.URI);
      req.URI = NULL;

      queries_free(req.queries, req.query_count);
      req.query_count = 0;

      free(req.param);
      req.param = NULL;

      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
      free(client->req_buffer);
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

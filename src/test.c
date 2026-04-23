#include "../includes/HTTPRequest.h"
#include "../includes/HTTPResponse.h"
#include "../includes/HTTPServer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080

void home_handler(HTTPRequest *req, HTTPResponseWriter *res) {
  res->write_status_code(res, 200);
  char body[]="<h1>hello<h1>";
  res->write_body(res, body, strlen(body));
}

void batman_handler(HTTPRequest *req, HTTPResponseWriter *res) {

    const char *body;
    size_t body_len;

    if (req->body && req->body_len > 0) {
        body = req->body;
        body_len = req->body_len;
    } else {
        body = "no body";
        body_len = 7;
    }

    const char prefix[] = "<h1>";
    const char suffix[] = "</h1>";

    size_t total = (sizeof(prefix)-1) + body_len + (sizeof(suffix)-1);

    char *response = malloc(total);
    if (!response) return;

    size_t offset = 0;

    memcpy(response + offset, prefix, sizeof(prefix)-1);
    offset += sizeof(prefix)-1;

    memcpy(response + offset, body, body_len);
    offset += body_len;

    memcpy(response + offset, suffix, sizeof(suffix)-1);

    res->write_status_code(res, 200);
    res->write_header(res, "Content-Type", "text/html");
    res->write_body(res, response, total);

    free(response);
}

void test_handler(HTTPRequest *req, HTTPResponseWriter *res) {
    char buf[1024];
    size_t offset = 0;

    for (size_t i = 0; i < req->query_count; i++) {
        char *key = req->queries[i].key;
        char *value = req->queries[i].value ? req->queries[i].value : "";
        offset += snprintf(buf + offset, sizeof(buf) - offset, "%s=%s\n", key, value);
        if (offset >= sizeof(buf))
            break;
    }

    if (req->param)
        offset += snprintf(buf + offset, sizeof(buf) - offset, "params=%s", req->param);

    res->write_header(res, "Content-Type", "text/html");
    res->write_status_code(res, 200);
    res->write_body(res, buf, offset);
}

int main() {
  printf("listening on port %d",PORT);
  fflush(stdout);
  signal(SIGPIPE, SIG_IGN); // important -- this prevents our server from
                            // exiting when wrtting to close socket
  HTTPServer http_server = http_server_constructor(PORT);

  add_handler(&http_server, (HTTPHandler){"/home", ROUTE_EXACT, home_handler});
  add_handler(&http_server,
              (HTTPHandler){"/batman", ROUTE_EXACT, batman_handler});
  add_handler(&http_server,
              (HTTPHandler){"/test/:id", ROUTE_PARAM, test_handler});

  http_listen_and_server(&http_server);
}



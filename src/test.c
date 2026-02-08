#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HTTPServer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void home_handler(HTTPRequest *req, HTTPResponseWriter *res) {
  res->write_status_code(res, 200);
  char body[]="<h1>hello<h1>";
  res->write_body(res, body, strlen(body));
}

void batman_handler(HTTPRequest *req, HTTPResponseWriter *res) {
  const char *body = req->body ? req->body : "no body";

  size_t needed = snprintf(NULL, 0, "<h1>%s</h1>", body) + 1;
  char response[needed];

  snprintf(response, needed, "<h1>%s</h1>", body);

  res->write_status_code(res, 200);
  res->write_header(res, "Content-Type", "text/html");
  res->write_body(res, response, needed);
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
  signal(SIGPIPE, SIG_IGN); // important -- this prevents our server from
                            // exiting when wrtting to close socket
  HTTPServer http_server = http_server_constructor(8080);

  add_handler(&http_server, (HTTPHandler){"/home", ROUTE_EXACT, home_handler});
  add_handler(&http_server,
              (HTTPHandler){"/batman", ROUTE_EXACT, batman_handler});
  add_handler(&http_server,
              (HTTPHandler){"/test/:id", ROUTE_PARAM, test_handler});

  http_listen_and_server(&http_server);
}

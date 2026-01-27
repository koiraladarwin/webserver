#include "HTTPResponse.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int rw_write_status_code(HTTPResponseWriter* res, int code) {
    char status_line[64];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d OK\r\n", code);
    return write(res->client_fd, status_line, strlen(status_line));
}

int rw_write_header(HTTPResponseWriter* res, const char* key, const char* value) {
    int n = snprintf(res->headers + strlen(res->headers),
                     sizeof(res->headers) - strlen(res->headers),
                     "%s: %s\r\n", key, value);
    return n;
}

int rw_write_body(HTTPResponseWriter* res, const char* body) {
    // end headers with empty line
    strcat(res->headers, "\r\n");

    write(res->client_fd, res->headers, strlen(res->headers));
    return write(res->client_fd, body, strlen(body));
}

HTTPResponseWriter make_http_response_writer(int client_fd) {
    HTTPResponseWriter res;
    res.client_fd = client_fd;
    res.headers[0] = '\0';
    res.write_status_code = rw_write_status_code;
    res.write_header = rw_write_header;
    res.write_body = rw_write_body;
    return res;
}

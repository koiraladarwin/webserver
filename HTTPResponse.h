#ifndef HTTPResponse_h
#define HTTPResponse_h

#include <stddef.h>

// Forward declaration
typedef struct HTTPResponseWriter HTTPResponseWriter;

struct HTTPResponseWriter {
    int client_fd;

    char *res_buffer;
    size_t res_buffer_size;
    size_t res_buffer_capacity;
    size_t res_buffer_written;

    int mode;
    int (*write_status_code)(HTTPResponseWriter*, int code);
    int (*write_header)(HTTPResponseWriter*, const char* key, const char* value);
    int (*write_body)(HTTPResponseWriter*, const char* body,size_t body_len);

    char headers[1024];
    char status_line[90];
};

HTTPResponseWriter* make_http_response_writer(int client_fd);

int rw_write_status_code(HTTPResponseWriter* res, int code);
int rw_write_header(HTTPResponseWriter* res, const char* key, const char* value);
int rw_write_body(HTTPResponseWriter* res, const char* body,size_t body_len);
int rw_flush(HTTPResponseWriter *res);

#endif


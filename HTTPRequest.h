#ifndef HTTPRequest_h
#define HTTPRequest_h
#include <stddef.h>

#define MAX_QUERY_PARAMS 16

enum HTTPMethods { GET, POST, OPTIONS, PUT, DELETE };
typedef struct {
  char *name;
  char *value;
} HTTPHeader;

typedef struct {
  char *key;
  char *value;
} Query;

typedef struct {
  HTTPHeader *header;
  size_t size;
  size_t capacity;
} HTTPHeaders;

typedef struct {
  int method;
  char *URI;
  size_t URI_len;
  float version;
  HTTPHeaders *headers;
  char *body;
  size_t body_len;
  char *param;
  Query queries[MAX_QUERY_PARAMS];
  size_t query_count;
} HTTPRequest;

char *get_header(HTTPHeaders *h, char *key);

__attribute__((deprecated("Use parse_headers() instead")))
int ParseRequest(char *request, HTTPRequest *out);

int parse_headers(char *req, size_t req_len, HTTPRequest *out);
void parse_queries(HTTPRequest *req);

void PrintHTTPRequest(const HTTPRequest *req);
#endif

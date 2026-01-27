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
  HTTPHeader *headers;
  size_t size;
  size_t capacity;
} HTTPHeaders;

typedef struct {
  int method;
  char *URI;
  float version;
  HTTPHeaders *header;
  char *body;
  char *param;
  Query queries[MAX_QUERY_PARAMS];
  size_t query_count;
} HTTPRequest;

char *get_header(HTTPHeaders *h, char *key);

int ParseRequest(char *request, HTTPRequest *out);
void PrintHTTPRequest(const HTTPRequest *req);
#endif

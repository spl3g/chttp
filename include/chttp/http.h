#ifndef HTTP_H_
#define HTTP_H_

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#include "const_strings.h"
#include "arena_strings.h"
#include "arena.h"

typedef struct {
  const_string key;
  const_string value;
} KV;

typedef struct {
  KV *data;
  size_t len;
  size_t cap;
} header_da;

typedef struct {
  const_string key;
  const_string val;
} http_query;

typedef struct {
  KV *data;
  size_t len;
  size_t cap;
} http_query_da;

typedef struct {
  const_string key;
  void *val;
} http_context_value;

typedef struct {
  http_context_value *data;
  size_t len;
  size_t cap;
} http_context;

typedef struct {
  size_t code;
  header_da headers;
  const_string body;
  bool sent;
} http_response;

typedef struct {
  size_t inc_fd;
  const_string method;
  const_string path;
  const_string version;
  http_query_da query;
  header_da headers;
  const_string body;

  arena *arena;
  http_response* resp;
  http_context *ctx;
} http_request;

typedef void (*http_handler_func)(http_request);

typedef struct http_handler_da http_handler_da;
typedef struct http_middleware http_middleware;
typedef struct http_handler http_handler; 

typedef void (*http_middleware_func)(http_middleware *, http_request);

struct http_middleware {
  http_middleware_func func;
  http_middleware *next;
  http_handler_func handler;
};

struct http_handler {
  const_string method;
  const_string path;
  http_middleware *middleware;
  http_handler_func func;
};

struct http_handler_da {
  http_handler *data;
  size_t len;
  size_t cap;
};

typedef struct {
  http_middleware *start;
  http_middleware *end;
} http_global_middleware;

typedef struct {
  size_t sockfd;
  struct sockaddr *addr;
  arena *arena;
  http_handler_da handlers;
  http_context global_ctx;
  http_global_middleware *global_middleware;
} http_server;

typedef enum {
  HTTP_INFO = 0,
  HTTP_WARNING,
  HTTP_ERROR,
} http_log_level;

typedef enum {
  CONTINUE = 100,
  SWITCHING_PROTOCOLS = 101,
  PROCESSING = 102,
  EARLY_HINTS = 103,
  OK = 200,
  CREATED = 201,
  ACCEPTED = 202,
  NO_CONTENT = 204,
  MULTIPLE_CHOICES = 300,
  MOVED_PERMANENTLY = 301,
  FOUND = 302,
  SEE_OTHER = 303,
  NOT_MODIFIED = 304,
  TEMPORARY_REDIRECT = 307,
  PERMANENT_REDIRECT = 308,
  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  PAYMENT_REQUIRED = 402,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  METHOD_NOT_ALLOWED = 405,
  NOT_ACCEPTABLE = 406,
  REQUEST_TIMEOUT = 408,
  IM_A_TEEPOT = 418,
  INTERNAL_SERVER_ERROR = 500,
  NOT_IMPLEMENTED = 501,
  BAD_GATEWAY = 502,
  SERVICE_UNAVAILABLE = 503,
  HTTP_VERSION_NOT_SUPPPORTED = 505,
} http_response_code;

const_string get_response_string(http_response_code code);
void *get_in_addr(struct sockaddr *sa);
int get_in_port(struct sockaddr *sa);
void http_log(http_log_level log, char *fmt, ...);

void set_context_value(arena *arena, http_context *ctx, http_context_value kv);
void *get_context_value(http_context *ctx, char* key);

void http_send(http_request req);
void http_send_json(http_request req, const_string json);
void http_send_body(http_request req, const_string body);

int init_server(arena *arena, http_server *serv, char *addr, char *port);
int listen_and_serve(http_server *serv);

http_handler *http_handle_path(http_server *serv, char *method, char *path, http_handler_func handler);

void http_run_next(http_middleware *self, http_request req);
void http_register_global_middleware(http_server *hand, http_middleware_func func);
void http_register_handler_middleware(arena *arena, http_handler *hand, http_middleware_func func);

#endif // HTTP_H_

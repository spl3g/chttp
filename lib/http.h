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
} query;

typedef struct {
  KV *data;
  size_t len;
  size_t cap;
} query_da;

typedef struct {
  const_string key;
  void *val;
} context_value;

typedef struct {
  context_value *data;
  size_t len;
  size_t cap;
} context;

struct request {
  size_t inc_fd;
  const_string method;
  const_string path;
  query_da query;
  header_da headers;
  const_string body;

  arena *arena;
  struct response* resp;
  context *ctx;
};

struct response {
  size_t code;
  header_da headers;
  const_string body;
  bool sent;
};

typedef void (*handler_func)(struct request);

typedef struct handler_da handler_da;
typedef struct http_middleware http_middleware;
typedef struct handler handler; 

typedef void (*http_middleware_func)(http_middleware *, struct request);

struct http_middleware {
  http_middleware_func func;
  http_middleware *next;
  handler_func handler;
};

struct handler {
  const_string method;
  const_string path;
  http_middleware *middleware;
  handler_func func;
};

struct handler_da {
  handler *data;
  size_t len;
  size_t cap;
};

typedef struct {
  http_middleware *start;
  http_middleware *end;
} http_global_middleware;

struct server {
  size_t sockfd;
  struct sockaddr *addr;
  arena *arena;
  handler_da handlers;
  context global_ctx;
  http_global_middleware *global_middleware;
};

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
} response_code;

const_string get_response_string(response_code code);
void *get_in_addr(struct sockaddr *sa);
int get_in_port(struct sockaddr *sa);
void http_log(http_log_level log, char *fmt, ...);

void set_context_value(arena *arena, context *ctx, context_value kv);
void *get_context_value(context *ctx, char* key);

void http_send(struct request req);
void http_send_json(struct request req, const_string json);
void http_send_body(struct request req, const_string body);

int init_server(arena *arena, struct server *serv, char *addr, char *port);
int listen_and_serve(struct server *serv);

handler *http_handle_path(struct server *serv, const_string method, const_string path, handler_func handler);

void http_register_global_middleware(struct server *hand, http_middleware_func func);
void http_register_handler_middleware(arena *arena, handler *hand, http_middleware_func func);

#endif // HTTP_H_

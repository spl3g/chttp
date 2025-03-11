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

#include "const_strings.h"
#include "arena_strings.h"
#include "arena.h"

struct header {
  const_string key;
  const_string value;
};


typedef struct {
  struct header *data;
  size_t len;
  size_t cap;
} header_da;

struct request {
  const_string method;
  const_string path;
  header_da headers;
  const_string body;
};

struct response {
  size_t code;
  const_string body;
  const_string string;
};

typedef void (*handler_func)(struct request, struct response *resp);

struct handler {
  const_string method;
  const_string path;
  handler_func func;
};

typedef struct {
  struct handler *data;
  size_t len;
  size_t cap;
} handler_da;

struct server {
  arena *arena;
  int sockfd;
  struct sockaddr *addr;
  handler_da handlers;
};

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
  IM_A_TEEPOT = 418,
  INTERNAL_SERVER_ERROR = 500,
  NOT_IMPLEMENTED = 501,
  BAD_GATEWAY = 502,
  SERVICE_UNAVAILABLE = 503,
  HTTP_VERSION_NOT_SUPPPORTED = 505,
} response_codes;

const_string get_response_string(int code);
void sigchld_handler();
void *get_in_addr(struct sockaddr *sa);
int get_in_port(struct sockaddr *sa);

int init_server(arena *arena, struct server *serv, char *addr, char *port);
void compose_response(struct response *resp, arena *perm, arena scratch);
void process_request(struct server *serv, ssize_t inc_fd);
int listen_and_serve(struct server *serv);

void handle_path(struct server *serv, const_string method, const_string path, handler_func handler);
void compose_response(struct response *resp, arena *perm, arena scratch);
void process_request(struct server *serv, ssize_t inc_fd);
#endif // HTTP_H_

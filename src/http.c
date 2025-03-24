#include <chttp/http.h>

#define MAX_DATA_SIZE 256
#define BUFFER_SIZE 4096
#define REQUEST_ARENA_SIZE 1024

int volatile keepRunning = 1;

struct {
  http_response_code code;
  const_string string;
} response_strings [] = {
  {100, CS_STATIC("Continue")},
  {101, CS_STATIC("Switching Protocols")},
  {102, CS_STATIC("Processing")},
  {103, CS_STATIC("Early Hints")},
  {200, CS_STATIC("OK")},
  {201, CS_STATIC("Created")},
  {202, CS_STATIC("Accepted")},
  {204, CS_STATIC("No Content")},
  {300, CS_STATIC("Multiple Choices")},
  {301, CS_STATIC("Moved Permanently")},
  {302, CS_STATIC("Found")},
  {303, CS_STATIC("See Other")},
  {304, CS_STATIC("Not Modified")},
  {307, CS_STATIC("Temporary Redirect")},
  {308, CS_STATIC("Permanent Redirect")},
  {400, CS_STATIC("Bad Request")},
  {401, CS_STATIC("Unauthorized")},
  {402, CS_STATIC("Payment Required")},
  {403, CS_STATIC("Forbidden")},
  {404, CS_STATIC("Not Found")},
  {405, CS_STATIC("Method Not Allowed")},
  {406, CS_STATIC("Not Acceptable")},
  {408, CS_STATIC("Request Timeout")},
  {418, CS_STATIC("Im A Teepot")},
  {500, CS_STATIC("Internal Server Error")},
  {501, CS_STATIC("Not Implemented")},
  {502, CS_STATIC("Bad Gateway")},
  {503, CS_STATIC("Service Unavailable")},
  {505, CS_STATIC("Http Version Not Suppported")},
};

const_string get_response_string(http_response_code code) {
  for (int i = 0; sizeof(response_strings)/sizeof(response_strings[0]) ;i++) {
	if (response_strings[i].code == code) {
	  return response_strings[i].string;
	}
  }
  return CS("");
}


void sigchld_handler() {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void http_log(http_log_level log, char *fmt, ...) {
  switch (log) {
  case HTTP_INFO:
	fprintf(stderr, "[INFO] ");
	break;
	
  case HTTP_WARNING:
	fprintf(stderr, "[WARNING] ");
	break;

  case HTTP_ERROR:
	fprintf(stderr, "[ERROR] ");
	break;
  }

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}


void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_in_port(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
	return ntohs((((struct sockaddr_in*)sa)->sin_port));
  }

  return (((struct sockaddr_in6*)sa)->sin6_port);
}

void set_context_value(arena *arena, http_context *ctx, http_context_value kv) {
  for (size_t i = 0; i < ctx->len; i++) {
	http_context_value *ctx_val = (ctx->data + i);
	if (cs_eq(kv.key, ctx_val->key)) {
	  ctx_val->val = kv.val;
	  return;
	}
  }
  arena_da_append(arena, ctx, kv);
}

void *get_context_value(http_context *ctx, char* key) {
  if (ctx == NULL) {
	return NULL;
  }

  const_string key_cs = cs_from_cstr(key);
  for (size_t i = 0; i < ctx->len; i++) {
	http_context_value *ctx_val = (ctx->data + i);
	if (cs_eq(key_cs, ctx_val->key)) {
	  return ctx_val->val;
	}
  }
  return NULL;
}

void http_internal_server_error_handler(http_request req) {
  const_string *error = get_context_value(req.ctx, "error");
  req.resp->code = INTERNAL_SERVER_ERROR;
  req.resp->body = *error;
  http_send(req);
}

void http_not_found_handler(http_request req) {
  req.resp->code = NOT_FOUND;
  http_send(req);
}

void http_run_next(http_middleware *self, http_request req) {
  if (self->next == NULL) {
	(self->handler)(req);
	return;
  }
  
  (self->next->func)(self->next, req);
};

void http_register_handler_middleware(arena *arena, http_handler *hand, http_middleware_func func) {
  http_middleware *middleware = arena_alloc(arena, sizeof(http_middleware));
  middleware->func = func;
  middleware->handler = hand->func;
  if (hand->middleware == NULL) {
	hand->middleware = middleware;
	return;
  }

  http_middleware **prev = &hand->middleware;
  http_middleware *next = hand->middleware;
  while (next->next != NULL) {
	prev = &next;
	next = next->next;
  }
  middleware->next = next;
  *prev = middleware;
}

void http_register_global_middleware(http_server *serv, http_middleware_func func) {
  arena *arena = serv->arena;
  
  http_middleware *middleware = arena_alloc(arena, sizeof(http_middleware));
  http_handler_func *handler = arena_alloc(arena, sizeof(http_handler_func));
  middleware->func = func;
  middleware->handler = *handler;
  if (serv->global_middleware == NULL) {
	http_global_middleware *global_middleware = arena_alloc(arena, sizeof(http_global_middleware));
	global_middleware->start = middleware;
	global_middleware->end = middleware;
	serv->global_middleware = global_middleware;
	return;
  }

  serv->global_middleware->end->next = middleware;
  serv->global_middleware->end = middleware;
}

int init_server(arena *arena, http_server *serv, char *addr, char *port) {
  // serv initialization
  struct addrinfo hints, *res;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(addr, port, &hints, &res);
  if (status != 0) {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
	return 1;
  }

  int sockfd = socket(res->ai_family, res->ai_socktype, 0);
  if (sockfd < 0) {
	perror("socket");
	return 1;
  }

  int yes = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
	perror("setsockopt");
	return 1;
  }

  if (bind(sockfd, res->ai_addr, res->ai_addrlen)) {
	perror("bind");
	return 1;
  }

  serv->arena = arena;
  serv->addr = (struct sockaddr *)res->ai_addr;
  serv->sockfd = sockfd;

  freeaddrinfo(res);

  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
	perror("sigaction");
	return 1;
  }

  return 0;
}

http_handler *http_handle_path(http_server *serv, char* method, char* path, http_handler_func handler) {
  http_handler hand = {
	.method = cs_from_cstr(method),
	.path = cs_from_cstr(path),
	.func = handler,
  };
  
  arena_da_append(serv->arena, &serv->handlers, hand);
  return &serv->handlers.data[serv->handlers.len-1];
}

const_string http_compose_response(arena *arena, http_response *resp) {
  const_string_da header_da = {0};
  arena_da_append(arena, &header_da, CS("HTTP/1.1"));

  if (resp->code != 0) {
	char buf[4];
	sprintf(buf, "%ld", resp->code);
	arena_da_append(arena, &header_da, cs_from_cstr(buf));
	arena_da_append(arena, &header_da, get_response_string(resp->code));
  } else {
	printf("Response is handeled, but no code is given\n");
	arena_da_append(arena, &header_da, CS("500"));
	arena_da_append(arena, &header_da, get_response_string(INTERNAL_SERVER_ERROR));
  }

  const_string_da resp_da= {0};

  const_string http_header = arena_cs_concat(arena, header_da, CS(" "));
  arena_da_append(arena, &resp_da, http_header);

  for (size_t i = 0; i < resp->headers.len; i++) {
	KV *header = (resp->headers.data + i);
	const_string header_str = {0};
	header_str = arena_cs_append(arena, header_str, header->key);
	header_str = arena_cs_append(arena, header_str, CS(": "));
	header_str = arena_cs_append(arena, header_str, header->value);
	arena_da_append(arena, &resp_da, header_str);
  }
  
  arena_da_append(arena, &resp_da, CS(""));
  if (resp->body.len != 0) {
	arena_da_append(arena, &resp_da, resp->body);
  }
  
  const_string str = arena_cs_concat(arena, resp_da, CS("\r\n"));
  
  return str;
}


void http_send_response(size_t inc_fd, const_string resp) {
  const char *resp_cstr = resp.data;
  int resp_len = resp.len;

  int sent = send(inc_fd, resp_cstr, resp_len, 0);
  while (sent > 0 && resp_len - sent > 0) {
	resp_len -= sent;
	sent = send(inc_fd, resp_cstr, resp_len, 0);
  }

  if (sent < 0) {
	perror("send");
  }
}

void http_send(http_request req) {
  arena *arena = req.arena;
  
  assert(req.resp->code);
  const_string response = http_compose_response(arena, req.resp);
  http_send_response(req.inc_fd, response);
  req.resp->sent = true;
}

void http_send_json(http_request req, const_string json) {
  arena *arena = req.arena;
  
  int num_str_len = snprintf(NULL, 0, "%d", json.len);
  char* len_str = arena_alloc(arena, snprintf(NULL, 0, "%d", json.len));
  sprintf(len_str, "%d", json.len);
  const_string len_cs = {len_str, num_str_len};

  arena_da_append(arena, &req.resp->headers, ((KV){CS("Content-Length"), len_cs}));
  arena_da_append(arena, &req.resp->headers, ((KV){CS("Content-Type"), CS("application/json")}));
  req.resp->body = json;
  
  http_send(req);
}
void http_send_body(http_request req, const_string body) {
  arena *arena = req.arena;
  
  int num_str_len = snprintf(NULL, 0, "%d", body.len+2);
  char* len_str = arena_alloc(arena, snprintf(NULL, 0, "%d", body.len));
  sprintf(len_str, "%d", body.len);
  const_string len_cs = {len_str, num_str_len};

  arena_da_append(arena, &req.resp->headers, ((KV){CS("Content-Length"), len_cs}));
  arena_da_append(arena, &req.resp->headers, ((KV){CS("Content-Type"), CS("application/text")}));
  req.resp->body = body;

  http_send(req);
}

int chop_query(arena *arena, const_string *path, http_query_da *queries) {
  const_string queries_str = *path;
  int query_count = 0;

  if (!cs_try_chop_delim(&queries_str, '?', path)) {
	return 0;
  }

  const_string query_str;

  if (!cs_try_chop_delim(&queries_str, '&', &query_str)) {
	query_str = queries_str;
  }

  do {
	KV query = {0};
	const_string key;
	if (!cs_try_chop_delim(&query_str, '=', &key)) {
	  return -1;
	}
	
	query.key = key;
	query.value = query_str;
	arena_da_append(arena, queries, query);
	query_count++;
  } while (cs_try_chop_delim(&queries_str, '&', &query_str));
  
  return query_count;
}

const_string chop_request(const_string *req_str, char delim, bool *parse_err) {
  if (*parse_err) {
	return CS("");
  }
  const_string str;
  if (!cs_try_chop_delim(req_str, delim, &str)) {
	*parse_err = true;
	return CS("");
  }
  return str;
}

int parse_request(arena *arena, size_t inc_fd, http_request *req) {
  char buf[MAX_DATA_SIZE];
  const_string req_str = {0};

  int got = recv(inc_fd, &buf, MAX_DATA_SIZE - 1, 0);
  if (got < 0) {
	perror("recv");
	return REQUEST_TIMEOUT;
  }

  buf[got] = '\0';

  req_str = arena_cs_append(arena, req_str, cs_from_cstr(buf));

  while (got == MAX_DATA_SIZE - 1) {
	got = recv(inc_fd, &buf, MAX_DATA_SIZE - 1, 0);
	if (got < 0) {
	  perror("recv");
	  return REQUEST_TIMEOUT;
	}
	
	buf[got] = '\0';

	const_string tmp = cs_from_parts(buf, got);
	req_str = arena_cs_append(arena, req_str, tmp);
  }

  bool parse_err = false;
  
  const_string method = chop_request(&req_str, ' ', &parse_err);
  const_string path = chop_request(&req_str, ' ', &parse_err);
  const_string version = chop_request(&req_str, '\r', &parse_err);
  req_str.data += 1;

  header_da headers = {0};
  while (!parse_err && req_str.data[0] != '\r') {
	const_string header_str = chop_request(&req_str, '\r', &parse_err);
	req_str.data += 1;

	KV header;
	header.key = chop_request(&header_str, ':', &parse_err);
	header_str.data++;
	header.value = header_str;

	arena_da_append(arena, &headers, header);
  }
  chop_request(&req_str, '\n', &parse_err);
  
  if (parse_err) {
	return BAD_REQUEST;
  }

  http_query_da queries = {0};
  
  if (chop_query(arena, &path, &queries) < 0) {
	return BAD_REQUEST;
  }
  
  
  req->method = method;
  req->path = path;
  req->version = version;
  req->query = queries;
  req->headers = headers;
  req->body = req_str;
  
  return 0;
}

bool check_handler(http_handler hand, http_request req) {
  arena *arena = req.arena;
  http_context *ctx = req.ctx;
  
  const_string handler_path = hand.path;
  const_string req_path = req.path;
  if (!(cs_eq(hand.method, req.method) ||
		(cs_eq(hand.method, CS("GET")) && cs_eq(req.method, CS("HEAD"))))) {
	return false;
  }

  const_string path;
  bool entered = false;

  while (cs_try_chop_delim(&handler_path, ':', &path)) {
	entered = true;
	
	const_string req_byte = {req_path.data, path.len};
	if (!cs_eq(path, req_byte)) {
	  return false;
	}
	
	req_path.data += path.len;
	req_path.len -= path.len;

	const_string name = cs_chop_delim(&handler_path, '/');
	const_string tmp = cs_chop_delim(&req_path, '/');
	const_string *value = arena_alloc(arena, sizeof(const_string));
	*value = tmp;
	set_context_value(arena, ctx, (http_context_value){name, (void *)value});
  }
  
  if (!entered && !cs_eq(handler_path, req_path)) {
	return false;
  }
  
  return true;
}

struct process_request_args {
  http_server serv;
  size_t inc_fd;
};

void *process_request(void *args) {
  struct process_request_args *pargs = args;
  http_server serv = pargs->serv;
  size_t inc_fd = pargs->inc_fd;
  
  arena req_arena = {0};

  http_request req = {0};
  req.inc_fd = inc_fd;
  req.arena = &req_arena;
  
  http_response resp = {0};
  req.resp = &resp;
  
  int parse_res = parse_request(&req_arena, inc_fd, &req);
  if (parse_res != 0) {
	resp.code = parse_res;
	http_send(req);
  }

  http_context req_context = {0};
  for (size_t i = 0; i < serv.global_ctx.len; i++) {
	set_context_value(&req_arena, &req_context, *(serv.global_ctx.data + i));
  }
  req.ctx = &req_context;

  http_global_middleware *g_mid = serv.global_middleware;
  if (!resp.sent) {
	for (size_t i = 0; i < serv.handlers.len; i++) {
	  http_handler handler = serv.handlers.data[i];
	  if (check_handler(handler, req)) {
		if (serv.global_middleware != NULL) {
		  g_mid->end->next = handler.middleware;
		  g_mid->end->handler = handler.func;
		  g_mid->start->func(g_mid->start, req);
		} else if (handler.middleware != NULL) {
		  handler.middleware->func(handler.middleware, req);
		} else {
		  handler.func(req);
		}
		
		if (!resp.sent) {
		  const_string error = CS("Server handled the request but did not send anything\n");
		  http_log(HTTP_ERROR, "%s", error.data);
		  set_context_value(&req_arena, &req_context, (http_context_value){CS("error"), (void *)&error});
		  g_mid->end->handler = http_internal_server_error_handler;
		  g_mid->start->func(g_mid->start, req);
		}
		break;
	  }
	}
  }

  if (!resp.code) {
	g_mid->end->handler = http_not_found_handler;
	g_mid->start->func(g_mid->start, req);
  }

  close(inc_fd);
  arena_free(&req_arena);

  pthread_exit(NULL);
}

int listen_and_serve(http_server *serv) {
  if (listen(serv->sockfd, 10) != 0) {
	perror("listen");
	return 1;
  }
  
  char my_ipstr[INET6_ADDRSTRLEN];
  inet_ntop(serv->addr->sa_family, get_in_addr(serv->addr), my_ipstr, sizeof my_ipstr);
  http_log(HTTP_INFO, "Listening on %s:%d\n", my_ipstr, get_in_port(serv->addr));

  while (keepRunning) {
	struct sockaddr inc_addr;
	socklen_t addr_size = sizeof inc_addr;
	int inc_fd = accept(serv->sockfd, &inc_addr, &addr_size);
	if (inc_fd < 0) {
	  perror("accept");
	  return 1;
	}

	struct process_request_args args = {
	  .serv = *serv,
	  .inc_fd = inc_fd,
	};
	
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, process_request, (void *)&args);
  }
  return 0;
}

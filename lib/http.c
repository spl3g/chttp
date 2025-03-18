#include "http.h"

#define MAX_DATA_SIZE 256
#define BUFFER_SIZE 4096
#define REQUEST_ARENA_SIZE 1024

int volatile keepRunning = 1;

struct {
  response_code code;
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

const_string get_response_string(response_code code) {
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

void set_context_value(arena *arena, context *ctx, context_value kv) {
  for (size_t i = 0; i < ctx->len; i++) {
	context_value *ctx_val = (ctx->data + i);
	if (cs_eq(kv.key, ctx_val->key)) {
	  ctx_val->val = kv.val;
	  return;
	}
  }
  arena_da_append(arena, ctx, kv);
}

void *get_context_value(context *ctx, const_string key) {
  for (size_t i = 0; i < ctx->len; i++) {
	context_value *ctx_val = (ctx->data + i);
	if (cs_eq(key, ctx_val->key)) {
	  return ctx_val->val;
	}
  }
  return NULL;
}

void resp_set_code(struct response* resp, response_code code) {
  resp->code = code;
}

void resp_set_json(arena *arena, struct response* resp, const_string json) {
  const_string len_str = arena_cs_init(arena, snprintf(NULL, 0, "%d", json.len+2));
  sprintf(len_str.data, "%d", json.len+2);
  arena_da_append(arena, &resp->headers, ((KV){CS("Content-Length"), len_str}));
  arena_da_append(arena, &resp->headers, ((KV){CS("Content-Type"), CS("application/json")}));
  resp->body = json;
}
void resp_set_body(arena *arena, struct response* resp, const_string body) {
  const_string len_str = arena_cs_init(arena, snprintf(NULL, 0, "%d", body.len));
  sprintf(len_str.data, "%d", body.len);
  arena_da_append(arena, &resp->headers, ((KV){CS("Content-Length"), len_str}));
  arena_da_append(arena, &resp->headers, ((KV){CS("Content-Type"), CS("application/text")}));
  resp->body = body;
}

int init_server(arena *arena, struct server *serv, char *addr, char *port) {
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
	perror("bind");
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

void handle_path(struct server *serv, const_string method, const_string path, handler_func handler) {
  struct handler hand = {
	.method = method,
	.path = path,
	.func = handler,
  };
  
  arena_da_append(serv->arena, &serv->handlers, hand);
}

void compose_response(arena *arena, struct response *resp) {
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
  
  if (resp->body.len != 0) {
	arena_da_append(arena, &resp_da, CS(""));
	arena_da_append(arena, &resp_da, resp->body);
  }
  
  arena_da_append(arena, &resp_da, CS(""));
  const_string str = arena_cs_concat(arena, resp_da, CS("\r\n"));
  
  resp->string = str;
}

int chop_query(arena *arena, const_string *path, query_da *queries) {
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

int parse_request(arena *arena, size_t inc_fd, struct request *req) {
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
  chop_request(&req_str, '\r', &parse_err);
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

  query_da queries = {0};
  
  if (chop_query(arena, &path, &queries) < 0) {
	return BAD_REQUEST;
  }
  
  struct request request = {
	.method = method,
	.path = path,
	.query = queries,
	.headers = headers,
	.body = req_str,
  };
  
  *req = request;
  return 0;
}

bool compare_paths(const_string handler_path, const_string req_path, arena* arena, context *ctx) {
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
	set_context_value(arena, ctx, (context_value){name, (void *)value});
  }
  
  if (!entered && !cs_eq(handler_path, req_path)) {
	return false;
  }
  
  return true;
}

void process_request(struct server *serv, size_t inc_fd) {
  arena req_arena = {0};

  struct request req;
  struct response resp = {0};
  int parse_res = parse_request(&req_arena, inc_fd, &req);
  if (parse_res != 0) {
	resp.code = parse_res;
  }

  context req_context = {0};
  set_context_value(&req_arena, &req_context, (context_value){CS("arena"), &req_arena});
  for (size_t i = 0; i < serv->global_ctx.len; i++) {
	set_context_value(&req_arena, &req_context, *(serv->global_ctx.data + i));
  }

  if (!resp.code) {
	for (size_t i = 0; i < serv->handlers.len; i++) {
	  struct handler handler = serv->handlers.data[i];
	  if ((cs_eq(handler.method, req.method) || (cs_eq(handler.method, CS("GET")) && cs_eq(req.method, CS("HEAD"))) )
		  && compare_paths(handler.path, req.path, &req_arena, &req_context)) {
		handler.func(&req_context, req, &resp);
		break;
	  }
	}
  }

  if (!resp.code) {
	resp.code = NOT_FOUND;
  }

  if (resp.string.len == 0) {
	compose_response(&req_arena, &resp);
  }

  char *resp_cstr = resp.string.data;
  int resp_len = resp.string.len;

  int sent = send(inc_fd, resp_cstr, resp_len, 0);
  while (sent > 0 && resp_len - sent > 0) {
	resp_len -= sent;
	sent = send(inc_fd, resp_cstr, resp_len, 0);
  }

  if (sent < 0) {
	perror("send");
  }

  close(inc_fd);
  arena_free(&req_arena);
}

int listen_and_serve(struct server *serv) {
  if (listen(serv->sockfd, 10) != 0) {
	perror("listen");
	return 1;
  }

  char my_ipstr[INET6_ADDRSTRLEN];
  inet_ntop(serv->addr->sa_family, get_in_addr(serv->addr), my_ipstr, sizeof my_ipstr);
  printf("Listening on %s:%d\n", my_ipstr, get_in_port(serv->addr));


  while (keepRunning) {
	struct sockaddr inc_addr;
	socklen_t addr_size = sizeof inc_addr;
	int inc_fd = accept(serv->sockfd, &inc_addr, &addr_size);
	if (inc_fd < 0) {
	  perror("accept");
	  return 1;
	}

	process_request(serv, inc_fd);
	close(inc_fd);
  }
  return 0;
}

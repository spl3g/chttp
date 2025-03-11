#include "http.h"

#define MAX_DATA_SIZE 256
#define BUFFER_SIZE 4096
#define REQUEST_ARENA_SIZE 16*1024

int volatile keepRunning = 1;

struct {
  int code;
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
  {418, CS_STATIC("Im A Teepot")},
  {500, CS_STATIC("Internal Server Error")},
  {501, CS_STATIC("Not Implemented")},
  {502, CS_STATIC("Bad Gateway")},
  {503, CS_STATIC("Service Unavailable")},
  {505, CS_STATIC("Http Version Not Suppported")},
};

const_string get_response_string(int code) {
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

  char my_ipstr[INET6_ADDRSTRLEN];
  inet_ntop(serv->addr->sa_family, get_in_addr(serv->addr), my_ipstr, sizeof my_ipstr);
  printf("Listening on %s:%d\n", my_ipstr, get_in_port(serv->addr));

  return 0;
}

void handle_path(struct server *serv, const_string method, const_string path, handler_func handler) {
  struct handler hand = {
	.method = method,
	.path = path,
	.func = handler,
  };
  
  *push(&serv->handlers, serv->arena) = hand;
}

void compose_response(struct response *resp, arena *perm, arena scratch) {
  const_string_da header_da = {0};
  *push(&header_da, &scratch) = CS("HTTP/1.1");

  if (resp->code != 0) {
	char buf[4];
	sprintf(buf, "%ld", resp->code);
	*push(&header_da, &scratch) = cs_from_cstr(buf);
	*push(&header_da, &scratch) = get_response_string(resp->code);
  } else {
	printf("Response is handeled, but no code is given");
	*push(&header_da, &scratch) = CS("500");
	*push(&header_da, &scratch) = get_response_string(INTERNAL_SERVER_ERROR);
  }

  volatile const_string header = arena_cs_concat(&scratch, header_da, CS(" "));

  volatile const_string str;
  if (resp->body.len != 0) {
	const_string_da resp_da= {0};
	
	*push(&resp_da, perm) = header;
	*push(&resp_da, perm) = CS("");
	*push(&resp_da, perm) = resp->body;
	
	str = arena_cs_concat(&scratch, resp_da, CS("\r\n"));
  } else {
	str = arena_cs_append(perm, header, CS("\r\n\r\n"));
  }
  
  resp->string = str;
}

void process_request(struct server *serv, ssize_t inc_fd) {
  arena req_arena = {0};
  arena req_scratch = {0};
  char buf[MAX_DATA_SIZE];
  const_string req_str = {0};

  int got = recv(inc_fd, &buf, MAX_DATA_SIZE - 1, 0);
  if (got < 0) {
	perror("recv");
	return;
  }

  buf[got] = '\0';

  req_str = arena_cs_append(&req_arena, req_str, cs_from_cstr(buf));

  while (got == MAX_DATA_SIZE - 1) {
	got = recv(inc_fd, &buf, MAX_DATA_SIZE - 1, 0);
	if (got < 0) {
	  perror("recv");
	  return;
	}

	buf[got] = '\0';

	const_string tmp = cs_from_parts(buf, got);
	req_str = arena_cs_append(&req_arena, req_str, tmp);
  }

  const_string method = cs_chop_delim(&req_str, ' ');
  const_string path = cs_chop_delim(&req_str, ' ');
  cs_chop_delim(&req_str, '\r');
  req_str.data += 1;

  header_da headers = {0};
  while (req_str.data[0] != '\r') {
	const_string header_str = cs_chop_delim(&req_str, '\r');
	req_str.data += 1;

	struct header header;
	header.key = cs_chop_delim(&header_str, ':');
	header_str.data++;
	header.value = header_str;

	*push(&headers, &req_arena) = header;
  }
  cs_chop_delim(&req_str, '\n');
  
  struct request req = {
	.method = method,
	.path = path,
	.headers = headers,
	.body = req_str,
  };
  struct response resp = {0};

  bool handled = false;
  for (size_t i = 0; i < serv->handlers.len; i++) {
	struct handler handler = serv->handlers.data[i];
	if ((cs_eq(handler.path, CS("HEAD"))
		 || cs_eq(handler.path, path))
		&& cs_eq(serv->handlers.data[i].path, path)) {
	  serv->handlers.data[i].func(req, &resp);
	  handled = true;
	  break;
	}
  }

  if (!handled) {
	resp.code = 404;
  }

  if (resp.string.len == 0) {
	compose_response(&resp, &req_arena, req_scratch);
  }

  char *resp_cstr = resp.string.data;
  int resp_len = resp.string.len;

  int sent = send(inc_fd, resp_cstr, resp_len, 0);
  while (sent > 0 && resp_len - sent > 0) {
	resp_len -= sent;
	sent = send(inc_fd, resp_cstr, resp_len, 0);
  }

  if (sent < 0) {
	fprintf(stderr, "send: %s\n", strerror(errno));
  }
  
  close(inc_fd);
  arena_free(&req_arena);
}

int listen_and_serve(struct server *serv) {
  if (listen(serv->sockfd, 10) != 0) {
	fprintf(stderr, "listen: %s\n", strerror(errno));
	return 1;
  }

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

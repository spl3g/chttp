#include "lib/const_strings.h"
#include "lib/http.h"

void success(struct request req) {
  req.resp->code = OK;
  req.resp->body = CS("Hello world!\n");
  http_send(req);
}

void logging_func(http_middleware *self, struct request req) {
  (self->next->func)(self->next, req);
  http_log(HTTP_INFO, "[1]"CS_FMT" "CS_FMT": %ld\n", CS_ARG(req.method), CS_ARG(req.path), req.resp->code);
}

int main() {
  arena arena = {0};
  struct server serv = {0};
  if (init_server(&arena, &serv, "127.0.0.1", "6969") != 0) {
	return 1;
  }

  handler *success_handler = handle_path(&serv, CS("GET"), CS("/"), success);
  http_register_handler_middleware(&arena, success_handler, logging_func);
  
  listen_and_serve(&serv);

  return 0;
}

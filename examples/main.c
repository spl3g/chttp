#include <chttp/const_strings.h>
#include <chttp/http.h>

void success(http_request req) {
  req.resp->code = OK;
  req.resp->body = CS("Hello world!\n");
  http_send(req);
}

void logging_func(http_middleware *self, http_request req) {
  http_run_next(self, req);
  http_log(HTTP_INFO, CS_FMT" "CS_FMT": %ld\n", CS_ARG(req.method), CS_ARG(req.path), req.resp->code);
}

void whatever(http_middleware *self, http_request req) {
  http_log(HTTP_INFO, "Whatever\n");
  http_run_next(self, req);
}

int main() {
  arena arena = {0};
  http_server serv = {0};
  if (init_server(&arena, &serv, "127.0.0.1", "6969") != 0) {
	return 1;
  }

  http_register_global_middleware(&serv, logging_func);
  http_handler *success_handler = http_handle_path(&serv, "GET", "/", success);
  http_register_handler_middleware(&arena, success_handler, whatever);
  
  listen_and_serve(&serv);

  return 0;
}

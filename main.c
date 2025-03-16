#include "lib/http.h"

void success(struct request req, struct response *resp) {
  printf("Success handler:\n");
  cs_print("Method: %s\n", req.method);
  cs_print("Path: %s\n", req.path);
  cs_print("Body: %s\n", req.body);

  for (size_t i = 0; i < req.query.len; i++) {
	cs_print("%s: ", req.query.data[i].key);
	cs_print("%s\n", req.query.data[i].val);
  };

  resp->code = OK;
  resp->body = CS("Hello world!\n");
}

int main() {
  arena arena = {0};
  struct server serv = {0};
  if (init_server(&arena, &serv, "127.0.0.1", "6969") != 0) {
	return 1;
  }

  handle_path(&serv, CS("GET"), CS("/"), success);
  listen_and_serve(&serv);

  return 0;
}

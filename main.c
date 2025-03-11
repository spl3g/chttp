#include "lib/http.h"

void success(struct request req, struct response *resp) {
  printf("Success handler:\n");
  cs_print("Method: %s\n", req.method);
  cs_print("Path: %s\n", req.path);
  cs_print("Body: %s\n", req.body);

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

  // closing and freeing
  printf("closing sockets");
  close(serv.sockfd);

  return 0;
}

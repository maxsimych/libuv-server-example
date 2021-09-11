#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define PORT 6000
#define BACKLOG 128

static uv_loop_t* loop;
static struct sockaddr_in addr;

typedef struct {
  uv_write_t  req;
  uv_buf_t buf;
} write_req_t;

static void free_write_req(uv_write_t* req) {
  write_req_t* wr = req;
  free(wr->buf.base);
  free(wr);
}

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void close_cb(uv_stream_t* client) {
  printf("Connection closed\n");
  free(client);
}

static void write_cb(uv_write_t* req, int status) {
  if (status) {
    fprintf(stderr, "Write error %s\n", uv_strerror(status));
  }
  printf("Sent back the same message\n");
  free_write_req(req);
}

static void read_cb(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  if (nread > 0) {
    printf("Incoming message is %s\0\n", *buf);
    write_req_t* req = malloc(sizeof(write_req_t));
    req->buf = uv_buf_init(buf->base, nread);
    uv_write(req, client, &req->buf, 1, write_cb);
    uv_close(client, close_cb);
    return;
  }
  if (nread < 0) {
    if (nread != UV_EOF) {
      fprintf(stderr, "Read error %s\n", uv_strerror(nread));
    }
    uv_close(client, close_cb);
  }
  free(buf->base);
}

static void on_new_connection(uv_stream_t* server, int status) {
  printf("New connection\n");
  if (status < 0) {
    fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    return;
  }

  uv_tcp_t* client = malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);
  if (uv_accept(server, client) == 0 ) {
    uv_read_start(client, alloc_buffer, read_cb);
  } else {
    uv_close(client, close_cb);
  }
}

void one_time_idle_cb (uv_idle_t* handle) {
  printf("TCP Server started on port %d\n", PORT);
  uv_idle_stop(handle);
}

int main() {
  loop = uv_default_loop();

  uv_idle_t one_time_idle;
  uv_idle_init(loop, &one_time_idle);
  uv_idle_start(&one_time_idle, one_time_idle_cb);

  uv_tcp_t server;
  uv_tcp_init(loop, &server);
  uv_ip4_addr("0.0.0.0", PORT, &addr);
  uv_tcp_bind(&server, &addr, 0);
  int status = uv_listen(&server, BACKLOG, on_new_connection);
  if (status) {
    fprintf(stderr, "Listen error %s\n", uv_strerror(status));
    return 1;
  }
  uv_run(loop, UV_RUN_DEFAULT);
  free(uv_default_loop());
  return 0;
}
#include "common.c"

static buffer send_buffer;

int main(void) {
  peer_t self = { 0 };
  struct sockaddr_in remote_addr = { 0 };
  uint32_t remote_addr_size = sizeof(remote_addr);
  buffer_t b = { .buff = &send_buffer, .cap = sizeof(send_buffer.data_buffer) };
  struct {
    peer_t* arr;
    size_t len, cap;
  } remotes;

  udp_listen(&self.fd, 42069);
  while (1) {
    endpoint_t ep;
    buff_recv(b, self, &ep);
    msg_kind_t kind;
    buff_read(b, kind);
    if (kind == msg_pts_hello) {
      peer_t r = { .remote_ip = ep.ip, .remote_port = ep.port };
      buff_read(b, r.local_port);
      buff_read_br_arr(b, r.local_ips);

      b.len = 0;
      buff_push_l(b, msg_stp_hello, uint8_t);
      buff_push(b, r.remote_ip);
      buff_push(b, r.remote_port);
      buff_push_l(b, remotes.len, uint8_t);
      for (size_t i = 0; i < remotes.len; ++i) {
        peer_t ro = remotes.arr[i];
        buff_push(b, ro.remote_ip);
        buff_push(b, ro.remote_port);
      }
    } else {
      LOGE(EP_FMT " Bad message kind %d", EPTOF(ep), kind);
    }
  }
}

// gcc -o stun stun_udp.c && ./stun
// scp stun_udp.c common.h root@10.169.0.1:/root && ssh root@10.169.0.1 "gcc stun_udp.c && ./a.out"

#define NAME "STUN"
#include "common.c"
#include "br_da.h"

static buffer send_buffer;

int main(void) {
  struct sockaddr_in remote_addr = { 0 };
  uint32_t remote_addr_size = sizeof(remote_addr);
  buffer_t b = { .buff = &send_buffer, .cap = sizeof(send_buffer.data_buffer)};
  struct {
    peer_t* arr;
    size_t len, cap;
  } remotes = { 0 };

  g_self.local_port = udp_listen(&g_self.fd, 42069);
  while (1) {
    endpoint_t ep;
    msg_kind_t kind;

    buff_recv(&b, g_self, &ep);
    buff_read(b, kind);
    if (kind == msg_pts_hello) {
      peer_t r = { .remote_ip = ep.ip, .remote_port = ep.port, .active_ep = ep };
      buff_read(b, r.local_port);
      buff_read_br_arr(b, r.local_ips);
      LOGI("Got remote: %d local_ips", r.local_ips.len);

      b.len = 0;
      buff_push_l(b, msg_stp_hello, uint8_t);
      buff_push(b, r.remote_ip);
      buff_push(b, r.remote_port);
      buff_push_l(b, remotes.len, uint8_t);
      for (size_t i = 0; i < remotes.len; ++i) {
        peer_t ro = remotes.arr[i];
        buff_push(b, ro.remote_ip);
        buff_push(b, ro.remote_port);
        buff_push(b, ro.local_port);
        buff_push_br_arr(b, ro.local_ips);
      }
      buff_move_to(&b, g_self, ep);

      buff_push_l(b, msg_stp_newp, msg_kind_t);
      buff_push(b, r.remote_ip);
      buff_push(b, r.remote_port);
      buff_push(b, r.local_port);
      buff_push_br_arr(b, r.local_ips);
      for (size_t i = 0; i < remotes.len; ++i) {
        buff_send_to(b, g_self, remotes.arr[i].active_ep);
      }
      br_da_push(remotes, r);
      b.read_index = b.len = 0;
    } else {
      LOGE(EP_FMT " Bad message kind %d", EPTOF(ep), kind);
    }
  }
}

// gcc -o stun stun_udp.c && ./stun
// scp stun_udp.c common.h root@10.169.0.1:/root && ssh root@10.169.0.1 "gcc stun_udp.c && ./a.out"

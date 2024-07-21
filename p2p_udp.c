#include "common.c"
#include "br_da.h"

#include "unistd.h"
#include "pthread.h"
#include "ifaddrs.h"

static void init_peer_self(peer_t* self) {
  uint16_t port = udp_listen(&self->fd, 0); // Port 0 -> Let the os decide and give oneself an efemeral port.
  struct ifaddrs *ifs = NULL;
  int n = getifaddrs(&ifs);
  for (struct ifaddrs* cur = ifs; cur; cur = cur->ifa_next) {
    struct sockaddr_in* sock = (void*)cur->ifa_addr;
    if (sock->sin_family != AF_INET) continue;
    uint32_t ip = sock->sin_addr.s_addr;
    if (ip == (IPTOI(127, 0, 0, 1))) continue; // Everybody has 127.0.0.1 so this is zero info..
    br_da_push(self->local_ips, ip);
  }
  freeifaddrs(ifs);
}

static void buff_push_pts_hello(buffer_t buff, const peer_t* self) {
  buff_push_l(buff, msg_pts_hello, msg_kind_t);
  buff_push(buff, self->local_port);
  buff_push_l(buff, self->local_ips.len, uint8_t);
  buff_push_arr(buff, self->local_ips.arr, self->local_ips.len);
}

buffer send_buffer;

int main(void) {
  endpoint_t stun = { .ip = IPTOI(194, 36, 45, 54), .port = LTOH(42069) };
  peer_t self = { 0 };
  buffer_t b = { .buff = &send_buffer, .len = 0, .cap = sizeof(send_buffer.data_buffer) };
  struct {
    peer_t* arr;
    size_t len, cap;
  } remotes;

  init_peer_self(&self);

  buff_push_pts_hello(b, &self);
  buff_move_to(b, self, stun);

  while(1) {
    endpoint_t endpoint;

    buff_recv(b, self, &endpoint);
    if (EPEQ(endpoint, stun)) {
      msg_kind_t kind;
      buff_read(b, kind);
      if (kind == msg_stp_hello) {
        uint8_t n = 0;
        buff_read(b, n);
        for (int i = 0; i < n; ++i) {
          peer_t remote = { 0 };
          buff_read(b, remote.remote_port);
          buff_read(b, remote.local_port);
          buff_read(b, remote.local_ips.len);
          remote.local_ips.arr = malloc(sizeof(remote.local_ips.arr[0]) * remote.local_ips.len);
          remote.local_ips.cap = 0;
          buff_read_arr(b, remote.local_ips.arr, remote.local_ips.len);
          br_da_push(remotes, remote);
        }
      } else {
        LOGE("Bad message kind from stun");
      }
    } else {
      LOGI(EP_FMT " " BUFF_FMT, EPTOF(endpoint), BTOF(b));
    }
  }
}

// gcc -o p2p p2p_udp.c && ./p2p
// gcc -o p2p p2p_udp.c && scp p2p_udp.c common.h root@10.169.0.5:/root && ./p2p

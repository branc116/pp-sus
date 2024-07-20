// This should be started on a client
// and this should communicate with a server that is
// on port 42069 and when it gets a msg from the server
// with other remotes, it will start sending msgs to all
// other remotes. ( and reciving msgs from other clients )
#include "common.h"
#include "br_da.h"

#include "unistd.h"
#include "pthread.h"
#include "ifaddrs.h"

typedef uint8_t msg_kind_t;
typedef uint8_t byte;

#define msg_pts_hello 1
#define msg_stp_hello 2

typedef struct {
  byte* buff;
  size_t len, cap;

  size_t read_index;
} buffer_t;

typedef struct {
  uint32_t* arr;
  int len, cap;
} ips_t;

typedef struct {
  uint32_t ip;
  uint16_t port;
} endpoint_t;

typedef struct {
  ips_t ips;
  uint16_t port;
  int fd;
} peer_t;

typedef struct {
  ips_t ips;
  uint16_t remote_port;
  uint16_t local_port;
  buffer_t buffer;
} remote_peer_t;

void init_peer_self(peer_t* self) {
  uint16_t port = udp_listen(&fd, 0); // Port 0 -> Let the os decide and give oneself an efemeral port.
  struct ifaddrs *ifs = NULL;
  int n = getifaddrs(&ifs);
  for (struct ifaddrs* cur = ifs; cur; cur = cur->ifa_next) {
    struct sockaddr_in* sock = (void*)cur->ifa_addr;
    if (sock->sin_family != AF_INET) continue;
    uint32_t ip = sock->sin_addr.s_addr;
    br_da_push(self->ips, ip);
  }
  freeifaddrs(ifs);
}

#define buff_push_a(B, WS, ADDR) do {\
  assert((WS) + (B).len <= (B).cap); \
  memcpy(&(B).buff[(B).len], ADDR, WS); \
  (B).len += write_size; \
} while (0)

#define buff_push(B, V) do { \
  size_t write_size = sizeof((V)); \
  buff_push_a(B, write_size, &(V)); \
} while(0)

#define buff_push_l(B, V, T) do { \
  size_t write_size = sizeof(T); \
  T tmp = V; \
  buff_push_a(B, write_size, &(tmp)); \
} while(0)

#define buff_push_arr(B, ARR, LEN) do { \
  size_t write_size = sizeof((ARR)[0]) * (LEN); \
  buff_push_a(B, write_size, ARR); \
} while(0)

#define buff_read(B, V) do { \
  assert(sizeof((V)) + (B).read_index <= (B).len); \
  memcpy(&(V), &(B).buff[(B).read_index], sizeof((V))); \
  (B).read_index += sizeof((V)); \
} while (0);

#define buff_read_arr(B, ARR, LEN) do { \
  size_t read_len = sizeof((ARR)[0]) * (LEN); \
  assert(read_len + (B).read_index <= (B).len); \
  memcpy((ARR), &(B).buff[(B).read_index], read_len); \
  (B).read_index += read_len; \
} while(0)

void buff_push_pts_hello(buffer_t buff, const peer_t* self) {
  buff_push_l(buff, msg_pts_hello, msg_kind_t);
  buff_push(buff, self->port);
  buff_push_l(buff, self->ips.len, uint8_t);
  buff_push_arr(buff, self->ips.arr, self->ips.len);
}

void buff_send_to(buffer_t buff, peer_t self, endpoint_t endpoint) {
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = endpoint.ip,
    .sin_port = endpoint.port
  };
  if (-1 == sendto(self.fd, buff.buff, buff.len, 0, (void*)&addr, sizeof(addr))) {
    LOGE("Failed to send: %s", strerror(errno));
    exit(1);
  }
}
void buff_move_to(buffer_t buff, peer_t self, endpoint_t endpoint) {
  buff_send_to(buff, self, endpoint);
  buff.len = 0;
}

void buff_recv(buffer_t buff, peer_t self, endpoint_t* ep) {
  assert(buff.len == 0);
  struct sockaddr_in addr = { .sin_family = AF_INET };
  uint32_t addr_size = sizeof(addr);
  ssize_t s = recvfrom(self.fd, buff.buff, buff.len, 0, (void*)&addr, &addr_size);
  if (s == -1) {
    LOGE("Failed to read from udp socket: %s", strerror(errno));
    exit(1);
  }
  buff.len += s;
  ep->ip = addr.sin_addr.s_addr;
  ep->port = addr.sin_port;
}

int main(void) {
  endpoint_t stun = { .ip = IPTOI(194, 36, 45, 54), .port = LTOH(42069) };
  peer_t self = { 0 };
  buffer_t b = { .buff = buff, .len = 0, .cap = sizeof(buff) };
  struct {
    remote_peer_t* arr;
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
          remote_peer_t remote = { 0 };
          buff_read(b, remote.remote_port);
          buff_read(b, remote.local_port);
          buff_read(b, remote.ips.len);
          remote.ips.arr = malloc(sizeof(remote.ips.arr[0]) * remote.ips.len);
          remote.ips.cap = 0;
          buff_read_arr(b, remote.ips.arr, remote.ips.len);
          br_da_push(remotes, remote);
        }
      } else {
        LOGE("Bad message kind from stun");
      }
    } else {
      LOGI("[" EP_FMT "] " BUFF_FMT, EPTOF(endpoint), BTOF(b));
    }
  }
}

// gcc -o p2p p2p_udp.c && ./p2p
// gcc -o p2p p2p_udp.c && scp p2p_udp.c common.h root@10.169.0.5:/root && ./p2p

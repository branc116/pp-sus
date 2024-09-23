#define NAME " P2P"
#include "common.c"
#include "br_da.h"

#include "unistd.h"
#include "pthread.h"
#include "ifaddrs.h"
#include "crypt.h"

static void set_rand(void) {
  long l = 0;
  FILE* f = fopen("/dev/random", "rb");
  fread(&l, sizeof(l), 1, f);
  srand(l);
  fclose(f);
  return;
}

static void init_peer_self(peer_t* self) {
  uint16_t port = udp_listen(&self->fd, 0); // Port 0 -> Let the os decide and give oneself an efemeral port.
  self->local_port = port;
  struct ifaddrs *ifs = NULL;
  int n = getifaddrs(&ifs);
  for (struct ifaddrs* cur = ifs; cur; cur = cur->ifa_next) {
    struct sockaddr_in* sock = (void*)cur->ifa_addr;
    if (sock == NULL || sock->sin_family != AF_INET) continue;
    uint32_t ip = sock->sin_addr.s_addr;
    if (ip == (IPTOI(127, 0, 0, 1))) continue; // Everybody has 127.0.0.1 so this is zero info..
    br_da_push(self->local_ips, ip);
  }
  freeifaddrs(ifs);
  set_rand();
  generate_rsa(self);
}

static void buff_push_pts_hello(buffer_t* buff, const peer_t* self) {
  buff_push_l(*buff, msg_pts_hello, msg_kind_t);
  buff_push(*buff, self->local_port);
  buff_push_l(*buff, self->local_ips.len, uint8_t);
  buff_push_arr(*buff, self->local_ips.arr, self->local_ips.len);
}

static void peer_set_active_ep(peer_t* remote, peer_t const* self) {
  const uint32_t localhost = IPTOI(127, 0, 0, 1);
  if (remote->remote_ip != self->remote_ip)                                remote->active_ep = (endpoint_t) { .ip = remote->remote_ip, .port = remote->remote_port };
  else if (remote->remote_ip == localhost && self->remote_ip == localhost) remote->active_ep = (endpoint_t) { .ip = localhost, remote->local_port };
  else if (remote->remote_port != self->remote_port) {
    if (remote->local_ips.len != 0)                                        LOGF("Remote is on the same network, but has no local ips...");
    else                                                                   remote->active_ep = (endpoint_t) { .ip = remote->local_ips.arr[0], remote->local_port };
  } else if (remote->local_port != self->local_port)                       remote->active_ep = (endpoint_t) { .ip = localhost, .port = remote->local_port };
  else                                                                     LOGF("Something strange is going on...");
  LOGI("Active ep: " EP_FMT, EPTOF(remote->active_ep));
}

typedef struct {
  int status;
  int sleep;
} remote_peer_data_t;

static buffer send_buffer;
static buffer recv_buffer;

static void* p2p_thread(void* null) {
  buffer_t b = { .buff = &send_buffer, .len = 0, .cap = sizeof(send_buffer.data_buffer) };

  do {
    for (size_t i = 0; i < remotes.len; ++i) {
      remote_peer_data_t* r = remotes.arr[i].data;
      if (--r->sleep >= 0) continue;
      if (r->status == 0) {
        buff_push_l(b, msg_ptp_punch, msg_kind_t); 
        buff_move_to(&b, g_self, remotes.arr[i].active_ep);
        r->sleep = 10;
      } else if (r->status == 1) {
        buff_push_l(b, msg_ptp_punch_ack, msg_kind_t); 
        buff_move_to(&b, g_self, remotes.arr[i].active_ep);
        r->sleep = 10;
      } else if (r->status == 2) {
        buff_push_l(b, msg_ptp_dice_throw, msg_kind_t);
        buff_push(b, g_self.dice_throw);
        buff_move_to(&b, g_self, remotes.arr[i].active_ep);
        r->sleep = 10;
      } else if (r->status == 3) {

      }
    }
    nanosleep(&(struct timespec) { .tv_nsec = 1000000 }, &(struct timespec) {});
  } while(1);
}

int main(void) {
  endpoint_t stun = {
    .ip = IPTOI(127, 0, 0, 1),
    //.ip = IPTOI(194, 36, 45, 54),
    .port = LTOH(42069)
  };
  buffer_t sb = { .buff = &send_buffer, .len = 0, .cap = sizeof(send_buffer.data_buffer) };
  buffer_t rb = { .buff = &recv_buffer, .len = 0, .cap = sizeof(recv_buffer.data_buffer) };

  init_peer_self(&g_self);

  buff_push_pts_hello(&sb, &g_self);
  buff_move_to(&sb, g_self, stun);
  pthread_create(&(pthread_t){}, NULL, p2p_thread, NULL);

  while(1) {
    endpoint_t endpoint;

    buff_recv(&rb, g_self, &endpoint);
    if (EPEQ(endpoint, stun)) {
      msg_kind_t kind;
      buff_read(rb, kind);
      if (kind == msg_stp_hello) {
        LOGI("Got hello");
        uint8_t n = 0;
        buff_read(rb, g_self.remote_ip);
        buff_read(rb, g_self.remote_port);
        buff_read(rb, n);
        for (int i = 0; i < n; ++i) {
          peer_t remote = { .data = calloc(1, sizeof(remote_peer_data_t)) };
          buff_read(rb, remote.remote_ip);
          buff_read(rb, remote.remote_port);
          buff_read(rb, remote.local_port);
          buff_read_br_arr(rb, remote.local_ips);
          peer_set_active_ep(&remote, &g_self);
          br_da_push(remotes, remote);
        }
      } else if (kind == msg_stp_newp) {
        LOGI("Got newp");
        peer_t remote = { .data = calloc(1, sizeof(remote_peer_data_t)) };
        buff_read(rb, remote.remote_ip);
        buff_read(rb, remote.remote_port);
        buff_read(rb, remote.local_port);
        buff_read_br_arr(rb, remote.local_ips);
        peer_set_active_ep(&remote, &g_self);
        br_da_push(remotes, remote);
      } else {
        LOGE("Bad message kind from stun: %d", kind);
      }
    } else {
      msg_kind_t kind;
      buff_read(rb, kind);
      if (kind == msg_ptp_punch) {
        for (size_t i = 0; i < remotes.len; ++i) {
          if (EPEQ(endpoint, remotes.arr[i].active_ep)) {
            remote_peer_data_t* r = remotes.arr[i].data;
            LOGI("Punch from " EP_FMT, EPTOF(remotes.arr[i].active_ep));
            r->status = 1;
            r->sleep = 0;
            goto end;
          }
        }
        LOGE("NO remote matches");
        exit(1);
      } else if (kind == msg_ptp_punch_ack) {
        for (size_t i = 0; i < remotes.len; ++i) {
          if (EPEQ(endpoint, remotes.arr[i].active_ep)) {
            LOGI("Punch acked from " EP_FMT, EPTOF(remotes.arr[i].active_ep));
            remote_peer_data_t* r = remotes.arr[i].data;
            r->status = 2;
            r->sleep = 0;
            goto end;
          }
        }
        LOGF("NO remote matches");
      } else {
        LOGF("Bad message kind from " EP_FMT " : %d", EPTOF(endpoint), kind);
      }
    }
    end: rb.len = rb.read_index = 0;
  }
}

// gcc -o p2p p2p_udp.c && ./p2p
// gcc -o p2p p2p_udp.c && scp p2p_udp.c common.h root@10.169.0.5:/root && ./p2p

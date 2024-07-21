#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"

#include "netinet/in.h"
#include "sys/socket.h"

#define LTOH(num) (uint16_t)(((num) >> 8) | ((num) << 8))
#define HTOL LTOH
#define ITOIP(out_str, ip) sprintf(out_str, "%d.%d.%d.%d", (((ip) >> 0) & 0xFF), \
                                                           (((ip) >> 8) & 0xFF), \
                                                           (((ip) >> 16) & 0xFF), \
                                                           (((ip) >> 24) & 0xFF));
#define LOGE(fmt, ...) fprintf(stderr, "[ERROR][%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#define LOGI(fmt, ...) fprintf(stderr, " [INFO][%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#define ADDR_FMT "(%d, %d.%d.%d.%d %u|%u)"
#define ATOF(a) (a).sin_family, (a).sin_addr.s_addr >> 0  & 0xFF, \
                              (a).sin_addr.s_addr >> 8  & 0xFF, \
                              (a).sin_addr.s_addr >> 16 & 0xFF, \
                              (a).sin_addr.s_addr >> 24 & 0xFF, HTOL((a).sin_port), (a).sin_port
#define EP_FMT "[%d.%d.%d.%d:%u]"
#define EPTOF(EP) (EP).ip >> 0  & 0xFF, \
                  (EP).ip >> 8  & 0xFF, \
                  (EP).ip >> 16 & 0xFF, \
                  (EP).ip >> 24 & 0xFF, HTOL((EP).port)
#define BUFF_FMT "%.*s"
#define BTOF(B) (int)(B).len, (B).buff->whole_buffer
#define IPEQ(a, b) ((a).sin_addr.s_addr == (b).sin_addr.s_addr && \
                    (a).sin_port == (b).sin_port && \
                    (a).sin_family == (b).sin_family)
#define EPEQ(a, b) ((a).ip == (b).ip && \
                    (a).port == (b).port)
#define IPTOI(a, b, c, d) a | (b << 8) | (c << 16) | (d << 24)

#define msg_pts_hello 1
#define msg_stp_hello 2

#define buff_push_a(B, WS, ADDR) do {\
  assert((WS) + (B).len <= (B).cap); \
  memcpy(&(B).buff->data_buffer[(B).len], ADDR, WS); \
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
  memcpy(&(V), &(B).buff->data_buffer[(B).read_index], sizeof((V))); \
  (B).read_index += sizeof((V)); \
} while (0);

#define buff_read_arr(B, ARR, LEN) do { \
  size_t read_len = sizeof((ARR)[0]) * (LEN); \
  assert(read_len + (B).read_index <= (B).len); \
  memcpy((ARR), &(B).buff->data_buffer[(B).read_index], read_len); \
  (B).read_index += read_len; \
} while(0)

#define buff_read_br_arr(B, ARR) do { \
  buff_read(B, (ARR).len); \
  (ARR).arr = malloc(sizeof((ARR).arr[0]) * (ARR).len); \
  (ARR).cap = (ARR).len; \
  buff_read_arr(B, (ARR).arr, (ARR).len); \
} while (0)

#define WHOLE_BUFFER_SIZE 1024

typedef uint8_t msg_kind_t;
typedef uint8_t byte;

typedef struct {
  uint16_t whole_size;
  uint16_t data_crc;
} header_t;

typedef struct {
  union {
    byte whole_buffer[WHOLE_BUFFER_SIZE];
    struct {
      header_t header;
      byte data_buffer[WHOLE_BUFFER_SIZE - sizeof(header_t)];
    };
  };
} buffer;

typedef struct {
  buffer* buff;
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
  ips_t local_ips;
  uint16_t local_port;

  uint32_t remote_ip;
  uint16_t remote_port;
  int fd;
} peer_t;


static uint16_t udp_listen(int* fd, int port) {
  *fd = socket(AF_INET, SOCK_DGRAM, AF_UNSPEC);
  if (*fd == -1) {
    LOGE("Failed to open a socket: %s", strerror(errno));
    exit(1);
  }
  LOGI("Created socket: %d", *fd);

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = INADDR_ANY,
    .sin_port = LTOH(port)
  };
  if (-1 == bind(*fd, (void*)&addr, sizeof(addr))) {
    LOGE("Failed to bind: %s", strerror(errno));
    exit(1);
  }
  if (-1 == getsockname(*fd, (void*)&addr, &(uint32_t) { sizeof(addr) })) {
    LOGE("Failed to get a port for efemeral socket: %s", strerror(errno));
  }
  LOGI("Bind success: %u", addr.sin_port);
  return addr.sin_port;
}

static void buff_send_to(buffer_t buff, peer_t self, endpoint_t endpoint) {
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = endpoint.ip,
    .sin_port = endpoint.port
  };
  buff.buff->header.whole_size = buff.len;
  if (-1 == sendto(self.fd, buff.buff->whole_buffer, buff.len + sizeof(buff.buff->header), 0, (void*)&addr, sizeof(addr))) {
    LOGE("Failed to send: %s", strerror(errno));
    exit(1);
  }
}

static void buff_move_to(buffer_t buff, peer_t self, endpoint_t endpoint) {
  buff_send_to(buff, self, endpoint);
  buff.len = 0;
}

static void buff_recv(buffer_t buff, peer_t self, endpoint_t* ep) {
  assert(buff.len == 0);
  struct sockaddr_in addr = { .sin_family = AF_INET };
  uint32_t addr_size = sizeof(addr);
  ssize_t s = recvfrom(self.fd, buff.buff, buff.len, 0, (void*)&addr, &addr_size);
  assert(s == buff.buff->header.whole_size + 4);
  if (s == -1) {
    LOGE("Failed to read from udp socket: %s", strerror(errno));
    exit(1);
  }
  buff.len += s;
  ep->ip = addr.sin_addr.s_addr;
  ep->port = addr.sin_port;
}


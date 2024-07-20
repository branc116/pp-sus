#include "sys/socket.h"
#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "netinet/in.h"

#define LTOH(num) (uint16_t)(((num) >> 8) | ((num) << 8))
#define HTOL LTOH
#define ITOIP(out_str, ip) sprintf(out_str, "%d.%d.%d.%d", (((ip) >> 0) & 0xFF), \
                                                           (((ip) >> 8) & 0xFF), \
                                                           (((ip) >> 16) & 0xFF), \
                                                           (((ip) >> 24) & 0xFF));
#define LOGE(fmt, ...) fprintf(stderr, "[ERROR][%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#define LOGI(fmt, ...) fprintf(stderr, " [INFO][%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#define ADDR_FMT "(%d, %d.%d.%d.%d %u|%u)"
#define EP_FMT "%d.%d.%d.%d:%u"
#define ATOF(a) (a).sin_family, (a).sin_addr.s_addr >> 0  & 0xFF, \
                              (a).sin_addr.s_addr >> 8  & 0xFF, \
                              (a).sin_addr.s_addr >> 16 & 0xFF, \
                              (a).sin_addr.s_addr >> 24 & 0xFF, HTOL((a).sin_port), (a).sin_port
#define EP_FMT "%d.%d.%d.%d:%u"
#define EPTOF(EP) (EP).ip >> 0  & 0xFF, \
                  (EP).ip >> 8  & 0xFF, \
                  (EP).ip >> 16 & 0xFF, \
                  (EP).ip >> 24 & 0xFF, HTOL((EP).port)
#define BUFF_FMT "%.*s"
#define BTOF(B) (int)(B).len, (B).buff
#define IPEQ(a, b) ((a).sin_addr.s_addr == (b).sin_addr.s_addr && \
                    (a).sin_port == (b).sin_port && \
                    (a).sin_family == (b).sin_family)
#define EPEQ(a, b) ((a).ip == (b).ip && \
                    (a).port == (b).port)
#define IPTOI(a, b, c, d) a | (b << 8) | (c << 16) | (d << 24)

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
  if (-1 == getsockname(*fd, (void*)&addr, sizeof(addr))) {
    LOGE("Failed to get a port for efemeral socket: %s", strerror(errno));
  }
  LOGI("Bind success: %u", addr.sin_port);
  return addr.sin_port;
}

#define REMOTES_N 8
static struct sockaddr_in remotes[REMOTES_N];
static const char* secret_msg = "Hi";
static int fd;
static uint8_t buff[1024];

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "stdbool.h"

#include "netinet/in.h"
#include "sys/socket.h"

#include "openssl/crypto.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "openssl/err.h"
#include "openssl/core_names.h"
#include "openssl/decoder.h"
#define KEY_SIZE 512

#if !defined(NAME)
#  define NAME "UNKN"
#endif

#define LTOH(num) (uint16_t)(((num) >> 8) | ((num) << 8))
#define HTOL LTOH
#define ITOIP(out_str, ip) sprintf(out_str, "%d.%d.%d.%d", (((ip) >> 0) & 0xFF), \
                                                           (((ip) >> 8) & 0xFF), \
                                                           (((ip) >> 16) & 0xFF), \
                                                           (((ip) >> 24) & 0xFF));
#define LOGI(fmt, ...) fprintf(stderr, "[%s][%5u] [INFO][%s:%d] " fmt "\n", NAME, HTOL(g_self.local_port), __FILE__, __LINE__, ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "[%s][%5u][ERROR][%s:%d] " fmt "\n", NAME, HTOL(g_self.local_port), __FILE__, __LINE__, ##__VA_ARGS__)
#define LOGF(fmt, ...) do { \
  fprintf(stderr, "[%s][%5u][ERROR][%s:%d] " fmt "\n", NAME, HTOL(g_self.local_port), __FILE__, __LINE__, ##__VA_ARGS__); \
  exit(1); \
} while (0)
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
#define msg_stp_newp 3
#define msg_ptp_punch 4
#define msg_ptp_punch_ack 5
#define msg_ptp_dice_throw 6

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
} while (0)

#define buff_push_arr(B, ARR, LEN) do { \
  size_t write_size = sizeof((ARR)[0]) * (LEN); \
  buff_push_a(B, write_size, ARR); \
} while (0)

#define buff_push_br_arr(B, ARR) do { \
  buff_push(B, (ARR).len); \
  buff_push_arr(B, (ARR).arr, (ARR).len); \
} while (0)

#define buff_read(B, V) do { \
  if (sizeof((V)) + (B).read_index > (B).len) { \
    LOGE("%zu < %zu", sizeof((V)) + (B).read_index, (B).len); \
    exit(1); \
  } \
  assert(sizeof((V)) + (B).read_index <= (B).len); \
  memcpy(&(V), &(B).buff->data_buffer[(B).read_index], sizeof((V))); \
  (B).read_index += sizeof((V)); \
} while (0);

#define buff_read_arr(B, ARR, LEN) do { \
  size_t read_len = sizeof((ARR)[0]) * (LEN); \
  if (read_len + (B).read_index > (B).len) { \
    LOGE("%zu < %zu", read_len + (B).read_index, (B).len); \
    exit(1); \
  } \
  memcpy((ARR), &(B).buff->data_buffer[(B).read_index], read_len); \
  (B).read_index += read_len; \
} while(0)

#define buff_read_br_arr(B, ARR) do { \
  buff_read(B, (ARR).len); \
  LOGI("Read ARR sizeof(%zu)[%d]", sizeof((ARR).arr[0]), (int)(ARR).len); \
  if ((ARR).len == 0) break; \
  (ARR).arr = malloc(sizeof((ARR).arr[0]) * (ARR).len); \
  (ARR).cap = (ARR).len; \
  buff_read_arr(B, (ARR).arr, (ARR).len); \
} while (0)

#define DATA_BUFFER_SIZE 1024

typedef uint8_t msg_kind_t;
typedef uint8_t byte;

typedef struct {
  uint16_t whole_size;
  uint16_t data_crc;
} header_t;

typedef struct {
  union {
    byte whole_buffer[DATA_BUFFER_SIZE + sizeof(header_t)];
    struct {
      header_t header;
      byte data_buffer[DATA_BUFFER_SIZE];
    };
  };
} buffer;

typedef struct {
  uint16_t enc_size;
} enc_header_t;

typedef struct {
  union {
    byte whole_enc_buffer[DATA_BUFFER_SIZE + sizeof(header_t) + sizeof(enc_header_t)];
    struct {
      enc_header_t header;
      buffer buff;
    };
  };
} enc_buffer;

typedef struct {
  buffer* buff;
  size_t len, cap;

  size_t read_index;
} buffer_t;

typedef struct {
  uint32_t* arr;
  uint8_t len, cap;
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

  endpoint_t active_ep;
  void* data;
  int dice_throw;

  EVP_PKEY_CTX* cntx;
  EVP_PKEY* key;
  unsigned char public_key[KEY_SIZE/8];
} peer_t;

peer_t g_self = { 0 };

struct {
  peer_t* arr;
  size_t len, cap;
} remotes = { 0 };

static bool find_peer(endpoint_t ep, peer_t** out_peer) {
  for (size_t i = 0; i < remotes.len; ++i) {
    if (EPEQ(remotes.arr[i].active_ep, ep)) {
      *out_peer = &remotes.arr[i];
      return true;
    }
  }

  for (size_t i = 0; i < remotes.len; ++i) {
    for (int j = 0; j < remotes.arr[i].local_ips.len; ++j) {
      endpoint_t other = { .ip = remotes.arr[i].local_ips.arr[j], .port = remotes.arr[i].local_port };
      if (EPEQ(other, ep)) {
        *out_peer = &remotes.arr[i];
        return true;
      }
    }
  }

  return false;
}

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
  LOGI("Bind success: %u", HTOL(addr.sin_port));
  return addr.sin_port;
}

static void buff_send_to(buffer_t buff, peer_t self, endpoint_t endpoint) {
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = endpoint.ip,
    .sin_port = endpoint.port
  };
  size_t send_size = buff.len + sizeof(buff.buff->header);
  buff.buff->header.whole_size = buff.len;
  if (-1 == sendto(self.fd, buff.buff->whole_buffer, send_size, 0, (void*)&addr, sizeof(addr))) {
    LOGE("Failed to send: %s", strerror(errno));
    exit(1);
  }
  LOGI("Send %zu bytes to " EP_FMT, send_size, EPTOF(endpoint));
}

static void buff_move_to(buffer_t* buff, peer_t self, endpoint_t endpoint) {
  buff_send_to(*buff, self, endpoint);
  buff->len = 0;
  buff->read_index = 0;
}

static void buff_decrypt(enc_buffer const* buff_read, buffer_t* write_buff, EVP_PKEY* key) {
  size_t data_len = KEY_SIZE / 8;
  EVP_PKEY_CTX *ctx = NULL;
  size_t out_buff_len = buff_read->header.enc_size;

  ctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
  EVP_PKEY_decrypt_init_ex(ctx, NULL);
  EVP_PKEY_decrypt(ctx, write_buff->buff->data_buffer, &out_buff_len, buff_read->buff.data_buffer, buff_read->header.enc_size);
}

enc_buffer encrypted_recv_buff;
static void buff_recv_enc(buffer_t* buff, peer_t self, endpoint_t* ep, peer_t** out_peer) {
  assert(buff->len == 0);
  struct sockaddr_in addr = { .sin_family = AF_INET };
  uint32_t addr_size = sizeof(addr);
  ssize_t s = recvfrom(self.fd, encrypted_recv_buff.whole_enc_buffer, sizeof(encrypted_recv_buff.whole_enc_buffer), 0, (void*)&addr, &addr_size);
  if (s == -1) LOGF("Failed to read from udp socket: %s", strerror(errno));

  size_t expected_size = encrypted_recv_buff.header.enc_size + sizeof(header_t) + sizeof(enc_header_t);
  if (s != expected_size) LOGF("Expected %zd == %zu", s, buff->buff->header.whole_size + sizeof(buff->buff->header));
  ep->ip = addr.sin_addr.s_addr;
  ep->port = addr.sin_port;
  if (false == find_peer(*ep, out_peer)) LOGF("Failed to find endpoint " EP_FMT, EPTOF(*ep));
  buff_decrypt(&encrypted_recv_buff, buff, (*out_peer)->key);
  buff->len += buff->buff->header.whole_size;
  LOGI("rcv from: " EP_FMT " buff.len = %zu", EPTOF(*ep), buff->len);
}

static void buff_recv(buffer_t* buff, peer_t self, endpoint_t* ep) {
  assert(buff->len == 0);
  struct sockaddr_in addr = { .sin_family = AF_INET };
  uint32_t addr_size = sizeof(addr);
  ssize_t s = recvfrom(self.fd, buff->buff->whole_buffer, sizeof(buff->buff->whole_buffer), 0, (void*)&addr, &addr_size);
  if (s == -1) {
    LOGE("Failed to read from udp socket: %s", strerror(errno));
    exit(1);
  }
  if (s != buff->buff->header.whole_size + sizeof(buff->buff->header)) {
    LOGE("Expected %zd == %zu", s, buff->buff->header.whole_size + sizeof(buff->buff->header));
    exit(1);
  }
  buff->len += buff->buff->header.whole_size;
  ep->ip = addr.sin_addr.s_addr;
  ep->port = addr.sin_port;
  LOGI("rcv from: " EP_FMT " buff.len = %zu", EPTOF(*ep), buff->len);
}

static void generate_rsa(peer_t* self) {
  if (NULL == (self->cntx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL))) { LOGE("Failed to create context"); goto error; }
  if (0 >= EVP_PKEY_keygen_init(self->cntx))                          { LOGE("Failed to init keygen"); goto error; }
  if (0 >= EVP_PKEY_CTX_set_rsa_keygen_bits(self->cntx, KEY_SIZE))    { LOGE("Failed to set rsa bits"); goto error; }
  if (0 >= EVP_PKEY_generate(self->cntx, &self->key))                 { LOGE("Failed to generate rsa keys"); goto error; }
  BIGNUM* n;
  EVP_PKEY_get_bn_param(self->key, OSSL_PKEY_PARAM_RSA_N, &n);
  BN_bn2bin(n, self->public_key);
  BN_free(n);
  return;

error:;
  unsigned long err = ERR_get_error();
  LOGF("erro %lu: %s\n", err, ERR_reason_error_string(err));
}

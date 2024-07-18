// This should be started on the server and
// this will listen on port 42069 and send
// all remote addresses to all remote addresses
// that have send the scret message.
#include "common.h"

static int remotes_index;

int main(void) {
  struct sockaddr_in remote_addr = { 0 };
  uint32_t remote_addr_size = sizeof(remote_addr);

  udp_listen(&fd, 42069);
  while (1) {
    ssize_t s = recvfrom(fd, buff, sizeof(buff), 0, (void*)&remote_addr, &remote_addr_size);
    if (s == -1) {
      LOGE("Failed to read from udp socket: %s", strerror(errno));
      exit(1);
    }
    if (memcmp(buff, secret_msg, strlen(secret_msg)) == 0) {
      for (int i = 0; i < REMOTES_N; ++i) {
        if (IPEQ(remotes[i], remote_addr)) goto snd; // Don't enter the same remote twice...
      }
      remotes[remotes_index++] = remote_addr;
      remotes_index %= REMOTES_N;
snd:
      for (int i = 0; i < REMOTES_N; ++i) {
        if (remotes[i].sin_family == AF_INET) {
          // Don't send msgs to self...
          remotes[i].sin_family = 0;
          if (-1 == sendto(fd, remotes, sizeof(remotes), 0, (void*)&remotes[i], sizeof(remotes[i]))) {
            LOGI("Error sending to remote" ADDR_FMT " %s", ATOF(remotes[i]), strerror(errno));
          }
          remotes[i].sin_family = AF_INET;
        }
      }
      LOGI("Remote inserted");
    } else {
      LOGI("Remote not inserted");
    }
    LOGI("Recived: %zd bytes: `%.*s` from remote port: " ADDR_FMT, s, (int)s, buff, ATOF(remote_addr));
  }
}

// gcc -o stun stun_udp.c && ./stun
// scp stun_udp.c common.h root@10.169.0.1:/root && ssh root@10.169.0.1 "gcc stun_udp.c && ./a.out"

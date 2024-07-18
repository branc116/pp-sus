// This should be started on a client
// and this should communicate with a server that is
// on port 42069 and when it gets a msg from the server
// with other remotes, it will start sending msgs to all
// other remotes. ( and reciving msgs from other clients )
#include "common.h"

#include "unistd.h"
#include "pthread.h"

void* _udp_write_loop(void* null) {
  static char song[][16] = { "My name is", "What", "my name is", "Who", "whaka whaka", "slim shady" };
  static int index = 0;

  while (1) {
    for (int i = 0; i < REMOTES_N; ++i) if (remotes[i].sin_family == AF_INET)
      if (-1 == sendto(fd, song[index], sizeof(song[index]), 0, (void*)&remotes[i], sizeof(remotes[i]))) {
        LOGI("Failed to send to " ADDR_FMT " %s", ATOF(remotes[i]), strerror(errno));
      }
    sleep(1);
    index++;
    index = index % 6;
  }
}

void udp_write_loop(void) {
  pthread_create(&(pthread_t){0}, NULL, _udp_write_loop, NULL);
}

int main(void) {
  struct sockaddr_in stun_remote = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = IPTOI(194, 36, 45, 54),
    .sin_port = LTOH(42069)
  };

  udp_listen(&fd, 0); // Port 0 -> Let the os decide and give oneself an efemeral port.
  udp_write_loop();
  if (sendto(fd, secret_msg, strlen(secret_msg), 0, (void*)&stun_remote, sizeof(stun_remote)) == -1) {
    LOGE("Faile to send: %s", strerror(errno));
    exit(1);
  }

  while(1) {
    struct sockaddr_in remote_addr = { 0 };
    uint32_t remote_addr_size = sizeof(remote_addr);
    ssize_t s = recvfrom(fd, buff, sizeof(buff), 0, (void*)&remote_addr, &remote_addr_size);
    if (s == -1) {
      LOGE("Failed to read from udp socket: %s", strerror(errno));
      exit(1);
    }
    if (IPEQ(remote_addr, stun_remote)) {
      if (s == sizeof(remotes)) {
        memcpy(remotes, buff, sizeof(remotes));
        LOGI("Got remotes");
      } else {
        LOGE("Bad message size from the stun %zd != %zu", s, sizeof(remotes));
        LOGE("Got: %.*s", (int)s, buff);
        exit(1);
      }
    } else {
      LOGI("[" ADDR_FMT "] %.*s", ATOF(remote_addr), (int)s, buff);
    }
  }
}

// gcc -o p2p p2p_udp.c && ./p2p
// gcc -o p2p p2p_udp.c && scp p2p_udp.c common.h root@10.169.0.5:/root && ./p2p

#include "stdio.h"
#include "ifaddrs.h"
#include "netinet/in.h"
#include "common.h"

int main(void) {
  struct ifaddrs *ifs = NULL;
  int n = getifaddrs(&ifs);
  printf("%d interfaces\n", n);
  for (struct ifaddrs* cur = ifs; cur; cur = cur->ifa_next) {
    struct sockaddr_in* sock = (void*)cur->ifa_addr;
    struct sockaddr_in* nm = (void*)cur->ifa_netmask;

    if (sock != NULL) {
      if (nm == NULL) {
        printf("name = %s, ADDR = " ADDR_FMT "\n", cur->ifa_name, ATOF(*sock));
      } else {
        printf("name = %s, ADDR = " ADDR_FMT ", MASK = " ADDR_FMT "\n", cur->ifa_name, ATOF(*sock), ATOF(*nm));
      }
    }
  }
  return 0;
}

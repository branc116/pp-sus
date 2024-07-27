killall --verbose stun; killall --verbose p2p; \
  (gcc -ggdb -fsanitize=address -o p2p p2p_udp.c -lcrypto -lssl && (./p2p & ./p2p) || killall --verbose stun) &
  (gcc -ggdb -fsanitize=address -o stun stun_udp.c -lcrypto -lssl && ./stun || killall --verbose p2p)

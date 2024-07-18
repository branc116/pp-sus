# P2P clinet and stun server

Just a demo p2p clinet and stun server for a referent.
This should just work.

## How to start everything
* Compile stun_udp.c: `gcc stun_udp.c`
* Put the output on your server
* Note the public ip of your server
* Paste the public ip of your server to ``p2p_upd.c:33``
* Compile the client: `gcc p2p_udp.c`
* Put the output to all your pc-s.


## What everything will do...

* All pc-s should first comunicate 1 message with the stun server.
* Stun server should send the ip addresses and ports to all other clinets. ( *REMOTES* )
* All client should get the list of *REMOTES*
* All clients should start sending messages directly to all other *REMOTES*
* When you have turned on all of your clients, stun server can be turned off,
  and client should still ping pong [lyrcs](https://www.youtube.com/watch?v=sNPnbI1arSE) around.

## LICENSE

MIT



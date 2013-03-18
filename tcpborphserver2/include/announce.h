#ifndef ANNOUNCE_H_
#define ANNOUNCE_H_

int run_announce_katcp(struct katcp_dispatch *d,char *host, int port);
int setup_announce(struct katcp_dispatch *d);

#endif

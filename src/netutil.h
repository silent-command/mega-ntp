/* Bringing mega-net up, and the three questions this client asks of it:
 * an address, a name, the time. Every wait is bounded on $D7FA. */
#ifndef NETUTIL_H
#define NETUTIL_H

#include <stdint.h>
#include "meganet.h"

/* Frames counted by net_poll(), for callers' timeouts. */
extern unsigned int net_frames;

/* 1 once mega-net is loaded and INIT has run; before that nothing may
 * call it. */
extern unsigned char net_ready;

/* Polls mega-net once it is up; counts frames either way. */
void net_poll(void);

/* Quiets the controller, loads MEGANET from the boot disk, runs INIT.
 * Returns 0 with a message in *err. */
unsigned char net_load(const char **err);

/* A DHCP lease, unless one is already held. Returns 0 with a message. */
unsigned char net_dhcp(const char **err);

/* Dotted quad or DNS. Returns 0 with a message in *err. */
unsigned char net_resolve(const char *host, unsigned char *ip, const char **err);

/* One SNTP exchange with `ip`. Returns 0 with a message in *err. */
unsigned char net_ntp(const unsigned char *ip, const char **err);

/* The last exchange's time as a date, shifted by `offset_min`. */
void net_ntp_time(int16_t offset_min, meganet_time_t *t);

#endif

/* The settings, in NTP.CFG on the program disk: a magic line, the time
 * server, the standard UTC offset as "+HH:MM", the daylight-saving rule
 * and the city chosen ("NTP2"). The first version's file, "NTP1", has
 * the offset alone and is still read. A missing or foreign file means
 * the defaults, and the client asks where it is before it syncs. */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CFG_SERVER_LEN 63
#define CFG_DEFAULT_SERVER "pool.ntp.org"

extern char cfg_server[CFG_SERVER_LEN + 1];
extern int16_t cfg_offset;                /* standard time, minutes east of UTC */
extern uint8_t cfg_rule;                  /* TC_DST_*, daylight saving on top of it */
#define CFG_CITY_LEN 16
extern char cfg_city[CFG_CITY_LEN + 1];   /* the city chosen, or empty for an offset set by hand */

/* 0 or 1, unit 8 or 9. 1 when NTP.CFG was there: the location is known. */
unsigned char cfg_load(unsigned char drive);
unsigned char cfg_save(void);            /* 1 on success, to the drive loaded from */

#endif

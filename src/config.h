/* The settings, in NTP.CFG on the program disk: a magic line, the time
 * server, the UTC offset as "+HH:MM". A missing or foreign file means
 * the defaults. */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CFG_SERVER_LEN 63
#define CFG_DEFAULT_SERVER "pool.ntp.org"

extern char cfg_server[CFG_SERVER_LEN + 1];
extern int16_t cfg_offset;                /* minutes east of UTC */

void cfg_load(unsigned char drive);      /* 0 or 1, unit 8 or 9 */
unsigned char cfg_save(void);            /* 1 on success, to the drive loaded from */

#endif

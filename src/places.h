/* The location list: ZONES on the program disk, made from the IANA time
 * zone database by tools/make_zones.py. Each line is "region|city|
 * standard offset|daylight-saving rule"; the text stays in bank 1 and is
 * read a field at a time. A disk without ZONES has no list, and the
 * offset is typed by hand. */
#ifndef PLACES_H
#define PLACES_H

#include <stdint.h>

#define PLACE_NAME_LEN 16              /* a city's name, without the NUL */
#define PLACE_REGION_LEN 28

extern uint8_t places_count;           /* cities in the list, 0 when there is none */
extern uint8_t regions_count;

uint8_t places_load(unsigned char drive);   /* 1 when ZONES was read */

/* Region r's name into out (PLACE_REGION_LEN + 1). */
void region_name(uint8_t r, char *out);
/* The cities of region r are first..first+count-1. */
void region_cities(uint8_t r, uint8_t *first, uint8_t *count);

/* City i: its name (PLACE_NAME_LEN + 1), standard offset and rule. */
void place_get(uint8_t i, char *name, int16_t *offset, uint8_t *rule);

#endif

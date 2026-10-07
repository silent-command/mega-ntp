/* The host check for src/places.c: the real assets/zones.txt loaded the
 * way the client loads it, every city read back, and the offset each
 * would set at the UTC moment given on the command line printed for
 * comparison with the time-zone database (tools/check_places.py). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "timecalc.h"
#include "places.h"

static unsigned char far_mem[0x10000];
unsigned char lpeek(unsigned long a) { return far_mem[a - 0x12000UL]; }
unsigned long cbmdos_load(const char *name, unsigned char drive, unsigned long dest, unsigned long maxlen)
{
  FILE *f = fopen("assets/zones.txt", "rb");
  size_t n;
  (void)name; (void)drive;
  if (!f) return 0;
  n = fread(far_mem + (dest - 0x12000UL), 1, maxlen, f);
  fclose(f);
  return n;
}

int main(int argc, char **argv)
{
  uint8_t r, i, first, count, rule;
  int16_t off;
  char name[PLACE_REGION_LEN + 1], city[PLACE_NAME_LEN + 1], t[8];
  tc_date utc = { 2026, 10, 7, 12, 0, 0, 0 };
  if (argc == 7) { utc.year = (uint16_t)atoi(argv[1]); utc.month = (uint8_t)atoi(argv[2]); utc.day = (uint8_t)atoi(argv[3]);
                   utc.hour = (uint8_t)atoi(argv[4]); utc.minute = (uint8_t)atoi(argv[5]); utc.second = (uint8_t)atoi(argv[6]); }
  if (!places_load(0)) { printf("FAIL: zones.txt did not load\n"); return 1; }
  fprintf(stderr, "%u cities in %u regions\n", places_count, regions_count);
  for (r = 0; r < regions_count; r++) {
    region_name(r, name);
    region_cities(r, &first, &count);
    for (i = 0; i < count; i++) {
      place_get((uint8_t)(first + i), city, &off, &rule);
      off = (int16_t)(off + (tc_dst_in_effect(rule, off, &utc) ? 60 : 0));
      tc_format_offset(t, off);
      printf("%s|%s|%s\n", name, city, t);
    }
  }
  return 0;
}

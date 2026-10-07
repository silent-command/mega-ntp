#include "mega65/memory.h"
#include "m65_cbmdos.h"
#include "timecalc.h"
#include "places.h"

#define PLACES_FILE "ZONES"
#define PLACES_MAGIC "ZONES1"
#define PLACES_FAR 0x12000UL           /* bank 1, above the font copy at $11000-$117FF */
#define PLACES_CAP 8192
#define PLACES_MAX 200
#define REGIONS_MAX 16

uint8_t places_count, regions_count;
static uint16_t text_len;
static uint16_t line_at[PLACES_MAX];   /* where each city's line starts */
static uint8_t region_first[REGIONS_MAX + 1];

static char at(uint16_t i) { return i < text_len ? (char)lpeek(PLACES_FAR + i) : '\n'; }

/* The field starting at *p into out (cap counts the NUL); *p moves past
 * its '|', or stays on the line's end. */
static void field(uint16_t *p, char *out, uint8_t cap)
{
  uint8_t n = 0;
  char c;
  while ((c = at(*p)) != '|' && c != '\n' && c != '\r') { if (n < cap - 1) out[n++] = c; (*p)++; }
  out[n] = 0;
  if (c == '|') (*p)++;
}

static uint8_t same_text(const char *a, const char *b)
{
  while (*a && *a == *b) { a++; b++; }
  return (uint8_t)(*a == *b);
}

uint8_t places_load(unsigned char drive)
{
  char magic[8], reg[PLACE_REGION_LEN + 1], last[PLACE_REGION_LEN + 1];
  uint16_t p = 0;

  places_count = regions_count = 0;
  last[0] = 0;
  text_len = (uint16_t)cbmdos_load(PLACES_FILE, drive, PLACES_FAR, PLACES_CAP);
  if (!text_len) return 0;
  field(&p, magic, sizeof magic);
  if (!same_text(magic, PLACES_MAGIC)) { text_len = 0; return 0; }
  for (;;) {
    while (p < text_len && (at(p) == '\n' || at(p) == '\r')) p++;   /* to the next line */
    if (p >= text_len || places_count >= PLACES_MAX) break;
    line_at[places_count] = p;
    field(&p, reg, sizeof reg);
    if (!same_text(reg, last) && regions_count < REGIONS_MAX) {
      region_first[regions_count++] = places_count;
      { uint8_t i; for (i = 0; (last[i] = reg[i]) != 0; i++) ; }
    }
    places_count++;
    while (p < text_len && at(p) != '\n') p++;
  }
  region_first[regions_count] = places_count;
  return (uint8_t)(places_count != 0);
}

void region_name(uint8_t r, char *out)
{
  uint16_t p = line_at[region_first[r]];
  field(&p, out, PLACE_REGION_LEN + 1);
}

void region_cities(uint8_t r, uint8_t *first, uint8_t *count)
{
  *first = region_first[r];
  *count = (uint8_t)(region_first[r + 1] - region_first[r]);
}

void place_get(uint8_t i, char *name, int16_t *offset, uint8_t *rule)
{
  char t[8];
  uint16_t p = line_at[i];
  uint8_t r;
  field(&p, name, PLACE_NAME_LEN + 1);          /* the region, skipped */
  field(&p, name, PLACE_NAME_LEN + 1);
  field(&p, t, sizeof t);
  if (!tc_parse_offset(t, offset)) *offset = 0;
  field(&p, t, sizeof t);
  r = tc_rule_parse(t);
  *rule = r == 0xff ? TC_DST_NONE : r;
}

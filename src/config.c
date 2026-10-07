#include "m65_cbmdos.h"
#include "timecalc.h"
#include "config.h"

#define CFG_FILE "NTP.CFG"
#define CFG_MAGIC "NTP2"
#define CFG_MAGIC_OLD "NTP1"                       /* the offset alone */
#define CFG_TEXT_LEN 128

char cfg_server[CFG_SERVER_LEN + 1];
int16_t cfg_offset;
uint8_t cfg_rule;
char cfg_city[CFG_CITY_LEN + 1];
static unsigned char cfg_drive;
static char text[CFG_TEXT_LEN];

static void copy(char *to, const char *from, unsigned char cap)
{
  unsigned char n = 0;
  while (from[n] && n < cap - 1) { to[n] = from[n]; n++; }
  to[n] = 0;
}

static unsigned char same(const char *a, const char *b)
{
  while (*a && *a == *b) { a++; b++; }
  return *a == *b;
}

/* The line at text[*pos] into `out` (cap counts the NUL); *pos moves past
 * its newline. */
static void take_line(unsigned char *pos, unsigned char len, char *out, unsigned char cap)
{
  unsigned char n = 0, p = *pos;
  while (p < len && text[p] != '\n') { if (n < cap - 1) out[n++] = text[p]; p++; }
  out[n] = 0;
  if (p < len) p++;
  *pos = p;
}

unsigned char cfg_load(unsigned char drive)
{
  char field[CFG_SERVER_LEN + 1];
  unsigned char len, pos = 0, v2;
  int16_t v;
  uint8_t r;

  cfg_drive = drive;
  copy(cfg_server, CFG_DEFAULT_SERVER, sizeof cfg_server);
  cfg_offset = 0; cfg_rule = TC_DST_NONE; cfg_city[0] = 0;
  len = (unsigned char)cbmdos_load(CFG_FILE, drive, (unsigned long)(unsigned int)text, CFG_TEXT_LEN);
  if (!len) return 0;
  take_line(&pos, len, field, sizeof field);
  v2 = same(field, CFG_MAGIC);
  if (!v2 && !same(field, CFG_MAGIC_OLD)) return 0; /* not ours, or newer: ignored, not guessed */
  take_line(&pos, len, field, sizeof field);
  if (field[0]) copy(cfg_server, field, sizeof cfg_server);
  take_line(&pos, len, field, sizeof field);
  if (tc_parse_offset(field, &v)) cfg_offset = v;
  if (v2) {
    take_line(&pos, len, field, sizeof field);
    r = tc_rule_parse(field);
    if (r != 0xff) cfg_rule = r;
    take_line(&pos, len, field, sizeof field);
    copy(cfg_city, field, sizeof cfg_city);
  }
  return 1;
}

static unsigned char put_text(const char *s)
{
  while (*s)
    if (cbmdos_put((unsigned char)*s++) != CBMDOS_OK) return 0;
  return 1;
}

unsigned char cfg_save(void)
{
  char t[8];
  unsigned char ok;

  tc_format_offset(t, cfg_offset);
  cbmdos_delete(CFG_FILE, cfg_drive);              /* absent the first time; fine */
  if (cbmdos_create(CFG_FILE, cfg_drive) != CBMDOS_OK) return 0;
  ok = (unsigned char)(put_text(CFG_MAGIC "\n") && put_text(cfg_server) && put_text("\n") &&
                       put_text(t) && put_text("\n") && put_text(tc_rule_name(cfg_rule)) && put_text("\n") &&
                       put_text(cfg_city) && put_text("\n"));
  return (unsigned char)(cbmdos_close() == CBMDOS_OK && ok);
}

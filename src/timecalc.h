/* The client's arithmetic, apart from the machine: UTC offsets as typed,
 * dates as the real-time clock holds them, and how far apart two of them
 * are. C99 and stdint.h only, so tests/test_timecalc.c runs it on the
 * host. */
#ifndef TIMECALC_H
#define TIMECALC_H

#include <stdint.h>

#define TC_OFFSET_MIN (-720)          /* UTC-12:00 */
#define TC_OFFSET_MAX 840             /* UTC+14:00 */

/* weekday 0 = Sunday, mega-net's and mega65-libc's convention */
typedef struct {
  uint16_t year;
  uint8_t month, day, hour, minute, second, weekday;
} tc_date;

/* "-4", "+5:30", "5:45", "utc+2", "0" or "" (UTC) to minutes. Returns 0,
 * leaving *minutes alone, for anything else or outside -12:00..+14:00. */
uint8_t tc_parse_offset(const char *s, int16_t *minutes);

/* Minutes to "+05:30"; 7 bytes. */
void tc_format_offset(char *out, int16_t minutes);

/* "2026-09-14 18:32:05"; 20 bytes. */
void tc_format_date(char *out, const tc_date *d);

const char *tc_weekday_name(uint8_t weekday);

/* 1 for a real date from 2000-01-01 00:00:00 to 2099-12-31 23:59:59, the
 * range the clock's two-digit year holds. */
uint8_t tc_valid(const tc_date *d);

/* Seconds since 2000-01-01 00:00:00, for a valid date. */
uint32_t tc_seconds(const tc_date *d);

/* 0 = Sunday, from the date alone. */
uint8_t tc_weekday(uint16_t year, uint8_t month, uint8_t day);

/* Daylight saving: the rules in use where the location list has cities
 * that keep it (2026). Each moves the clock one hour forward between two
 * transitions a year. TC_DST_NONE is every place that stays on one
 * offset all year. */
#define TC_DST_NONE 0
#define TC_DST_US 1      /* US and Canada: second Sunday of March to first Sunday of November, 02:00 local */
#define TC_DST_EU 2      /* Europe: last Sunday of March to last Sunday of October, 01:00 UTC */
#define TC_DST_AU 3      /* south-east Australia: first Sunday of October to first Sunday of April */
#define TC_DST_NZ 4      /* New Zealand: last Sunday of September to first Sunday of April */
#define TC_DST_CL 5      /* Chile: first Sunday of September 04:00 UTC to first Sunday of April 03:00 UTC */
#define TC_DST_IL 6      /* Israel: the Friday before the last Sunday of March to the last Sunday of October */
#define TC_DST_EG 7      /* Egypt: last Friday of April to the end of the last Thursday of October */
#define TC_DST_RULES 8

/* The rule named as the location file writes it ("NONE", "US", "EU" ...),
 * or 0xff; and its names, short for the file and long for the screen. */
uint8_t tc_rule_parse(const char *name);
const char *tc_rule_name(uint8_t rule);
const char *tc_rule_text(uint8_t rule);

/* 1 when daylight saving is in effect at the UTC moment `utc` in a place
 * on `std_minutes` east of UTC that keeps `rule`. */
uint8_t tc_dst_in_effect(uint8_t rule, int16_t std_minutes, const tc_date *utc);

/* How far `clock` is from `truth`, both from tc_seconds: "3 seconds slow",
 * "5m07s fast", "1h02m03s slow", "2 days fast", or "right to within a
 * second". 32 bytes. */
void tc_format_drift(char *out, uint32_t truth, uint32_t clock);

#endif

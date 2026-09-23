#include "timecalc.h"

static const uint8_t mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const char *const names[7] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

/* Every fourth year, which is the Gregorian rule for 2000-2099: 2000 is a
 * leap year by the 400-year rule and 2100 is outside the range. */
static uint8_t leap(uint16_t y) { return (uint8_t)(y % 4 == 0); }

static uint8_t month_days(uint16_t y, uint8_t m)
{
  return (uint8_t)(m == 2 && leap(y) ? 29 : mdays[m - 1]);
}

static void two(char *p, uint8_t v)
{
  p[0] = (char)('0' + v / 10);
  p[1] = (char)('0' + v % 10);
}

static char *put_s(char *p, const char *s)
{
  while (*s) *p++ = *s++;
  return p;
}

static char *put_u32(char *p, uint32_t v)
{
  char t[10];
  uint8_t n = 0;
  do { t[n++] = (char)('0' + (uint8_t)(v % 10)); v /= 10; } while (v);
  while (n) *p++ = t[--n];
  return p;
}

/* Up to two digits at s into *value; returns past them, or 0 for three. */
static const char *take_digits(const char *s, uint8_t *digits, uint16_t *value)
{
  *digits = 0; *value = 0;
  while (*s >= '0' && *s <= '9') {
    if (++*digits > 2) return 0;
    *value = (uint16_t)(*value * 10 + (uint16_t)(*s - '0'));
    s++;
  }
  return s;
}

uint8_t tc_parse_offset(const char *s, int16_t *minutes)
{
  uint8_t neg = 0, digits;
  uint16_t h, m = 0;
  int16_t v;

  while (*s == ' ') s++;
  if ((s[0] == 'u' || s[0] == 'U') && (s[1] == 't' || s[1] == 'T') && (s[2] == 'c' || s[2] == 'C')) s += 3;
  while (*s == ' ') s++;
  if (!*s) { *minutes = 0; return 1; }
  if (*s == '+' || *s == '-') { neg = (uint8_t)(*s == '-'); s++; }
  s = take_digits(s, &digits, &h);
  if (!s || !digits) return 0;
  if (*s == ':') {
    s = take_digits(s + 1, &digits, &m);
    if (!s || digits != 2 || m > 59) return 0;
  }
  while (*s == ' ') s++;
  if (*s) return 0;
  v = (int16_t)(h * 60 + m);
  if (neg) v = (int16_t)-v;
  if (v < TC_OFFSET_MIN || v > TC_OFFSET_MAX) return 0;
  *minutes = v;
  return 1;
}

void tc_format_offset(char *out, int16_t minutes)
{
  uint16_t a = (uint16_t)(minutes < 0 ? -minutes : minutes);
  out[0] = minutes < 0 ? '-' : '+';
  two(out + 1, (uint8_t)(a / 60));
  out[3] = ':';
  two(out + 4, (uint8_t)(a % 60));
  out[6] = 0;
}

void tc_format_date(char *out, const tc_date *d)
{
  two(out, (uint8_t)(d->year / 100));
  two(out + 2, (uint8_t)(d->year % 100));
  out[4] = '-'; two(out + 5, d->month);
  out[7] = '-'; two(out + 8, d->day);
  out[10] = ' '; two(out + 11, d->hour);
  out[13] = ':'; two(out + 14, d->minute);
  out[16] = ':'; two(out + 17, d->second);
  out[19] = 0;
}

const char *tc_weekday_name(uint8_t weekday)
{
  return weekday < 7 ? names[weekday] : "?";
}

uint8_t tc_valid(const tc_date *d)
{
  return (uint8_t)(d->year >= 2000 && d->year <= 2099 && d->month >= 1 && d->month <= 12 &&
                   d->day >= 1 && d->day <= month_days(d->year, d->month) &&
                   d->hour < 24 && d->minute < 60 && d->second < 60);
}

/* Days since 2000-01-01; at most 36524, so 16 bits. */
static uint16_t day_number(uint16_t y, uint8_t m, uint8_t d)
{
  uint16_t n = (uint16_t)((y - 2000) * 365 + (y - 2000 + 3) / 4);
  uint8_t i;
  for (i = 1; i < m; i++) n = (uint16_t)(n + month_days(y, i));
  return (uint16_t)(n + d - 1);
}

uint32_t tc_seconds(const tc_date *d)
{
  return (uint32_t)day_number(d->year, d->month, d->day) * 86400UL +
         (uint32_t)d->hour * 3600UL + (uint16_t)(d->minute * 60) + d->second;
}

uint8_t tc_weekday(uint16_t year, uint8_t month, uint8_t day)
{
  return (uint8_t)((day_number(year, month, day) + 6) % 7);   /* 2000-01-01 was a Saturday */
}

void tc_format_drift(char *out, uint32_t truth, uint32_t clock)
{
  uint32_t d;
  const char *word;
  char *p = out;
  uint16_t sod;

  if (clock >= truth) { d = clock - truth; word = " fast"; }
  else { d = truth - clock; word = " slow"; }
  /* The reply carries whole seconds and the clock reads whole seconds, so
   * a second either way is the measurement, not the clock. */
  if (d <= 1) { p = put_s(p, "right to within a second"); *p = 0; return; }
  if (d >= 86400UL) {
    p = put_u32(p, d / 86400UL);
    p = put_s(p, d >= 172800UL ? " days" : " day");
  } else if (d >= 60) {
    sod = (uint16_t)(d / 60);                      /* minutes, at most 1439 */
    if (sod >= 60) { p = put_u32(p, sod / 60); *p++ = 'h'; two(p, (uint8_t)(sod % 60)); p += 2; }
    else p = put_u32(p, sod);
    *p++ = 'm'; two(p, (uint8_t)(d % 60)); p += 2; *p++ = 's';
  } else {
    p = put_u32(p, d);
    p = put_s(p, " seconds");
  }
  p = put_s(p, word);
  *p = 0;
}

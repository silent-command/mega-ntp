/* The host suite for src/timecalc.c: python3 build.py test. */
#include <stdio.h>
#include <string.h>
#include "timecalc.h"

static int checks, failed;

#define CHECK(cond, what) do { checks++; if (!(cond)) { failed++; printf("FAIL line %d: %s\n", __LINE__, what); } } while (0)

static void offset_ok(const char *s, int16_t want)
{
  int16_t v = 12345;
  char msg[64];
  snprintf(msg, sizeof msg, "offset \"%s\" -> %d", s, want);
  CHECK(tc_parse_offset(s, &v) && v == want, msg);
}

static void offset_bad(const char *s)
{
  int16_t v = 12345;
  char msg[64];
  snprintf(msg, sizeof msg, "offset \"%s\" refused, untouched", s);
  CHECK(!tc_parse_offset(s, &v) && v == 12345, msg);
}

static void offset_text(int16_t m, const char *want)
{
  char t[8], msg[64];
  tc_format_offset(t, m);
  snprintf(msg, sizeof msg, "%d formats as %s (got %s)", m, want, t);
  CHECK(!strcmp(t, want), msg);
}

static tc_date date(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
{
  tc_date r;
  r.year = y; r.month = mo; r.day = d; r.hour = h; r.minute = mi; r.second = s; r.weekday = 0;
  return r;
}

static void drift(uint32_t truth, uint32_t clock, const char *want)
{
  char t[32], msg[96];
  tc_format_drift(t, truth, clock);
  snprintf(msg, sizeof msg, "drift %lu vs %lu: \"%s\" (got \"%s\")", (unsigned long)truth, (unsigned long)clock, want, t);
  CHECK(!strcmp(t, want), msg);
}

int main(void)
{
  tc_date d;
  char t[20];
  int16_t m, back;
  uint16_t y;
  uint8_t mo, day, wd, round_ok;
  uint32_t prev;

  /* offsets as typed */
  offset_ok("", 0);
  offset_ok("0", 0);
  offset_ok("+0", 0);
  offset_ok("utc", 0);
  offset_ok("UTC+2", 120);
  offset_ok("-4", -240);
  offset_ok("+5:30", 330);
  offset_ok("5:45", 345);
  offset_ok("-12:00", -720);
  offset_ok("+14", 840);
  offset_ok(" -3:30 ", -210);
  offset_ok("-09:30", -570);
  offset_bad("+15");
  offset_bad("-12:01");
  offset_bad("+5:3");
  offset_bad("+5:60");
  offset_bad("abc");
  offset_bad("+");
  offset_bad("123");
  offset_bad("5:");
  offset_bad("--4");
  offset_bad("4 5");

  offset_text(-240, "-04:00");
  offset_text(330, "+05:30");
  offset_text(0, "+00:00");
  offset_text(-570, "-09:30");
  offset_text(840, "+14:00");

  round_ok = 1;
  for (m = TC_OFFSET_MIN; m <= TC_OFFSET_MAX; m++) {
    char s[8];
    tc_format_offset(s, m);
    if (!tc_parse_offset(s, &back) || back != m) round_ok = 0;
  }
  CHECK(round_ok, "every offset from -12:00 to +14:00 formats and parses back");

  /* validity */
  d = date(2024, 2, 29, 0, 0, 0); CHECK(tc_valid(&d), "2024-02-29 is a date");
  d = date(2000, 2, 29, 0, 0, 0); CHECK(tc_valid(&d), "2000-02-29 is a date (the 400-year rule)");
  d = date(2023, 2, 29, 0, 0, 0); CHECK(!tc_valid(&d), "2023-02-29 is not");
  d = date(2026, 13, 1, 0, 0, 0); CHECK(!tc_valid(&d), "month 13 is not");
  d = date(2026, 0, 1, 0, 0, 0); CHECK(!tc_valid(&d), "month 0 is not");
  d = date(2026, 4, 31, 0, 0, 0); CHECK(!tc_valid(&d), "April 31 is not");
  d = date(2026, 1, 0, 0, 0, 0); CHECK(!tc_valid(&d), "day 0 is not");
  d = date(2026, 1, 1, 24, 0, 0); CHECK(!tc_valid(&d), "24:00 is not");
  d = date(2026, 1, 1, 0, 60, 0); CHECK(!tc_valid(&d), "minute 60 is not");
  d = date(2026, 1, 1, 0, 0, 60); CHECK(!tc_valid(&d), "second 60 is not");
  d = date(1999, 12, 31, 23, 59, 59); CHECK(!tc_valid(&d), "1999 is outside the clock");
  d = date(2100, 1, 1, 0, 0, 0); CHECK(!tc_valid(&d), "2100 is outside the clock");
  d = date(2099, 12, 31, 23, 59, 59); CHECK(tc_valid(&d), "2099-12-31 23:59:59 is the last");

  /* anchors */
  d = date(2000, 1, 1, 0, 0, 0); CHECK(tc_seconds(&d) == 0, "2000-01-01 is second 0");
  /* mega-net REQUIREMENTS.md 5.7: NTP 3997530584 was 2026-09-04 17:09:44
   * UTC; NTP's 2000-01-01 is 3155673600. */
  d = date(2026, 9, 4, 17, 9, 44); CHECK(tc_seconds(&d) == 3997530584UL - 3155673600UL, "2026-09-04 17:09:44 against NTP");
  CHECK(tc_weekday(2000, 1, 1) == 6, "2000-01-01 was a Saturday");
  CHECK(tc_weekday(2024, 2, 29) == 4, "2024-02-29 was a Thursday");
  CHECK(tc_weekday(2026, 9, 4) == 5, "2026-09-04 was a Friday");
  CHECK(tc_weekday(2026, 9, 14) == 1, "2026-09-14 is a Monday");
  CHECK(!strcmp(tc_weekday_name(1), "Monday") && !strcmp(tc_weekday_name(9), "?"), "weekday names");

  d = date(2026, 9, 4, 17, 9, 44);
  tc_format_date(t, &d);
  CHECK(!strcmp(t, "2026-09-04 17:09:44"), "date text");

  /* every day of the century: one day's seconds apart, the weekday
   * stepping by one, and the day after each month's last refused */
  round_ok = 1;
  prev = 0; wd = 6;
  for (y = 2000; y <= 2099; y++)
    for (mo = 1; mo <= 12; mo++)
      for (day = 1; day <= 32; day++) {
        d = date(y, mo, day, 0, 0, 0);
        if (!tc_valid(&d)) { if (day < 28) round_ok = 0; break; }
        if (!(y == 2000 && mo == 1 && day == 1)) {
          if (tc_seconds(&d) != prev + 86400UL) round_ok = 0;
          wd = (uint8_t)((wd + 1) % 7);
        }
        if (tc_weekday(y, mo, day) != wd) round_ok = 0;
        prev = tc_seconds(&d);
      }
  CHECK(round_ok, "36525 days in sequence");
  CHECK(prev == 36524UL * 86400UL, "2099-12-31 is day 36524");

  /* drift */
  drift(100, 100, "right to within a second");
  drift(100, 101, "right to within a second");
  drift(101, 100, "right to within a second");
  drift(100, 103, "3 seconds fast");
  drift(103, 100, "3 seconds slow");
  drift(0, 59, "59 seconds fast");
  drift(0, 60, "1m00s fast");
  drift(0, 307, "5m07s fast");
  drift(3723, 0, "1h02m03s slow");
  drift(0, 86399UL, "23h59m59s fast");
  drift(0, 86400UL, "1 day fast");
  drift(0, 200000UL, "2 days fast");
  drift(3000000000UL, 0, "34722 days slow");

  printf("%d checks, %d failed\n", checks, failed);
  return failed != 0;
}

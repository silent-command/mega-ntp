/* MEGA65 NTP client: asks a time server for the time over mega-net and
 * sets the real-time clock to it, in local time by the UTC offset kept
 * in NTP.CFG. It syncs once at start; RETURN syncs again. */
#include "mega65/memory.h"
#include "mega65/time.h"
#include "mega65/targets.h"
#include "meganet.h"
#include "m65_screen.h"
#include "m65_boot.h"
#include "m65_exit.h"
#include "netutil.h"
#include "config.h"
#include "timecalc.h"
#include "ui.h"

#define NTPC_VERSION "0.1.1"

#define ROW_CLOCK 2
#define ROW_SERVER 4
#define ROW_OFFSET 5
#define ROW_BOARD 6
#define ROW_ADDR 8
#define ROW_ASKED 9
#define ROW_UTC 10
#define ROW_LOCAL 11
#define ROW_BEFORE 12
#define ROW_AFTER 13

/* The clock registers at $FFD7110 are the machine's mirror of the I2C
 * chip, so a write is read back after a pause, not at once. */
#define SETTLE_FRAMES 60
#define VERIFY_TRIES 3

static char line[81];
static unsigned char at;

static void clear(void) { at = 0; line[0] = 0; }

static void add(const char *s)
{
  while (*s && at < 79) line[at++] = *s++;
  line[at] = 0;
}

static void add_num(unsigned long v)
{
  char t[11];
  ui_put_ulong(t, v);
  add(t);
}

static void add_ip(const unsigned char *ip)
{
  unsigned char i;
  for (i = 0; i < 4; i++) { if (i) add("."); add_num(ip[i]); }
}

static void add_date(const tc_date *d)
{
  char t[20];
  tc_format_date(t, d);
  add(t);
}

static void add_offset(int16_t m)
{
  char t[8];
  tc_format_offset(t, m);
  add("UTC");
  add(t);
}

/* ---- the clock -------------------------------------------------------- */

/* Board revisions whose clock mega65-libc's setrtc() knows how to write. */
static unsigned char clock_settable(void)
{
  unsigned char t = detect_target();
  return (unsigned char)(t >= TARGET_MEGA65R2 && t <= TARGET_MEGA65R6);
}

/* The library passes the chip's month (1-12) and weekday straight
 * through in both directions, whatever its header says about 0-11; the
 * year is from 1900. */
static void rtc_read(tc_date *d)
{
  struct m65_tm tm;
  getrtc(&tm);
  d->year = (uint16_t)(tm.tm_year + 1900);
  d->month = tm.tm_mon;
  d->day = tm.tm_mday;
  d->hour = tm.tm_hour;
  d->minute = tm.tm_min;
  d->second = tm.tm_sec;
  d->weekday = tm.tm_wday;
}

static void rtc_write(const tc_date *d)
{
  struct m65_tm tm;
  tm.tm_sec = d->second;
  tm.tm_min = d->minute;
  tm.tm_hour = d->hour;
  tm.tm_mday = d->day;
  tm.tm_mon = d->month;
  tm.tm_year = (unsigned short)(d->year - 1900);
  tm.tm_wday = tc_weekday(d->year, d->month, d->day);
  tm.tm_yday = 0;
  tm.tm_isdst = 0;                                 /* the offset already says what local time is */
  setrtc(&tm);
}

static unsigned char last_second = 0xff;
static unsigned char idle_frames;

static void draw_clock(unsigned char force)
{
  static char row[48];
  char t[20];
  const char *p;
  unsigned char n = 0;
  tc_date d;

  rtc_read(&d);
  if (!force && d.second == last_second) return;
  last_second = d.second;
  if (tc_valid(&d)) {
    tc_format_date(t, &d);
    for (p = t; *p; p++) row[n++] = *p;
    row[n++] = ' '; row[n++] = ' ';
    for (p = tc_weekday_name(tc_weekday(d.year, d.month, d.day)); *p; p++) row[n++] = *p;
    row[n] = 0;
    ui_line(ROW_CLOCK, "Clock:    ", row);
  } else {
    ui_line(ROW_CLOCK, "Clock:    ", "no valid date in the real-time clock");
  }
}

static void on_idle(void)
{
  if (++idle_frames < 10) return;
  idle_frames = 0;
  draw_clock(0);
}

/* ---- drawing ---------------------------------------------------------- */

static void draw_server(void)
{
  ui_line(ROW_SERVER, "Server:   ", cfg_server);
}

static void draw_offset(void)
{
  clear(); add("Offset:   "); add_offset(cfg_offset);
  add("   the clock is kept in this local time");
  ui_line(ROW_OFFSET, line, 0);
}

static void draw_board(void)
{
  unsigned char t = detect_target();
  clear(); add("Board:    ");
  if (t >= TARGET_MEGA65R1 && t <= TARGET_MEGA65R6) { add("MEGA65 R"); add_num(t); }
  else if (t == TARGET_EMULATION) add("emulator");
  else { add("target "); add_num(t); }
  if (!clock_settable()) add(", whose clock this program cannot set");
  ui_line(ROW_BOARD, line, 0);
}

static void draw_keys(void)
{
  ui_line(UI_ROW_KEYS, "RETURN sync   S server   O offset   F/B color   RUN/STOP quit", 0);
}

static void draw_all(void)
{
  ui_line(UI_ROW_TITLE, "MEGA65 NTP Client - version " NTPC_VERSION, 0);
  draw_clock(1);
  draw_server();
  draw_offset();
  draw_board();
  draw_keys();
}

/* ---- syncing ---------------------------------------------------------- */

static void to_date(const meganet_time_t *t, tc_date *d)
{
  d->year = (uint16_t)(t->year[0] | ((uint16_t)t->year[1] << 8));
  d->month = t->month;
  d->day = t->day;
  d->hour = t->hour;
  d->minute = t->minute;
  d->second = t->second;
  d->weekday = t->weekday;
}

static void wait_frames(unsigned int n)
{
  net_frames = 0;
  while (net_frames < n) net_poll();
}

static void sync(void)
{
  static meganet_ipconf_t conf;
  static meganet_time_t t;
  char drift[32];
  const char *err;
  unsigned char ip[4], attempt, tries;
  unsigned int waited = 0;
  tc_date utc, local, before, after;
  uint32_t want, got = 0;

  ui_clear_rows(ROW_ADDR, ROW_AFTER);
  if (!clock_settable()) { ui_status("this board's clock is not one this program can set", 0); return; }
  if (!net_ready) {
    ui_status("loading mega-net...", 0);
    if (!net_load(&err)) { ui_status("network: ", err); return; }
  }
  ui_status("waiting for an address...", 0);
  if (!net_dhcp(&err)) { ui_status("network: ", err); return; }
  meganet_get_ip(&conf);
  clear(); add("Address:  "); add_ip(conf.ip);
  ui_line(ROW_ADDR, line, 0);

  /* A pool name answers with another server when asked again, so a
   * silent server gets one more lookup before giving up. */
  for (attempt = 0; ; attempt++) {
    ui_status("looking up ", cfg_server);
    if (!net_resolve(cfg_server, ip, &err)) { ui_status("time server: ", err); return; }
    clear(); add("Asked:    "); add(cfg_server); add(" ("); add_ip(ip); add(")");
    ui_line(ROW_ASKED, line, 0);
    ui_status("asking for the time...", 0);
    if (net_ntp(ip, &err)) break;
    if (attempt) { ui_status("time server: ", err); return; }
  }

  net_ntp_time(0, &t);
  to_date(&t, &utc);
  net_ntp_time(cfg_offset, &t);
  to_date(&t, &local);
  if (!tc_valid(&local)) { ui_status("the server's time is outside the clock's years, 2000 to 2099", 0); return; }

  rtc_read(&before);
  rtc_write(&local);
  want = tc_seconds(&local);

  clear(); add("UTC:      "); add_date(&utc);
  ui_line(ROW_UTC, line, 0);
  clear(); add("Set to:   "); add_date(&local); add("  ");
  add(tc_weekday_name(tc_weekday(local.year, local.month, local.day)));
  add("  ("); add_offset(cfg_offset); add(")");
  ui_line(ROW_LOCAL, line, 0);
  clear(); add("Was:      ");
  if (tc_valid(&before)) {
    add_date(&before); add(", ");
    tc_format_drift(drift, want, tc_seconds(&before));
    add(drift);
  } else {
    add("no valid date");
  }
  ui_line(ROW_BEFORE, line, 0);

  ui_status("checking the clock took it...", 0);
  for (tries = 0; tries < VERIFY_TRIES; tries++) {
    wait_frames(SETTLE_FRAMES);
    waited += SETTLE_FRAMES;
    rtc_read(&after);
    if (!tc_valid(&after)) continue;
    got = tc_seconds(&after);
    /* at least what was written, less a second for the rounding, and no
     * more than the wait since (counted at 50 frames a second, the slower
     * rate, so it is an upper bound) and two seconds besides */
    if (got + 1 >= want && got <= want + waited / 50 + 2) break;
  }
  draw_clock(1);
  if (tries == VERIFY_TRIES) {
    clear(); add("Reads:    ");
    if (tc_valid(&after)) add_date(&after); else add("no valid date");
    ui_line(ROW_AFTER, line, 0);
    ui_status("the clock did not take the new time", 0);
    return;
  }
  ui_status("the clock is set", 0);
}

/* ---- settings --------------------------------------------------------- */

static void save_settings(void)
{
  ui_status(cfg_save() ? "saved in NTP.CFG; RETURN syncs with it" : "could not write NTP.CFG (write protected?)", 0);
}

static void edit_server(void)
{
  static char buf[CFG_SERVER_LEN + 1];
  unsigned char i;

  for (i = 0; (buf[i] = cfg_server[i]) != 0; i++) ;
  ui_status("a host name or an address; an empty line puts back " CFG_DEFAULT_SERVER, 0);
  if (!ui_read_line(ROW_SERVER, "Server:   ", buf, CFG_SERVER_LEN)) { draw_server(); ui_status(0, 0); return; }
  if (!buf[0]) { const char *d = CFG_DEFAULT_SERVER; for (i = 0; (buf[i] = d[i]) != 0; i++) ; }
  for (i = 0; (cfg_server[i] = buf[i]) != 0; i++) ;
  draw_server();
  save_settings();
}

static void edit_offset(void)
{
  char buf[8];
  int16_t v;

  tc_format_offset(buf, cfg_offset);
  ui_status("hours from UTC, like -5, +1 or +5:30; change it for daylight saving", 0);
  for (;;) {
    if (!ui_read_line(ROW_OFFSET, "Offset:   UTC", buf, 7)) { draw_offset(); ui_status(0, 0); return; }
    if (tc_parse_offset(buf, &v)) break;
    ui_status("not an offset: like -5, +1 or +5:30, from -12:00 to +14:00", 0);
  }
  cfg_offset = v;
  draw_offset();
  save_settings();
}

int main(void)
{
  const char *err;
  unsigned char k;

  mega65_io_enable();
  m65_own_vectors();                              /* first: a stray ethernet event before this enters the KERNAL's handler (mega-net 5.18) */
  m65_screen_init();
  draw_all();
  ui_status("loading mega-net...", 0);
  if (!net_load(&err)) ui_status("network: ", err);   /* the clock and the settings still work */
  cfg_load(boot_drive);
  draw_server();
  draw_offset();
  ui_idle = on_idle;
  if (net_ready) sync();

  for (;;) {
    k = ui_wait_key();
    if (k >= 0xc1 && k <= 0xda && (ui_last_mods & MOD_MEGA)) k = (unsigned char)(k & 0x7f);   /* MEGA+letter (ssh 5.29) */
    switch (k) {
    case KEY_RETURN: sync(); break;
    case 's': case 'S': edit_server(); break;
    case 'o': case 'O': edit_offset(); break;
    case 'f': case 'F':
      /* Every row, not only the ones draw_all() knows how to draw: the
       * sync's results and the status keep no copy of their text, so the
       * whole color RAM ($FF80000, a byte per cell, no attributes in use
       * here) takes the new color instead of a redraw. */
      m65_screen_cycle_text_colour();
      lfill(0xff80000UL, m65_screen_text_colour(), 80 * (unsigned int)m65_screen_rows());
      break;
    case 'b': case 'B': m65_screen_cycle_background(); break;
    case 'q': case 'Q': case KEY_STOP:
      m65_exit_to_basic();                         /* BASIC's READY, disk still mounted; never returns */
      break;
    default: break;
    }
  }
}

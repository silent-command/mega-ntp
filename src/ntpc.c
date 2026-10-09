/* MEGA65 NTP client: asks a time server for the time over mega-net and
 * sets the real-time clock to it, in local time: the location kept in
 * NTP.CFG gives the standard offset and the daylight-saving rule, and
 * each sync works out which is in force. On a disk without NTP.CFG it
 * asks where it is before it syncs. It syncs once at start; RETURN
 * syncs again. */
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
#include "places.h"
#include "ui.h"

#define NTPC_VERSION "0.2.2"

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

static uint8_t bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

/* One register of the R4-R6 clock, then three frames for it to reach the
 * chip. mega65-libc's setrtc writes them back to back with pauses too
 * short for this board: on 2026-10-07 the day register took the month's
 * value, 10 for 08, the first time a sync changed the date (Sydney,
 * already tomorrow); a lone write of the day took at once. */
static void rtc_put(uint8_t reg, uint8_t v)
{
  uint8_t last, n = 0;
  lpoke(0xffd7110UL + reg, v);
  last = PEEK(0xd7fa);
  while (n < 3) if (PEEK(0xd7fa) != last) { last = PEEK(0xd7fa); n++; }
}

static void rtc_write(const tc_date *d)
{
  struct m65_tm tm;
  unsigned char t = detect_target();
  if (t >= TARGET_MEGA65R4 && t <= TARGET_MEGA65R6) {
    /* the seconds to 0 first, so the chip cannot carry into the minute
     * while the slower fields go in, and the real seconds last */
    rtc_put(0, 0);
    rtc_put(1, bcd(d->minute));
    rtc_put(2, bcd(d->hour));                      /* 24-hour: this chip has no AM/PM */
    rtc_put(3, bcd(d->day));
    rtc_put(4, bcd(d->month));
    rtc_put(5, bcd((uint8_t)(d->year - 2000)));
    rtc_put(6, tc_weekday(d->year, d->month, d->day));
    rtc_put(0, bcd(d->second));
    return;
  }
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

/* "New York (UTC-05:00, US and Canada daylight saving)" */
static void draw_offset(void)
{
  clear(); add("Location: ");
  add(cfg_city[0] ? cfg_city : "set by hand");
  add(" ("); add_offset(cfg_offset); add(", "); add(tc_rule_text(cfg_rule)); add(")");
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
  ui_line(UI_ROW_KEYS, "RETURN sync   S server   L location   MEGA-F/B color   RUN/STOP quit", 0);
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
  uint8_t summer;
  int16_t offset;

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
  /* daylight saving decided from UTC itself, so a sync on the night of
   * a change is right either side of it */
  summer = tc_dst_in_effect(cfg_rule, cfg_offset, &utc);
  offset = (int16_t)(cfg_offset + (summer ? 60 : 0));
  net_ntp_time(offset, &t);
  to_date(&t, &local);
  if (!tc_valid(&local)) { ui_status("the server's time is outside the clock's years, 2000 to 2099", 0); return; }

  rtc_read(&before);
  rtc_write(&local);
  want = tc_seconds(&local);

  clear(); add("UTC:      "); add_date(&utc);
  ui_line(ROW_UTC, line, 0);
  clear(); add("Set to:   "); add_date(&local); add("  ");
  add(tc_weekday_name(tc_weekday(local.year, local.month, local.day)));
  add("  ("); add_offset(offset); if (summer) add(", daylight saving"); add(")");
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

/* ---- the location ------------------------------------------------------
 * Where the MEGA65 is: a region, then a city from the list made from the
 * time-zone database, or an offset typed by hand with its daylight-saving
 * rule chosen. Asked before the first sync on a disk without NTP.CFG,
 * since a clock set to UTC is the mistake otherwise (2026-10-07), and by
 * L afterwards. */

#define ROW_PICK_HEAD 4                   /* under the clock, which keeps ticking on row 2 */
#define PICK_FIRST 6
#define PICK_ROWS ((unsigned char)(m65_screen_rows() - 10))

static uint8_t pick_first;                /* the cities shown are pick_first.. */
static uint8_t pick_mode;                 /* what the list holds */
#define PICK_REGIONS 0
#define PICK_CITIES 1
#define PICK_RULES 2

/* Row i of the list as text in `line`. */
static void pick_text(uint8_t i)
{
  char name[PLACE_REGION_LEN + 1];
  int16_t off;
  uint8_t rule;
  clear(); add("  ");
  if (pick_mode == PICK_REGIONS) {
    if (i < regions_count) { region_name(i, name); add(name); }
    else add("None of these: set an offset by hand");
  } else if (pick_mode == PICK_CITIES) {
    place_get((uint8_t)(pick_first + i), name, &off, &rule);
    add(name);
    while (at < 22) add(" ");
    add_offset(off);
    if (rule) { add("  "); add(tc_rule_text(rule)); }
  } else {
    add(tc_rule_text(i));
    if (i == TC_DST_EU) add(" (UK, Ireland, the EU)");
  }
}

static void pick_row(uint8_t i, uint8_t top, uint8_t sel)
{
  pick_text(i);
  if (i == sel) m65_screen_reverse(1);
  ui_line((unsigned char)(PICK_FIRST + i - top), line, 0);
  m65_screen_reverse(0);
}

static void pick_page(uint8_t n, uint8_t top, uint8_t sel)
{
  uint8_t r;
  for (r = 0; r < PICK_ROWS; r++) {
    if ((uint8_t)(top + r) < n) pick_row((uint8_t)(top + r), top, sel);
    else ui_line((unsigned char)(PICK_FIRST + r), 0, 0);
  }
}

/* A list of n rows under `heading`; the index chosen, or 0xff for RUN/STOP.
 * Letters jump to the next row that starts with them. */
static uint8_t pick(const char *heading, uint8_t n, const char *stop_word)
{
  uint8_t sel = 0, top = 0, old, k, i, c;
  ui_clear_rows(3, (unsigned char)(m65_screen_rows() - 2));
  ui_line(ROW_PICK_HEAD, heading, 0);
  clear(); add("CRSR moves   RETURN chooses   ");
  if (pick_mode == PICK_CITIES) add("a letter jumps   ");
  add("RUN/STOP "); add(stop_word);
  ui_line(UI_ROW_KEYS, line, 0);
  pick_page(n, top, sel);
  for (;;) {
    k = ui_wait_key();
    old = sel;
    if (k == KEY_DOWN) { if (sel + 1 < n) sel++; }
    else if (k == KEY_UP) { if (sel) sel--; }
    else if (k == KEY_RIGHT) { if (top + PICK_ROWS < n) sel = (uint8_t)(top + PICK_ROWS); }
    else if (k == KEY_LEFT) sel = (uint8_t)(top >= PICK_ROWS ? top - PICK_ROWS : 0);
    else if (k == KEY_HOME) sel = 0;
    else if (k == KEY_RETURN) return sel;
    else if (k == KEY_STOP) return 0xff;
    else if (pick_mode == PICK_CITIES && ((k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z'))) {
      k = (uint8_t)(k | 0x20);
      for (i = 1; i <= n; i++) {              /* the next match after the selection, round to the top */
        uint8_t j = (uint8_t)((sel + i) % n);
        pick_text(j);
        c = (uint8_t)(line[2] | 0x20);
        if (c == k) { sel = j; break; }
      }
    } else continue;
    if (sel < top || sel >= top + PICK_ROWS) { top = (uint8_t)(sel - sel % PICK_ROWS); pick_page(n, top, sel); }
    else if (sel != old) { pick_row(old, top, sel); pick_row(sel, top, sel); }
  }
}

/* An offset and a rule typed and chosen; 1 when both were. */
static uint8_t location_by_hand(void)
{
  char buf[8];
  int16_t v;
  uint8_t r;
  tc_format_offset(buf, cfg_offset);
  ui_clear_rows(3, (unsigned char)(m65_screen_rows() - 1));
  ui_line(ROW_PICK_HEAD, "Your standard time, in hours from UTC: like -5, +1 or +5:30.", 0);
  ui_line(ROW_PICK_HEAD + 1, "Give the winter offset; daylight saving is chosen next.", 0);
  ui_line(UI_ROW_KEYS, "RETURN accepts   RUN/STOP goes back", 0);
  for (;;) {
    if (!ui_read_line(PICK_FIRST + 1, "Offset: UTC", buf, 7)) return 0;
    if (tc_parse_offset(buf, &v)) break;
    ui_status("not an offset: like -5, +1 or +5:30, from -12:00 to +14:00", 0);
  }
  pick_mode = PICK_RULES;
  r = pick("Does the clock change for daylight saving there?", TC_DST_RULES, "goes back");
  if (r == 0xff) return 0;
  cfg_offset = v; cfg_rule = r; cfg_city[0] = 0;
  return 1;
}

/* The whole question; 1 when a location was chosen. `first` is the run
 * before any NTP.CFG, where RUN/STOP leaves the program rather than
 * syncing to UTC. */
static uint8_t choose_location(uint8_t first)
{
  uint8_t r, c, cf, cn, rule;
  int16_t off;
  char heading[64];
  for (;;) {
    if (!places_count) {
      if (location_by_hand()) return 1;
      if (first) m65_exit_to_basic();
      return 0;
    }
    pick_mode = PICK_REGIONS;
    r = pick(first ? "Where is this MEGA65? Choose a region, so the clock keeps local time."
                   : "Where is this MEGA65? Choose a region.",
             (uint8_t)(regions_count + 1), first ? "quits" : "goes back");
    if (r == 0xff) { if (first) m65_exit_to_basic(); return 0; }
    if (r == regions_count) { if (location_by_hand()) return 1; continue; }
    region_cities(r, &cf, &cn);
    pick_first = cf;
    pick_mode = PICK_CITIES;
    clear(); add("Choose the city nearest you in "); region_name(r, heading); add(heading); add(".");
    for (c = 0; (heading[c] = line[c]) != 0; c++) ;
    c = pick(heading, cn, "goes back");
    if (c == 0xff) continue;
    place_get((uint8_t)(cf + c), cfg_city, &off, &rule);
    cfg_offset = off; cfg_rule = rule;
    return 1;
  }
}

static void edit_location(void)
{
  uint8_t chose = choose_location(0);
  ui_clear_rows(1, (unsigned char)(m65_screen_rows() - 1));
  draw_all();
  if (!chose) { ui_status(0, 0); return; }
  save_settings();
  if (net_ready) sync();                          /* the new location at once, as the first run does */
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
  places_load(boot_drive);
  ui_idle = on_idle;
  if (!cfg_load(boot_drive)) {
    /* no NTP.CFG: where the MEGA65 is comes first, and nothing is synced
     * until it is known (or RUN/STOP leaves) */
    choose_location(1);
    ui_clear_rows(1, (unsigned char)(m65_screen_rows() - 1));
    draw_all();
    save_settings();
  }
  draw_server();
  draw_offset();
  if (net_ready) sync();

  for (;;) {
    k = ui_wait_key();
    if (k >= 0xc1 && k <= 0xda && (ui_last_mods & MOD_MEGA)) k = (unsigned char)(k & 0x7f);   /* MEGA+letter (ssh 5.29) */
    switch (k) {
    case KEY_RETURN: sync(); break;
    case 's': case 'S': edit_server(); break;
    case 'l': case 'L': case 'o': case 'O': edit_location(); break;   /* O was the offset key */
    case 'f': case 'F':
      /* MEGA held, as every client binds the colors (2026-09-29).
       * Every row, not only the ones draw_all() knows how to draw: the
       * sync's results and the status keep no copy of their text, so the
       * whole color RAM ($FF80000, a byte per cell, no attributes in use
       * here) takes the new color instead of a redraw. */
      if (!(ui_last_mods & MOD_MEGA)) break;
      m65_screen_cycle_text_colour();
      lfill(0xff80000UL, m65_screen_text_colour(), 80 * (unsigned int)m65_screen_rows());
      break;
    case 'b': case 'B': if (ui_last_mods & MOD_MEGA) m65_screen_cycle_background(); break;
    case 'q': case 'Q': case KEY_STOP:
      m65_exit_to_basic();                         /* BASIC's READY, disk still mounted; never returns */
      break;
    default: break;
    }
  }
}

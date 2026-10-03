# mega-ntp

An NTP client for the MEGA65 on mega-net: DHCP, the time server looked
up by name, one SNTP exchange, the real-time clock set in local time by
a UTC offset, and read back to prove it. Settings in `NTP.CFG`. Native
mode, llvm-mos, 80x25, `-Oz`. Version in `src/ntpc.c` (`NTPC_VERSION`).
0BSD. Releases carry `bin/NTP.D81`.

The platform modules under `src/platform/` (`m65_*`: screen, font,
F011, CBM DOS, scratch from `../mega-ftp`; boot and exit from `../mega-ssh`,
which own the interrupt vectors) are shared with the gopher, FTP and
SSH clients. A fix to one belongs in all of them.

## Read first

1. This file.
2. `../mega-net/docs/PLATFORM.md`: the machine, the family's memory
   map, the traps, the tools, the test driver.
3. `REQUIREMENTS.md`: section 2 decisions, section 4 the plan, section
   5 the findings, 5.1 to 5.3.

## Commands

```
python3 build.py test     host suite for src/timecalc.c; must be 0 failed before anything else
python3 build.py          the client and bin/NTP.D81; refuses on the ssh 5.6 miscompile shape
```

On the machine, with the driver:

```
python3 tools/deploy.py                            NTP.D81 onto the card, carrying NTP.CFG over; the only way to redeploy
source ../mega-net/tools/m65lib.sh
boot_prg ntp.d81 ntp 'lock is set'                 one sync, start to finish
type_keys 'o'; type_line '-4'                      the offset ("Offset:" renders as "?FFSET:")
type_keys '~M'; wait_for 'clock is set' 30         sync again
python3 ../mega-net/tools/mon.py 'm ffd7110'       the clock's registers: BCD s m h d m y weekday
type_keys 'q'                                      to BASIC
```

Never a plain `put_d81`: it replaces `NTP.CFG` with none, the offset
falls back to UTC, and the next sync sets the user's clock four or
more hours wrong (5.3). Screen patterns stay lowercase and must not
match a fixed row: `'the clock'` is also in the Offset row, so it
matches before any sync has happened.

## How it fits

`src/ntpc.c` is the screen and the sync; `src/netutil.c` wraps mega-net
(load, DHCP, DNS, SNTP), every wait bounded; `src/config.c` is
`NTP.CFG`; `src/timecalc.c` is the arithmetic, portable, with the host
suite in `tests/`. mega-net converts the NTP seconds to a date with the
caller's offset (`NTP_RESULT`); the client writes the clock with
mega65-libc's `setrtc()`, not mega-net's `SET_RTC`, because the library
detects the board and unlocks and paces the R2/R3 chip (REQUIREMENTS.md
2, "Writing the clock"). F recolors the whole color RAM rather than
redrawing, because the sync's rows keep no copy of their text (5.2).

## What is mine in memory

Nothing beyond the family's table: the screen at `$0800`, the font copy
at `$11000`, the exit stub at `$1FB0`, the program from `$2001`.

## Rules

- The client owns its interrupt vectors as its first instruction and
  quiets `$D6E0` before mega-net's INIT (mega-net 5.18, ssh 5.8); never
  reorder `main`'s first lines.
- Every wait is bounded on `$D7FA`; poll mega-net from every loop.
  Nothing calls mega-net before `net_load()` succeeds (`net_ready`).
- The clock registers are a mirror of the I2C chip: read a write back
  after a pause, not at once.
- mega65-libc's `struct m65_tm` carries the chip's month (1-12) and
  weekday straight through, whatever its header says; the weekday is
  Sunday = 0, computed from the date.
- Test with a built program on disk, never by typing through `m65 -T`;
  one field per call.
- Record every hardware finding in `REQUIREMENTS.md`, numbered.
- Commits are local until the user asks for a push or a release. A
  release bumps `NTPC_VERSION`, rebuilds, tags `vX.Y.Z`, and attaches
  `NTP.D81` with a short note ending in how to run it.

## Not to reopen

SNTP, one exchange, whole seconds: the clock holds whole seconds. A UTC
offset, not time-zone rules; the user changes it for daylight saving.

## Open

Two faults each seen once and not reproduced in 15 runs since (5.3): a
sync that reported "the clock did not take the new time", and boots
that stopped at "waiting for an address..." past the DHCP bound. On
the next sighting capture the Reads row, `mon.py r` twice, `$FF80` and
the mailbox at `$1600` before anything resets the machine. Not yet
run: an R2/R3 board, a silent server, a write-protected disk.

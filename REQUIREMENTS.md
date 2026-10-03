# mega-ntp: requirements, decisions, and record

## 1. Goal

A native MEGA65 program that sets the real-time clock from the internet:
run it, and the clock, and so BASIC 65's date and time and the dates on
new files, are right. The application mega-net's step 4 proved possible
(mega-net REQUIREMENTS.md R-7, 5.7) as a program a person uses.

## 2. Decisions

| Decision | Choice, and why |
|---|---|
| **Protocol** | SNTP (RFC 4330), one request, whole seconds: mega-net's `NTP_START`/`STATE`/`RESULT`. The clock holds whole seconds, so NTP's filtering and fractions would buy nothing. |
| **Local time** | A UTC offset in minutes, `-12:00` to `+14:00`, passed to `NTP_RESULT`, which does the calendar arithmetic. No time-zone rules: the user changes the offset for daylight saving. |
| **Writing the clock** | mega65-libc's `setrtc()`, not mega-net's `SET_RTC`. The library reads the board revision (`$FFD3629`), unlocks and paces the R2/R3 chip's writes, and handles the R4-R6 chip; `SET_RTC` writes seven bytes by DMA to `$FFD7110` with neither, and does not translate the weekday (mega-net 5.7, carried). A client is the place for board knowledge, not the stack (mega-net rule 10). |
| **Weekday** | Sunday = 0, computed from the date: mega65-libc's convention and mega-net's actual one (`test_dnsntp.c`: a Friday is 5). `docs/ABI.md` said Monday = 0 until 2026-09-14; corrected, with a note on mega-net 5.7. |
| **Proof** | The clock is read back after a second, not at once, and the sync reports failure unless it holds the written time. The screen shows how far off the clock was. |
| **Shape** | One screen: a ticking clock, the settings, the last sync. Sync once at start, RETURN again, S and O for the server and the offset, the family's F/B colors and RUN/STOP to quit. |
| **Settings** | `NTP.CFG` on the program disk: magic, server, offset. Missing or foreign means `pool.ntp.org` and UTC. |
| **Platform code** | The FTP client's screen, font, F011, CBM DOS and scratch modules, and the SSH client's boot and exit, which own the interrupt vectors (mega-net 5.18). Copies, shared by rule. |
| **Build** | `build.py`, Python, all three development hosts; the host suite for the arithmetic; the ssh 5.6 miscompile check on every link. |

## 3. Requirements

- **F-1** Get an address by DHCP, resolve a named server, or take an address.
- **F-2** Ask the server for the time and set the real-time clock in local time.
- **F-3** Show the result: the address, the server, UTC, local time set, and the clock's error before.
- **F-4** Change and keep the server and the UTC offset.
- **F-5** Every failure explained in words on screen; nothing hangs.
- **N-1** No wait unbounded; the frame counter paces everything.
- **N-2** Runs from one `.d81` carrying the client and `MEGANET`.
- **N-3** Boards R2 to R6; anything else is told, not written.

## 4. Plan

| Step | What | Status |
|---|---|---|
| 1 | Skeleton, platform modules, arithmetic with the host suite | **Done** — 64 checks |
| 2 | The sync: DHCP, DNS, SNTP, `setrtc`, read-back | **Done** — 5.1 |
| 3 | Settings in `NTP.CFG`, the offset, restart and quit | **Done** — 5.1 |

## 5. Findings

### 5.1 The clock set from the internet, in local time, on an R6 (2026-09-14)

`bin/NTP.D81` (`ntp` 21,787 bytes, 86 blocks) put on the card, run
through the JTAG link with `m65lib.sh`. The board reports R6
(`$FFD3629` = 6).

**First sync, the defaults.** DHCP gave 192.168.1.252; `pool.ntp.org`
resolved to 170.187.142.180; the clock set to 2026-09-14 23:34:54 UTC
and read back. A minute later the registers at `$FFD7110` read
`47 35 A3 14 09 26 01`: 23:35:47, 14-09-26, weekday 1 (a Monday,
Sunday = 0), with the host at 23:35:48 UTC.

**Offset.** O, `-4`: "saved in NTP.CFG". RETURN: the pool answered from
44.190.5.123; set to 19:36:23 Monday (UTC-04:00); the clock "was
23:36:23, 4h00m00s fast", which is exactly the UTC it had been set to.
Registers `27 36 99 14 09 26 01`: 19:36:27, host 19:36:28 EDT.

**Restart.** A reset and `RUN "NTP"` booted on the first try in 5 s,
loaded `-04:00` from `NTP.CFG`, synced, and found the clock "right to
within a second". **Quit.** Q reached BASIC 65, whose banner read
`14-SEP-2026 19:36:51`: the system date BASIC reports is the clock just
set.

**The R6 chip keeps bit 7 of the hour register set.** `setrtc()` writes
the hour as plain BCD on R4-R6, yet it reads back `$A3` for 23 and `$99`
for 19. `getrtc()` masks the hour with `$3F` on those boards, so the
client sees the right hour; anything reading `$FFD7112` itself must mask
too.

**What it cost.** One mistake in the test, not the program: the boot
driver waited for `ntp client`, and the text screenshot renders a
capital as `?` (PLATFORM.md), so it saw `?lient`, called the start
stuck, and reset and re-synced the machine twice before being stopped.
Patterns stay lowercase and capital-free (`lock is set`).

### 5.2 F left the sync's rows in the old color (2026-09-14)

Seen by the user on the machine: after F, the title, clock, settings and
keys took the new color, but the five rows from "Address:" down, and the
status, did not. F redrew what `draw_all()` can draw, and the sync's
rows and the status keep no copy of their text. F now fills the whole
color RAM (`$FF80000`, 2000 cells; this client sets no attributes) with
the new color instead of redrawing. Measured through the monitor: color
bytes on rows 0, 8, 9, 23 and 24 went `01` to `02` to `03` with two
presses.

### 5.3 A redeploy set the user's clock to UTC; two faults not reproduced (2026-09-14)

**The redeploy.** Testing 5.2 put the new `NTP.D81` on the card with a
plain `put_d81`, which replaced the disk and with it the user's
`NTP.CFG` (UTC-04:00). The client fell back to UTC and every sync in
the tests that followed set the clock to UTC: the machine read 00:12
Tuesday during the user's 20:12 Monday. Put right by setting `-4` again
("was 00:13:12, 3h59m59s fast"). `tools/deploy.py`, on the SSH
client's model, now takes the card's disk first, keeps it under
`build/deploy`, and carries `NTP.CFG` into the new image; measured: it
reported `keeping ntp.cfg`, and the next boot synced at UTC-04:00, the
clock right to within a second. It is the only way to redeploy.

**A pattern that matched a fixed row.** A boot test waited for
`the clock`, which is also in the Offset row, so it matched a second
after `RUN` whatever the sync did. Patterns must match a sync's outcome
only (`lock is set`).

**Two faults seen once and not again.** (a) The first boot after the
5.2 upload showed "the clock did not take the new time"; the screen was
lost to the driver's retry before the Reads row was captured. (b) Two
of four cold boots in a later run sat at "waiting for an address..."
for over 60 s, past the DHCP wait's own bound of about 24 s, so the
program had stopped rather than failed; nothing was captured then
either. Since then 8 syncs by RETURN and 7 cold boots, run to capture
registers, `$FF80` and the mailbox on a stick and the Reads row on a
failure, all set the clock in 6 to 11 s. (b) resembles mega-net's
start-up stick (PLATFORM.md trap 1, about one boot in twenty); (a) is
unexplained. Both open.

**Carried.** Not yet seen: an R2/R3 board (the unlock path), a server
that never answers (the second lookup and the timeout message), a
write-protected disk (the save message), a real PNG capture of the
screen (`m65 -S` did not write the file as invoked).

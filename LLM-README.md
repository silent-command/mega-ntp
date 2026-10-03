# mega-ntp

An NTP client for the [MEGA65](https://mega65.org), written in C for
llvm-mos, running in native MEGA65 mode on [mega-net](../mega-net). It
asks a time server for the time and sets the machine's real-time clock
to it, in your local time.

**Status: working.** Syncing, the offset, the settings file and the
colors are verified on a MEGA65 R6, over about twenty syncs and cold
boots (REQUIREMENTS.md 5.1 to 5.3). See `REQUIREMENTS.md` for the
decisions, what was seen on hardware, and what is still open.

## Using it

`bin/NTP.D81` holds the client and mega-net. Mount it and `RUN "NTP"`.
The client gets an address by DHCP, looks up the time server, asks it
for the time, sets the clock, and reads the clock back to check that it
took the new time. The screen shows the address it was given, the
server it asked, the time in UTC, the local time it set, and how far
off the clock was before.

The first time, the server is `pool.ntp.org` and the offset is UTC+00:00,
so set the offset once:

| Key | Does |
|---|---|
| RETURN | sync again |
| S | change the time server: a name or an address; an empty line puts back `pool.ntp.org` |
| O | change the offset from UTC: `-5`, `+1`, `+5:30`, from `-12:00` to `+14:00` |
| F | next text color |
| B | next background and border color |
| RUN/STOP or Q | leave for BASIC's READY, the disk still mounted |

Settings are saved in `NTP.CFG` on the program disk. The clock keeps
local time and the MEGA65 has no daylight-saving rule, so change the
offset when the clocks change (`-5` to `-4` for New York in March, for
example) and press RETURN.

The clock line at the top ticks from the real-time clock itself, so it
shows what BASIC 65's `DT$` and `TI$` and the file dates will see.

The clock can be set on MEGA65 boards R2 to R6, through mega65-libc's
`setrtc()`, which knows each board's clock chip. On other targets the
client says so and leaves the clock alone.

## Building

Needs [llvm-mos](https://llvm-mos.org), CMake, `c1541` from VICE, Python 3,
and two sibling checkouts: `../mega-net` and
`../mega65-libc` (github.com/MEGA65/mega65-libc).

```
python3 build.py test     the host suite for the date and offset arithmetic
python3 build.py          the client and bin/NTP.D81
python3 tools/deploy.py   bin/NTP.D81 onto the SD card over the serial link, keeping NTP.CFG
```

The deploy tool needs `m65` and `mega65_ftp` from mega65-tools. Copying
`NTP.D81` onto the card any other way replaces the settings file, and
the next sync sets the clock to UTC.

## License

0BSD, see `LICENSE`. mega-net is a separate project under the same
license; mega65-libc is under its own. The screen, F011, CBM DOS, boot
and exit modules under `src/platform/` are shared with the gopher, FTP
and SSH clients, under the same license.

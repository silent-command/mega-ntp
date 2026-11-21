# mega-ntp

Sets the MEGA65's clock from an internet time server, in your local
time. Choose your city once and it works out daylight saving itself at
every sync.

## Starting

Mount `NTP.D81` and type `RUN "NTP"`, or choose NTP from the
[Net-Tools](https://github.com/silent-command/mega-menu) menu. It needs a
network cable and a MEGA65 with a real-time clock (boards R2 to R6). The
first time, it asks where you are before it syncs.

## Keys

| Key | Does |
|---|---|
| RETURN | sync again |
| L | choose your location |
| S | change the time server (empty for `pool.ntp.org`) |
| MEGA+F / MEGA+B | change the text / background colour |
| RUN/STOP or Q | quit to BASIC |

Choosing a location:

| Key | Does |
|---|---|
| CRSR up / down | move |
| CRSR left / right | previous / next page |
| CLR/HOME | top of the list |
| a letter | jump to the next city starting with it |
| RETURN | choose |
| RUN/STOP | back |

If your city is not listed, choose "None of these" and type your
standard offset from UTC (for example `-5` or `+5:30`), then pick a
daylight-saving rule.

## Credits

Created by Justin Cameron. Apache License 2.0; see [LICENSE](LICENSE)
and [NOTICE](NOTICE).

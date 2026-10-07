#!/usr/bin/env python3
"""Runs build/host/test_places (the client's loader over assets/zones.txt)
at three moments, summer and winter on both sides of the equator, and
compares each city's offset with the IANA time-zone database. Called by
`python3 build.py test`."""
import subprocess, sys
from datetime import datetime, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_zones

def main():
    exe = sys.argv[1]
    zones = {(r, c): z for r, lst in make_zones.CITIES for c, z in lst}
    bad = n = 0
    for when in (datetime(2026, 10, 7, 12, tzinfo=timezone.utc), datetime(2027, 1, 15, 12, tzinfo=timezone.utc),
                 datetime(2027, 7, 15, 12, tzinfo=timezone.utc)):
        out = subprocess.run([exe, str(when.year), str(when.month), str(when.day), str(when.hour), "0", "0"],
                             capture_output=True, text=True)
        for line in filter(None, out.stdout.split("\n")):
            reg, city, off = line.split("|")
            n += 1
            m = int(when.astimezone(ZoneInfo(zones[(reg, city)])).utcoffset().total_seconds() // 60)
            want = f"{'-' if m < 0 else '+'}{abs(m) // 60:02d}:{abs(m) % 60:02d}"
            if want != off:
                bad += 1
                print(f"FAIL {when.date()} {city}: {off}, the database says {want}")
    print(f"locations: {n} city-dates checked, {bad} wrong")
    return 1 if bad or not n else 0

if __name__ == "__main__":
    sys.exit(main())

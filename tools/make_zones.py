#!/usr/bin/env python3
"""Makes assets/zones.txt, the location list the client offers, from the
IANA time-zone database (Python's zoneinfo): for each city, its standard
offset, and which of the client's daylight-saving rules reproduces every
one of its real transitions from today to YEAR_TO. The rules are
checked by running the client's own C (src/timecalc.c) through a small
host program, so the list can only name a rule the client gets right.
A city that keeps daylight saving by a rule the client does not have is
left out, and the script says so.

    python3 tools/make_zones.py        rewrites assets/zones.txt

Run it again when governments change their zones, and rebuild."""
import os, subprocess, sys, tempfile
from datetime import datetime, timedelta, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parent.parent
YEAR_FROM, YEAR_TO = 2026, 2036
RULES = ["NONE", "US", "EU", "AU", "NZ", "CL", "IL", "EG"]

# Region, the city's name as shown (16 characters at most), its zone.
CITIES = [
 ("North America", [("St. John's", "America/St_Johns"), ("Halifax", "America/Halifax"), ("Toronto", "America/Toronto"),
   ("Montreal", "America/Toronto"), ("New York", "America/New_York"), ("Boston", "America/New_York"),
   ("Washington DC", "America/New_York"), ("Atlanta", "America/New_York"), ("Miami", "America/New_York"),
   ("Detroit", "America/Detroit"), ("Chicago", "America/Chicago"), ("Dallas", "America/Chicago"),
   ("Houston", "America/Chicago"), ("Minneapolis", "America/Chicago"), ("Winnipeg", "America/Winnipeg"),
   ("Regina", "America/Regina"), ("Mexico City", "America/Mexico_City"), ("Denver", "America/Denver"),
   ("Calgary", "America/Edmonton"), ("Edmonton", "America/Edmonton"), ("Salt Lake City", "America/Denver"),
   ("Phoenix", "America/Phoenix"), ("Whitehorse", "America/Whitehorse"), ("Los Angeles", "America/Los_Angeles"),
   ("San Francisco", "America/Los_Angeles"), ("Seattle", "America/Los_Angeles"), ("Las Vegas", "America/Los_Angeles"),
   ("Vancouver", "America/Vancouver"), ("Tijuana", "America/Tijuana"), ("Anchorage", "America/Anchorage"),
   ("Honolulu", "Pacific/Honolulu")]),
 ("Central and South America", [("Guatemala City", "America/Guatemala"), ("San Jose CR", "America/Costa_Rica"),
   ("Panama City", "America/Panama"), ("Kingston", "America/Jamaica"), ("Bogota", "America/Bogota"),
   ("Quito", "America/Guayaquil"), ("Lima", "America/Lima"), ("Caracas", "America/Caracas"),
   ("Santo Domingo", "America/Santo_Domingo"), ("San Juan", "America/Puerto_Rico"), ("La Paz", "America/La_Paz"),
   ("Manaus", "America/Manaus"), ("Santiago", "America/Santiago"), ("Asuncion", "America/Asuncion"),
   ("Buenos Aires", "America/Argentina/Buenos_Aires"), ("Montevideo", "America/Montevideo"),
   ("Sao Paulo", "America/Sao_Paulo"), ("Rio de Janeiro", "America/Sao_Paulo"), ("Brasilia", "America/Sao_Paulo")]),
 ("Europe", [("Reykjavik", "Atlantic/Reykjavik"), ("Dublin", "Europe/Dublin"), ("London", "Europe/London"),
   ("Lisbon", "Europe/Lisbon"), ("Madrid", "Europe/Madrid"), ("Paris", "Europe/Paris"), ("Brussels", "Europe/Brussels"),
   ("Amsterdam", "Europe/Amsterdam"), ("Berlin", "Europe/Berlin"), ("Zurich", "Europe/Zurich"), ("Rome", "Europe/Rome"),
   ("Vienna", "Europe/Vienna"), ("Prague", "Europe/Prague"), ("Warsaw", "Europe/Warsaw"), ("Copenhagen", "Europe/Copenhagen"),
   ("Oslo", "Europe/Oslo"), ("Stockholm", "Europe/Stockholm"), ("Budapest", "Europe/Budapest"), ("Belgrade", "Europe/Belgrade"),
   ("Helsinki", "Europe/Helsinki"), ("Riga", "Europe/Riga"), ("Tallinn", "Europe/Tallinn"), ("Vilnius", "Europe/Vilnius"),
   ("Kyiv", "Europe/Kyiv"), ("Bucharest", "Europe/Bucharest"), ("Sofia", "Europe/Sofia"), ("Athens", "Europe/Athens"),
   ("Istanbul", "Europe/Istanbul"), ("Minsk", "Europe/Minsk"), ("Moscow", "Europe/Moscow")]),
 ("Africa", [("Accra", "Africa/Accra"), ("Dakar", "Africa/Dakar"), ("Casablanca", "Africa/Casablanca"),
   ("Algiers", "Africa/Algiers"), ("Tunis", "Africa/Tunis"), ("Lagos", "Africa/Lagos"), ("Kinshasa", "Africa/Kinshasa"),
   ("Cairo", "Africa/Cairo"), ("Johannesburg", "Africa/Johannesburg"), ("Harare", "Africa/Harare"),
   ("Nairobi", "Africa/Nairobi"), ("Addis Ababa", "Africa/Addis_Ababa")]),
 ("Middle East", [("Jerusalem", "Asia/Jerusalem"), ("Tel Aviv", "Asia/Jerusalem"), ("Amman", "Asia/Amman"),
   ("Damascus", "Asia/Damascus"), ("Baghdad", "Asia/Baghdad"), ("Riyadh", "Asia/Riyadh"), ("Kuwait City", "Asia/Kuwait"),
   ("Doha", "Asia/Qatar"), ("Tehran", "Asia/Tehran"), ("Dubai", "Asia/Dubai"), ("Muscat", "Asia/Muscat"),
   ("Baku", "Asia/Baku"), ("Tbilisi", "Asia/Tbilisi"), ("Yerevan", "Asia/Yerevan")]),
 ("Asia", [("Kabul", "Asia/Kabul"), ("Karachi", "Asia/Karachi"), ("Tashkent", "Asia/Tashkent"), ("Almaty", "Asia/Almaty"),
   ("Delhi", "Asia/Kolkata"), ("Mumbai", "Asia/Kolkata"), ("Bengaluru", "Asia/Kolkata"), ("Kolkata", "Asia/Kolkata"),
   ("Colombo", "Asia/Colombo"), ("Kathmandu", "Asia/Kathmandu"), ("Dhaka", "Asia/Dhaka"), ("Yangon", "Asia/Yangon"),
   ("Bangkok", "Asia/Bangkok"), ("Hanoi", "Asia/Bangkok"), ("Ho Chi Minh City", "Asia/Ho_Chi_Minh"),
   ("Jakarta", "Asia/Jakarta"), ("Singapore", "Asia/Singapore"), ("Kuala Lumpur", "Asia/Kuala_Lumpur"),
   ("Manila", "Asia/Manila"), ("Hong Kong", "Asia/Hong_Kong"), ("Beijing", "Asia/Shanghai"), ("Shanghai", "Asia/Shanghai"),
   ("Taipei", "Asia/Taipei"), ("Seoul", "Asia/Seoul"), ("Tokyo", "Asia/Tokyo"), ("Osaka", "Asia/Tokyo"),
   ("Vladivostok", "Asia/Vladivostok")]),
 ("Oceania", [("Perth", "Australia/Perth"), ("Darwin", "Australia/Darwin"), ("Adelaide", "Australia/Adelaide"),
   ("Brisbane", "Australia/Brisbane"), ("Sydney", "Australia/Sydney"), ("Melbourne", "Australia/Melbourne"),
   ("Canberra", "Australia/Sydney"), ("Hobart", "Australia/Hobart"), ("Guam", "Pacific/Guam"), ("Noumea", "Pacific/Noumea"),
   ("Auckland", "Pacific/Auckland"), ("Wellington", "Pacific/Auckland"), ("Suva", "Pacific/Fiji"), ("Apia", "Pacific/Apia"),
   ("Nuku'alofa", "Pacific/Tongatapu"), ("Kiritimati", "Pacific/Kiritimati")]),
 ("UTC", [("UTC", "UTC")]),
]

PROBE = r'''
#include <stdio.h>
#include "timecalc.h"
int main(void) {
  unsigned r, y, mo, d, h, mi; int std;
  while (scanf("%u %d %u %u %u %u %u", &r, &std, &y, &mo, &d, &h, &mi) == 7) {
    tc_date t = { (uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, 0, 0 };
    printf("%u\n", tc_dst_in_effect((uint8_t)r, (int16_t)std, &t));
  }
  return 0;
}
'''

def samples(zone):
    """Each UTC hour from YEAR_FROM to YEAR_TO, and whether the zone is on
    summer time then. Hourly is exact here: every transition in these
    zones falls on the hour or the half hour, and both sides are probed."""
    z = ZoneInfo(zone)
    # from today, not New Year: a place that changed its rules this year
    # (British Columbia and Alberta stayed on summer time from March
    # 2026) is described by what it does now
    t = datetime.now(timezone.utc).replace(minute=0, second=0, microsecond=0)
    end = datetime(YEAR_TO + 1, 1, 1, tzinfo=timezone.utc)
    offs = []
    while t < end:
        offs.append((t, int(t.astimezone(z).utcoffset().total_seconds() // 60)))
        t += timedelta(minutes=30)
    return offs

def main():
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "probe.c"; exe = Path(td) / "probe"
        src.write_text(PROBE)
        subprocess.run(["cc", "-std=c99", "-O2", "-I", str(ROOT / "src"), str(src), str(ROOT / "src" / "timecalc.c"), "-o", str(exe)], check=True)
        out_lines, left_out = [], []
        for region, cities in CITIES:
            for city, zone in cities:
                assert len(city) <= 16, city
                offs = samples(zone)
                std = min(o for _, o in offs)
                has_dst = any(o != std for _, o in offs)
                chosen = None
                for r, name in enumerate(RULES):
                    if (r == 0) == has_dst: continue
                    if r == 0: chosen = name; break
                    want = [1 if o != std else 0 for _, o in offs]
                    feed = "".join(f"{r} {std} {t.year} {t.month} {t.day} {t.hour} {t.minute}\n" for t, _ in offs)
                    got = subprocess.run([str(exe)], input=feed, capture_output=True, text=True).stdout.split()
                    if [int(g) for g in got] == want: chosen = name; break
                if chosen is None or (has_dst and any(abs(o - std) not in (0, 60) for _, o in offs)):
                    left_out.append(f"{city} ({zone})"); continue
                sign = "-" if std < 0 else "+"
                out_lines.append(f"{region}|{city}|{sign}{abs(std) // 60:02d}:{abs(std) % 60:02d}|{chosen}")
    head = ["ZONES1"]
    (ROOT / "assets").mkdir(exist_ok=True)
    (ROOT / "assets" / "zones.txt").write_text("\n".join(head + out_lines) + "\n")
    print(f"assets/zones.txt: {len(out_lines)} cities, checked from today to {YEAR_TO} at every half hour")
    if left_out:
        print("left out, no rule matches:", ", ".join(left_out))
    return 0

if __name__ == "__main__":
    sys.exit(main())

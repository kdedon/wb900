#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""screenmodeprefs.py OUT MODEID WIDTH HEIGHT DEPTH: write a saved
ScreenMode setting, ENV:Sys/screenmode.prefs: an IFF FORM PREF of a
PRHD chunk and an SCRM chunk (struct ScreenModePrefs, big-endian).
"""
import struct
import sys


def chunk(cid, data):
    return cid + struct.pack('>L', len(data)) + data + b'\0' * (len(data) & 1)


def prefs(modeid, width, height, depth):
    prhd = struct.pack('>BBL', 0, 0, 0)
    scrm = struct.pack('>4LL4H', 0, 0, 0, 0, modeid, width, height, depth, 0)
    body = b'PREF' + chunk(b'PRHD', prhd) + chunk(b'SCRM', scrm)
    return b'FORM' + struct.pack('>L', len(body)) + body


if __name__ == '__main__':
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    with open(sys.argv[1], 'wb') as f:
        f.write(prefs(int(sys.argv[2], 0), *map(int, sys.argv[3:])))

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Generate the headers the firmware includes (fonts, icons, tables, samples, drum kits, FM6 patches, logo)
into tab5/gen. They are shipped pre-generated for the SLOOP commit this port was tested with, so normally this
does nothing. To regenerate (another SLOOP version): delete tab5/gen/*.h and run it again (needs Pillow)."""
import os, subprocess, sys

root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
gen = os.path.join(root, "tab5", "gen")
os.makedirs(gen, exist_ok=True)
jobs = [("gen_font", "felucca_font.h"), ("gen_icons", "felucca_icons.h"), ("gen_tables", "felucca_tables.h"),
        ("gen_samples", "felucca_samples.h"), ("gen_drumkits", "felucca_drumkits.h"),
        ("gen_fm6_patches", "felucca_fm6.h"), ("gen_logo", "sloop_logo.h")]
for script, out in jobs:
    dst = os.path.join(gen, out)
    if os.path.exists(dst):
        continue
    r = subprocess.run([sys.executable, os.path.join(root, "tools", script + ".py"), dst], cwd=root,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        sys.exit("could not generate %s (%s needs Pillow: python -m pip install pillow):\n%s" % (out, script, r.stderr[-400:]))

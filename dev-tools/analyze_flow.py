"""Read-only analysis of a worldspace's water flow tiles (flow.X.Y.dds, one 64x64 tile per exterior cell).

Encoding (from the Community Shaders Water.hlsl): R,G = flow direction (0..1 -> -1..1), B = slowness (speed ~ sqrt(1.01 - B)), A = mask.
Cell Y increases toward the top of the image (tile (x,y+1) sits above tile (x,y)); cell X increases to the right.

usage: python analyze_flow.py <flow folder> <output folder>
"""
import collections
import csv
import os
import re
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

SRC = sys.argv[1] if len(sys.argv) > 1 else "C:/Modlists/Tabula/mods/CS Water Mod/textures/water/skyrim.esm"
OUT = sys.argv[2] if len(sys.argv) > 2 else "E:/WorkSpace/WaterSeam/report"
os.makedirs(OUT, exist_ok=True)

MIN_SPEED = 0.25      # a pixel only counts for direction comparisons when it really flows
BAD_ANGLE = 45.0      # mean turn across a border above this is reported as a seam
BAD_SPEED = 0.30      # mean speed jump across a border above this is reported as a seam


def speed(a):
    return np.sqrt(np.clip(1.01 - a[..., 2], 0, None))


def vec(a):
    return a[..., 0] * 2 - 1, a[..., 1] * 2 - 1


def dxgi_format(path):
    with open(path, "rb") as f:
        d = f.read(148)
    fourcc = d[84:88]
    return struct.unpack_from("<I", d, 128)[0] if fourcc == b"DX10" else 0


tiles, fmt, tga = {}, {}, {}
for name in sorted(os.listdir(SRC)):
    m = re.match(r"flow\.(-?\d+)\.(-?\d+)\.(dds|tga)$", name, re.I)
    if not m:
        continue
    key = (int(m[1]), int(m[2]))
    path = os.path.join(SRC, name)
    arr = np.asarray(Image.open(path).convert("RGBA")).astype(np.float32) / 255.0
    if m[3].lower() == "dds":
        tiles[key] = arr
        fmt[key] = dxgi_format(path)
    else:
        tga[key] = arr
print(f"{len(tiles)} dds tiles, {len(tga)} tga tiles")

# ---------------------------------------------------------------- per-tile facts
names = {0: "uncompressed", 98: "BC7_UNORM", 99: "BC7_SRGB"}
fmt_count = collections.Counter(names.get(v, str(v)) for v in fmt.values())
srgb = sorted(k for k, v in fmt.items() if v == 99)

tga_diff = []
for k, t in tga.items():
    if k in tiles:
        # the CK writes the TGA with the origin at the bottom-left; compare both orientations
        d0 = np.abs(t - tiles[k]).mean()
        d1 = np.abs(t[::-1] - tiles[k]).mean()
        tga_diff.append((k, min(d0, d1)))
tga_mismatch = sorted([x for x in tga_diff if x[1] > 0.02], key=lambda x: -x[1])

# ---------------------------------------------------------------- borders
def edge(t, side):
    return {"E": t[:, -1], "W": t[:, 0], "N": t[0, :], "S": t[-1, :]}[side]


def compare(a, b):
    ax, ay = vec(a)
    bx, by = vec(b)
    sa, sb = speed(a), speed(b)
    moving = (sa > MIN_SPEED) & (sb > MIN_SPEED) & (np.hypot(ax, ay) > 0.2) & (np.hypot(bx, by) > 0.2)
    dsp = float(np.abs(sa - sb).mean())
    if moving.sum() < 8:
        return None, dsp, int(moving.sum())
    cosang = (ax * bx + ay * by) / (np.hypot(ax, ay) * np.hypot(bx, by) + 1e-9)
    ang = np.degrees(np.arccos(np.clip(cosang, -1, 1)))
    return float(ang[moving].mean()), dsp, int(moving.sum())


borders = []   # (tileA, tileB, orientation, angle, speedDiff, movingPixels)
for (x, y), t in tiles.items():
    right = (x + 1, y)
    up = (x, y + 1)
    if right in tiles:
        ang, dsp, n = compare(edge(t, "E"), edge(tiles[right], "W"))
        borders.append(((x, y), right, "x", ang, dsp, n))
    if up in tiles:
        ang, dsp, n = compare(edge(t, "N"), edge(tiles[up], "S"))
        borders.append(((x, y), up, "y", ang, dsp, n))

with_flow = [b for b in borders if b[3] is not None]
bad = [b for b in borders if (b[3] is not None and b[3] > BAD_ANGLE) or b[4] > BAD_SPEED]
bad.sort(key=lambda b: -(b[3] or 0))

isolated = [k for k in tiles if not any((k[0] + dx, k[1] + dy) in tiles for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]

# ---------------------------------------------------------------- outputs
with open(os.path.join(OUT, "borders.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["cellA", "cellB", "axis", "mean_turn_deg", "mean_speed_jump", "moving_pixels", "fmtA", "fmtB"])
    for a, b, ax_, ang, dsp, n in sorted(borders, key=lambda r: -(r[3] or 0)):
        w.writerow([f"{a[0]},{a[1]}", f"{b[0]},{b[1]}", ax_, "" if ang is None else f"{ang:.1f}", f"{dsp:.3f}", n, names.get(fmt[a]), names.get(fmt[b])])

# overview map
S = 16
xs = [k[0] for k in tiles]
ys = [k[1] for k in tiles]
x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
W, H = (x1 - x0 + 1) * S, (y1 - y0 + 1) * S
img = Image.new("RGB", (W, H), (20, 20, 24))
px = img.load()
import colorsys
for (x, y), t in tiles.items():
    small = t.reshape(S, 64 // S, S, 64 // S, 4).mean((1, 3))
    vx, vy = vec(small)
    sp = speed(small)
    for j in range(S):
        for i in range(S):
            ang = (np.degrees(np.arctan2(-vy[j, i], vx[j, i])) % 360) / 360.0
            r, g, b = colorsys.hsv_to_rgb(ang, 0.85, 0.25 + 0.75 * float(np.clip(sp[j, i], 0, 1)))
            px[(x - x0) * S + i, (y1 - y) * S + j] = (int(r * 255), int(g * 255), int(b * 255))
d = ImageDraw.Draw(img)
for a, b, ax_, ang, dsp, n in bad:
    if ax_ == "x":
        X = (b[0] - x0) * S
        Y = (y1 - a[1]) * S
        d.line([(X, Y), (X, Y + S)], fill=(255, 255, 255), width=2)
    else:
        X = (a[0] - x0) * S
        Y = (y1 - b[1]) * S + S
        d.line([(X, Y), (X + S, Y)], fill=(255, 255, 255), width=2)
for k in srgb:
    X, Y = (k[0] - x0) * S, (y1 - k[1]) * S
    d.rectangle([X, Y, X + S - 1, Y + S - 1], outline=(255, 0, 255))
for k in isolated:
    X, Y = (k[0] - x0) * S, (y1 - k[1]) * S
    d.rectangle([X + 4, Y + 4, X + S - 5, Y + S - 5], outline=(255, 255, 0))
img.save(os.path.join(OUT, "flow_overview.png"))

lines = []
lines.append(f"# Water flow tile report\n\nSource: `{SRC}`\n")
lines.append(f"- {len(tiles)} tiles (cells X {x0}..{x1}, Y {y0}..{y1}); formats: " + ", ".join(f"{n} {c}" for n, c in fmt_count.items()))
lines.append(f"- **sRGB tiles ({len(srgb)}): the engine linearises R,G,B, so direction and speed are wrong there:** " + ", ".join(f"({a},{b})" for a, b in srgb))
lines.append(f"- {len(tga)} CK source TGAs; {len(tga_mismatch)} differ from their DDS by more than 2%: " + ", ".join(f"({k[0]},{k[1]}) {v:.3f}" for k, v in tga_mismatch[:10]))
lines.append(f"- tiles with no neighbouring tile ({len(isolated)}): " + ", ".join(f"({a},{b})" for a, b in isolated[:30]))
lines.append(f"- tile borders: {len(borders)} total, {len(with_flow)} with real flow on both sides, "
             f"{sum(1 for b in with_flow if b[3] > BAD_ANGLE)} turn more than {BAD_ANGLE:.0f} degrees, "
             f"{sum(1 for b in borders if b[4] > BAD_SPEED)} jump in speed by more than {BAD_SPEED}")
angs = np.array([b[3] for b in with_flow])
if len(angs):
    lines.append(f"- turn across borders with flow: median {np.median(angs):.1f}, p90 {np.percentile(angs, 90):.1f}, max {angs.max():.1f} degrees")
lines.append("\n## Worst borders\n\n| cell A | cell B | turn (deg) | speed jump | moving px | formats |\n|---|---|---|---|---|---|")
for a, b, ax_, ang, dsp, n in bad[:25]:
    lines.append(f"| ({a[0]},{a[1]}) | ({b[0]},{b[1]}) | {'-' if ang is None else f'{ang:.0f}'} | {dsp:.2f} | {n} | {names.get(fmt[a])}/{names.get(fmt[b])} |")
lines.append("\nOverview image: `flow_overview.png` (hue = flow direction, brightness = speed, white line = bad border, magenta box = sRGB tile, yellow box = tile with no neighbour).")
open(os.path.join(OUT, "report.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("\n".join(lines[:9]))

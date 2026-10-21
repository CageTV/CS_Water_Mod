"""Flow tile border fixes for CS Water Mod (test list): dry run by default, `apply` writes (with a backup next to each tile).

  srgb     re-encode the BC7_SRGB flow tiles as UNORM (the engine linearises sRGB, which bends direction and speed)
  blend    ramp the speed of a fast river tile down to the speed of its slower neighbour over the last WIDTH pixels,
           so the water does not jump from 1.0 to 0.1 at a tile border (what 0.-15 / 0.-14 got by hand)

usage: python blendborders.py <srgb|blend> [apply]
Flow encoding: R,G = direction (0..1 -> -1..1), B = slowness, speed = sqrt(1.01 - B), A = mask. Image row 0 = north.
"""
import numpy as np, os, shutil, struct, sys, time
from PIL import Image

D = 'C:/Modlists/Tabula/mods/CS Water Mod/textures/water/skyrim.esm/'
WIDTH = 20
ts = time.strftime('%Y%m%d_%H%M%S')

# (tile, edge to ramp toward, neighbour tile). Only pairs where both cells are flowmap water and the tile edge runs ~1.0 beside a 0.10 tile.
BLEND = [
    ((-10, -14), 'E', (-9, -14)),
    ((-10, -14), 'N', (-10, -13)),
    ((-10, -14), 'S', (-10, -15)),
    ((-11, -14), 'N', (-11, -13)),
    ((-11, -14), 'S', (-11, -15)),
    ((22, 9), 'W', (21, 9)),
    ((-36, -3), 'W', (-37, -3)),
    ((3, -12), 'S', (3, -13)),
    ((-24, 13), 'E', (-23, 13)),
    ((0, -14), 'E', (1, -14)),
    ((-41, 0), 'E', (-40, 0)),
]
SRGB = [(19, 8), (19, 9), (19, 10), (20, 8), (20, 9), (20, 10), (21, 9), (21, 10)]


def path(t):
    return D + f'flow.{t[0]}.{t[1]}.dds'


def load(t):
    return np.asarray(Image.open(path(t)).convert('RGBA')).copy()


def speed(a):
    return np.sqrt(np.clip(1.01 - a[..., 2] / 255.0, 0, None))


def setspeed(a, s):
    a[..., 2] = np.round(np.clip(1.01 - s * s, 0, 1) * 255).astype(np.uint8)


def write_dds(p, a):
    h, w, _ = a.shape
    hdr = bytearray(128)
    hdr[0:4] = b'DDS '
    struct.pack_into('<I', hdr, 4, 124)
    struct.pack_into('<I', hdr, 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x8)
    struct.pack_into('<I', hdr, 12, h); struct.pack_into('<I', hdr, 16, w); struct.pack_into('<I', hdr, 20, w * 4)
    struct.pack_into('<I', hdr, 28, 1)
    struct.pack_into('<I', hdr, 76, 32)
    struct.pack_into('<I', hdr, 80, 0x4)
    hdr[84:88] = b'DX10'
    struct.pack_into('<I', hdr, 108, 0x1000)
    dx10 = struct.pack('<5I', 28, 3, 0, 1, 0)      # R8G8B8A8_UNORM, 2D, single mip
    open(p, 'wb').write(bytes(hdr) + dx10 + a.tobytes())


def backup(p, tag):
    shutil.copy2(p, p + f'.bak_pre_{tag}_{ts}')


def edge_profile(a, edge):
    s = speed(a)
    return {'E': s[:, -1], 'W': s[:, 0], 'N': s[0, :], 'S': s[-1, :]}[edge]


def ramp(a, edge, target):
    """Pull the speed toward `target` (per pixel along the edge) at the edge, back to the tile's own speed WIDTH pixels in. Only slows."""
    s = speed(a)
    new = s.copy()
    for d in range(WIDTH):
        t = d / float(WIDTH)
        k = t * t * (3 - 2 * t)
        if edge == 'E':
            line, idx = s[:, 63 - d], (slice(None), 63 - d)
        elif edge == 'W':
            line, idx = s[:, d], (slice(None), d)
        elif edge == 'N':
            line, idx = s[d, :], (d, slice(None))
        else:
            line, idx = s[63 - d, :], (63 - d, slice(None))
        new[idx] = np.minimum(line, target + (line - target) * k)
    out = a.copy()
    setspeed(out, new)
    return out


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else 'blend'
    do = len(sys.argv) > 2 and sys.argv[2] == 'apply'
    if mode == 'srgb':
        for t in SRGB:
            a = load(t)
            print(t, 'BC7_SRGB -> R8G8B8A8_UNORM; speed mean', round(float(speed(a).mean()), 3))
            if do:
                backup(path(t), 'srgbfix')
                write_dds(path(t), a)
        return
    cache = {}
    for tile, edge, nb in BLEND:
        a = cache.get(tile)
        if a is None:
            a = cache[tile] = load(tile)
        target = edge_profile(load(nb), {'E': 'W', 'W': 'E', 'N': 'S', 'S': 'N'}[edge])
        before = [round(float(edge_profile_at(a, edge, d).mean()), 2) for d in (0, 5, 10, 20)]
        cache[tile] = ramp(a, edge, target)
        after = [round(float(edge_profile_at(cache[tile], edge, d).mean()), 2) for d in (0, 5, 10, 20)]
        print(tile, edge, '| neighbour', nb, 'edge speed', round(float(target.mean()), 2), '| speed at 0,5,10,20 px in: before', before, 'after', after)
    if do:
        for tile, a in cache.items():
            backup(path(tile), 'borderblend')
            write_dds(path(tile), a)
            chk = np.asarray(Image.open(path(tile)).convert('RGBA'))
            print('written', tile, 'alpha min', chk[..., 3].min())


def edge_profile_at(a, edge, d):
    s = speed(a)
    return {'E': s[:, 63 - d], 'W': s[:, d], 'N': s[d, :], 'S': s[63 - d, :]}[edge]


if __name__ == '__main__':
    main()

import os, re

BS = chr(92)


def load(d):
    out = {}
    for f in sorted(os.listdir(d)):
        t = open(os.path.join(d, f), encoding='utf-8').read()
        kv = {}
        for l in t.splitlines():
            m = re.match(r'^(\w+): ?(.*)$', l)
            if m:
                kv[m[1]] = m[2]
        out[kv.get('FormKey', '?')] = kv
    return out


cs = load('C:/Temp/csw_yaml_CSWaterMod/Waters')
cp = load('C:/Temp/csw_yaml_CSwaterCleanNeedsPatch/Waters')
print('CS Water Mod.esp', len(cs), 'Clean patch', len(cp))


def tex(k):
    return k.get('NoiseLayerOneTexture', '').replace('data' + BS + 'textures' + BS + 'water' + BS, '')


final = dict(cs)
for k, v in cp.items():
    final[k] = v
new = [k for k in cs if k.endswith('CS Water Mod.esp')]
print('new CS types:', len(new), ' overrides of master types:', len(cs) - len(new))
print()
print('%-6s %-36s %-14s %-28s %s' % ('where', 'editor id', 'flags', 'layer1 texture', 'shallow / deep'))
rows = sorted(final.items(), key=lambda x: (x[0].endswith('CS Water Mod.esp'), x[1].get('EditorID', '')))
for k, v in rows:
    fl = v.get('Flags', '').replace('[', '').replace(']', '')
    where = 'new' if k.endswith('CS Water Mod.esp') else ('ovr+p' if k in cp else 'ovr')
    print('%-6s %-36s %-14s %-28s %s / %s' % (where, v.get('EditorID', '?')[:36], fl[:14], tex(v)[:28], v.get('ShallowColor', ''), v.get('DeepColor', '')))

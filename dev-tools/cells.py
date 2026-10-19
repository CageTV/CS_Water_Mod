import os, re, collections

BASE = 'C:/Temp/csw_yaml_CSWaterMod'


def load(d):
    out = {}
    for f in sorted(os.listdir(d)):
        t = open(os.path.join(d, f), encoding='utf-8').read()
        kv = {}
        for l in t.splitlines():
            m = re.match(r'^(\w+): ?(.*)$', l)
            if m:
                kv[m[1]] = m[2]
        out[kv.get('FormKey', '?')] = kv.get('EditorID', '?')
    return out


names = load(BASE + '/Waters')
# vanilla names from the Water for ENB serialization (same records, vanilla editor ids)
names.update({k: v for k, v in load('C:/Temp/wfe_yaml2/Waters').items() if k not in names})

count = collections.Counter()
cells = []
for root, _, files in os.walk(BASE + '/Worldspaces'):
    for f in files:
        if f.endswith('.yaml') and f != 'GroupRecordData.yaml':
            t = open(os.path.join(root, f), encoding='utf-8').read()
            m = re.search(r'^Water: (.+?)[ ]*$', t, re.M)
            if m and re.search(r'^FormKey:.*', t, re.M) and 'Grid' in t or (m and 'XCLC' in t):
                pass
            if m:
                key = m[1]
                count[key] += 1
                cells.append((os.path.relpath(root, BASE + '/Worldspaces'), f, key))
total = sum(count.values())
print('cell records with a Water assignment in CS Water Mod.esp:', total)
mine = sum(c for k, c in count.items() if k.endswith('CS Water Mod.esp'))
print('  pointing at a NEW CS type:', mine, ' at a vanilla/other type:', total - mine)
print()
for k, c in count.most_common():
    print('%5d  %-36s %s' % (c, names.get(k, '?'), k))

print()
custom = [(r, f, k) for (r, f, k) in cells if k.endswith('CS Water Mod.esp')]
import json
out = []
for r, f, k in custom:
    t = open(os.path.join(BASE, 'Worldspaces', r, f), encoding='utf-8').read()
    fk = re.search(r'^FormKey: (.+)$', t, re.M)
    gr = re.search(r'^Grid:.*|^GridX.*', t, re.M)
    out.append({'cell': fk[1] if fk else f, 'file': f, 'dir': r, 'csType': names.get(k, k)})
json.dump(out, open('C:/Temp/waterplan/custom_cells.json', 'w'), indent=1)
print(len(out), 'custom-type cells saved')
by = collections.Counter(o['dir'].split(chr(92))[0].split('/')[0] for o in out)
print(by)
print(out[:3])

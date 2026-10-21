"""Assign the CS water sounds per water type, put every CS water sound in the CSW audio category (one slider), and add the river / creek
descriptors. Works on COPIES of the Spriggit YAML (csw_now -> csw_snd, cswclean_now -> cswclean_snd); nothing in the game is touched."""
import os, re, shutil, collections, sys

S = os.path.dirname(os.path.abspath(__file__))
RIVER_ID, CREEK_ID = '000D70', '000D71'
MOD = 'CS Water Mod.esp'
SND = {'river': f'{RIVER_ID}:{MOD}', 'creek': f'{CREEK_ID}:{MOD}', 'lake': f'000816:{MOD}', 'ocean': f'000817:{MOD}'}
CAT = f'000814:{MOD}'
DB = -3.1   # dB: the CSW category carries a 0.7 static volume multiplier (20*log10(0.7)); the ambient sounds keep their loudness


def kind(name):
    n = name.lower()
    if any(w in n for w in ('interior', 'ankleheight', 'waistheight', 'cave', 'dungeon', 'lava', 'blood', 'apocrypha', 'volcanic', 'blackreach', 'darkfall', 'ancestorglade',
                            'standingstone', 'sleepingtree', 'horsetrough', 'shadowmere', 'helgen', 'animwater', 'warehouse', '_sb', 'wrwatertype', 'fastflow', 'frostreach')):
        return 'keep'
    if n in ('csw lakewaterne_riverflowblend', 'cswlakewaterne_riverflowblend'):
        return 'keep'                       # lake/river blend keeps the lake sound it has
    if 'puddle' in n and 'creek' not in n:
        return 'silent'
    if 'creek' in n:
        return 'creek'
    if n in ('riverwoodwater', 'cswriverwoodwater'):
        return 'lake'                       # the calm Riverwood lake, not a river
    if 'defaultwater' in n or n.endswith('_lod') or 'cold' in n:
        if 'flowblend' in n or ('flow' in n and 'blend' in n):
            return 'keep'
        return 'ocean'
    if any(w in n for w in ('river', 'wrwaterflow', 'whiterunwaterflow', 'markarthwaterflow', 'markarthwatersystemflow', 'geirwaterflow', 'falkreathwaterflow', 'ivartead')):
        return 'river'
    if any(w in n for w in ('lake', 'pond', 'marsh', 'calm', 'murky', 'clearpine', 'morthal', 'riften', 'markarthwater', 'honrich', 'geirwater', 'deepwood')):
        return 'lake'
    return 'keep'


def setfield(text, key, value):
    """replace or add `key: value` (value None removes it) in a flat Spriggit record text"""
    pat = re.compile(rf'^{key}: .*\n', re.M)
    if value is None:
        return pat.sub('', text, count=1)
    line = f'{key}: {value}\n'
    if pat.search(text):
        return pat.sub(line, text, count=1)
    # new line goes in front of ImageSpace (where OpenSound sits in the vanilla order), else after the Flags block
    m = re.search(r'^ImageSpace: ', text, re.M)
    if m:
        return text[:m.start()] + line + text[m.start():]
    m = re.search(r'^Flags:.*\n(?:- .*\n)*', text, re.M)
    return text[:m.end()] + line + text[m.end():]


changes = collections.Counter()
detail = []
for src, dst in (('csw_now', 'csw_snd'), ('cswclean_now', 'cswclean_snd')):
    shutil.rmtree(os.path.join(S, dst), ignore_errors=True)
    shutil.copytree(os.path.join(S, src), os.path.join(S, dst))
    wdir = os.path.join(S, dst, 'Waters')
    for f in sorted(os.listdir(wdir)):
        p = os.path.join(wdir, f)
        t = open(p, encoding='utf-8', newline='').read()
        nl = '\r\n' if '\r\n' in t else '\n'
        t = t.replace('\r\n', '\n')
        name = re.search(r'^EditorID: (.+?)\s*$', t, re.M)[1]
        k = kind(name)
        cur = re.search(r'^OpenSound: (.+?)\s*$', t, re.M)
        cur = cur[1] if cur else None
        if k == 'keep':
            changes[(dst, 'keep')] += 1
            continue
        new = None if k == 'silent' else SND[k]
        if new == cur:
            changes[(dst, 'already')] += 1
            continue
        t2 = setfield(t, 'OpenSound', new)
        open(p, 'w', encoding='utf-8', newline='').write(t2.replace('\n', nl))
        changes[(dst, k)] += 1
        detail.append((dst, name, k, cur, new))

# --- the CS plugin only: descriptors and the category
d = os.path.join(S, 'csw_snd')
sd = os.path.join(d, 'SoundDescriptors')
for f in os.listdir(sd):
    p = os.path.join(sd, f)
    t = open(p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in t else '\n'
    t = t.replace('\r\n', '\n')
    if 'CSWAMBWaterLake' in f or 'CSWAMBWaterOcean' in f:
        t = re.sub(r'^Category: .*$', f'Category: {CAT}', t, flags=re.M)
        m = re.search(r'^StaticAttenuation: ([\d.]+)', t, re.M)
        t = re.sub(r'^StaticAttenuation: .*$', f'StaticAttenuation: {max(float(m[1]) + DB, 0):.2f}', t, flags=re.M)
        open(p, 'w', encoding='utf-8', newline='').write(t.replace('\n', nl))
        print('descriptor', f, 'category -> CSW, attenuation', m[1], '->', f'{max(float(m[1]) + DB, 0):.2f}')
    if 'CSWAMBWaterLake' in f:
        base, nlb = t, nl
for key, fid, atten, label in (('river', RIVER_ID, 9.0, 'CSWAMBWaterRiver'), ('creek', CREEK_ID, 15.0, 'CSWAMBWaterCreek')):
    t = base
    t = re.sub(r'^FormKey: .*$', f'FormKey: {fid}:{MOD}', t, flags=re.M)
    t = re.sub(r'^EditorID: .*$', f'EditorID: {label}', t, flags=re.M)
    t = re.sub(r'- data\\Sound\\fx\\CSWATER\\water_lake\.wav', r'- data\\Sound\\fx\\CSWATER\\water_river.wav', t)
    t = re.sub(r'^StaticAttenuation: .*$', f'StaticAttenuation: {atten:.2f}', t, flags=re.M)
    out = os.path.join(sd, f'{label} - {fid}_{MOD}.yaml')
    open(out, 'w', encoding='utf-8', newline='').write(t.replace('\n', nlb))
    print('new descriptor', label, fid, 'attenuation', atten)

cp = os.path.join(d, 'SoundCategories')
for f in os.listdir(cp):
    p = os.path.join(cp, f)
    t = open(p, encoding='utf-8', newline='').read()
    t2 = t.replace('Value: CSW-Wave', 'Value: CSW-Water')
    if t2 != t:
        open(p, 'w', encoding='utf-8', newline='').write(t2)
        print('category display name CSW-Wave -> CSW-Water')

print()
for (dst, k), n in sorted(changes.items()):
    print(f'{dst:14s} {k:8s} {n}')
print()
for dst, name, k, cur, new in detail:
    print(f'{dst:13s} {name:34s} {k:7s} {str(cur):24s} -> {new}')

"""Batch 1 (marsh): edit COPIES of the serialized CS plugins. Nothing live is touched."""
import json, os, re, shutil

BS = chr(92)
SRC_MOD = 'C:/Temp/csw_yaml_CSWaterMod'
SRC_PATCH = 'C:/Temp/csw_yaml_CSwaterCleanNeedsPatch'
ST_MOD = 'C:/Temp/waterplan/stage/CSWaterMod'
ST_PATCH = 'C:/Temp/waterplan/stage/CSwaterCleanNeedsPatch'

for s, d in ((SRC_MOD, ST_MOD), (SRC_PATCH, ST_PATCH)):
    if os.path.isdir(d):
        shutil.rmtree(d)
    shutil.copytree(s, d)

plan = json.load(open('C:/Temp/waterplan/revert_plan.json'))
custom = json.load(open('C:/Temp/waterplan/custom_cells.json'))
dirs = {o['cell']: o['dir'] for o in custom}

MORTHAL = ('CSWMorthalWater', 'CSWMorthalWaterFlowBlend')
changed = []
for row in plan:
    if row['csType'] not in MORTHAL:
        continue
    key = row['vanillaKey']
    assert key in ('001232:Update.esm', '105CC3:Skyrim.esm'), row
    p = os.path.join(ST_MOD, 'Worldspaces', dirs[row['cell']], 'RecordData.yaml')
    t = open(p, encoding='utf-8').read()
    n = len(re.findall(r'^Water: .*$', t, re.M))
    assert n == 1, (p, n)
    t2 = re.sub(r'^Water: .*$', 'Water: ' + key, t, flags=re.M)
    open(p, 'w', encoding='utf-8', newline='').write(t2)
    changed.append((row['cell'], row['csType'], key))
print('cells edited:', len(changed))

# marsh type: right-side texture set, flatter third layer (final version lives in the Clean Needs Patch)
p = os.path.join(ST_PATCH, 'Waters', 'DefaultMarshWater - 105CC3_Skyrim.esm.yaml')
t = open(p, encoding='utf-8').read()
base = 'data' + BS + 'textures' + BS + 'water' + BS + 'CustomWater' + BS
want = {'NoiseLayerOneTexture': base + 'oceanwater.dds', 'NoiseLayerTwoTexture': base + 'oceanwater.dds', 'NoiseLayerThreeTexture': base + 'CSWCalmWater.dds'}
for k, v in want.items():
    assert len(re.findall('^' + k + ': .*$', t, re.M)) == 1, k
    t = re.sub('^' + k + ': .*$', lambda m: k + ': ' + v, t, flags=re.M)
open(p, 'w', encoding='utf-8', newline='').write(t)
print('DefaultMarshWater textures set:', want)
json.dump(changed, open('C:/Temp/waterplan/marsh_changed.json', 'w'), indent=1)

"""Batches 2+ (lake/pond/calm/puddle): edit the STAGED copies (batch 1 already applied there)."""
import json, os, re

BS = chr(92)
ST_MOD = 'C:/Temp/waterplan/stage/CSWaterMod'
ST_PATCH = 'C:/Temp/waterplan/stage/CSwaterCleanNeedsPatch'
ST_WF = 'C:/Temp/waterplan/stage/WaterFixes'

plan = json.load(open('C:/Temp/waterplan/revert_plan.json'))
custom = {o['cell']: o for o in json.load(open('C:/Temp/waterplan/custom_cells.json'))}
wf = {r['cell']: r['path'].replace('C:/Temp/wf_yaml', ST_WF) for r in json.load(open('C:/Temp/waterplan/wf_cells.json'))}
DONE = ('CSWMorthalWater', 'CSWMorthalWaterFlowBlend')
OK = ('001232:Update.esm', '105CC3:Skyrim.esm', '105CE1:Skyrim.esm', '062A72:Skyrim.esm')


def setwater(path, key):
    t = open(path, encoding='utf-8').read()
    assert len(re.findall(r'^Water: .*$', t, re.M)) == 1, path
    open(path, 'w', encoding='utf-8', newline='').write(re.sub(r'^Water: .*$', 'Water: ' + key, t, flags=re.M))


n = nw = skipped = 0
for r in plan:
    if r['csType'] in DONE:
        continue
    if r['vanillaKey'] is None:
        skipped += 1
        continue
    assert r['vanillaKey'] in OK, r
    setwater(os.path.join(ST_MOD, 'Worldspaces', custom[r['cell']]['dir'], 'RecordData.yaml'), r['vanillaKey'])
    n += 1
    if r['cell'] in wf:
        setwater(wf[r['cell']], r['vanillaKey'])
        nw += 1
print('CS Water Mod cells edited:', n, ' Water Fixes cells edited:', nw, ' skipped (vanilla inherited worldspace default):', skipped)

base = 'data' + BS + 'textures' + BS + 'water' + BS + 'CustomWater' + BS
sets = {
    'RiverWaterFlow - 001232_Update.esm.yaml': {'NoiseLayerThreeTexture': base + 'CSWCalmWater.dds'},
    'DefaultRiverWater - 105CE1_Skyrim.esm.yaml': {
        'NoiseLayerOneTexture': base + 'oceanwater.dds', 'NoiseLayerTwoTexture': base + 'oceanwater.dds',
        'NoiseLayerThreeTexture': base + 'CSWCalmWater.dds'},
}
for f, want in sets.items():
    p = os.path.join(ST_PATCH, 'Waters', f)
    t = open(p, encoding='utf-8').read()
    for k, v in want.items():
        assert len(re.findall('^' + k + ': .*$', t, re.M)) == 1, (f, k)
        t = re.sub('^' + k + ': .*$', lambda m: k + ': ' + v, t, flags=re.M)
    open(p, 'w', encoding='utf-8', newline='').write(t)
    print('textures set:', f, list(want))

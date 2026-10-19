import json, collections

v = json.load(open('C:/Temp/waterplan/vanilla_cells.json'))
custom = json.load(open('C:/Temp/waterplan/custom_cells.json'))
cells, watr = v['cells'], v['watr']

# sanity checks against what houseCARL showed for the original game
checks = {'0093BA:Skyrim.esm': 'DefaultMarshWater', '0092B4:Skyrim.esm': 'DefaultMarshWaterTransition', '009B55:Skyrim.esm': None, '0096D2:Skyrim.esm': 'RiverWaterFlowNE', '00905B:Skyrim.esm': 'MurkyWater'}
for k, exp in checks.items():
    got = watr.get(cells.get(k)) if cells.get(k) else None
    print('check', k, 'expected', exp, 'got', got, 'OK' if got == exp else 'MISMATCH')

tab = collections.Counter()
rows = []
for o in custom:
    orig = cells.get(o['cell'])
    name = watr.get(orig, orig) if orig else '(none: inherits worldspace default)'
    tab[(o['csType'], name)] += 1
    rows.append({'cell': o['cell'], 'csType': o['csType'], 'vanilla': name, 'vanillaKey': orig})
print()
for (cs, van), n in sorted(tab.items(), key=lambda x: (x[0][0], -x[1])):
    print('%4d  %-32s was  %s' % (n, cs, van))
json.dump(rows, open('C:/Temp/waterplan/revert_plan.json', 'w'), indent=1)

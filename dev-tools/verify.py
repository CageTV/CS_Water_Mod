import os, re, sys


def norm(t):
    t = t.replace('\r\n', '\n')
    return re.sub(r'(?<![0-9.])-0(?![0-9.])', '0', t)


def compare(a, b, label):
    bad = []
    n = 0
    for root, _, files in os.walk(a):
        for f in files:
            pa = os.path.join(root, f)
            pb = os.path.join(b, os.path.relpath(pa, a))
            n += 1
            if not os.path.exists(pb):
                bad.append(('missing', pb))
                continue
            ta = norm(open(pa, encoding='utf-8').read())
            tb = norm(open(pb, encoding='utf-8').read())
            if ta != tb:
                bad.append(('differs', os.path.relpath(pa, a)))
    extra = []
    for root, _, files in os.walk(b):
        for f in files:
            pb = os.path.join(root, f)
            if not os.path.exists(os.path.join(a, os.path.relpath(pb, b))):
                extra.append(os.path.relpath(pb, b))
    print(label, ':', n, 'files compared;', len(bad), 'real differences;', len(extra), 'extra files in rebuilt')
    for x in bad[:10]:
        print('   ', x)
    for x in extra[:10]:
        print('    extra', x)


compare('C:/Temp/waterplan/stage/CSWaterMod', 'C:/Temp/waterplan/verify_mod', 'CS Water Mod.esp')
compare('C:/Temp/waterplan/stage/CSwaterCleanNeedsPatch', 'C:/Temp/waterplan/verify_patch', 'Clean Needs Patch')

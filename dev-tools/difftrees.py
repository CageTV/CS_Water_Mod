"""Compare two Spriggit YAML trees file by file (CRLF and -0 noise ignored). Prints every difference."""
import os, sys, re, difflib

a_root, b_root = sys.argv[1], sys.argv[2]


def norm(t):
    t = t.replace('\r\n', '\n')
    return re.sub(r'(?<![\d.])-0(?![\d.])', '0', t)


def files(root):
    out = {}
    for dp, dn, fn in os.walk(root):
        for f in fn:
            p = os.path.join(dp, f)
            out[os.path.relpath(p, root).replace('\\', '/')] = p
    return out


A, B = files(a_root), files(b_root)
only_a = sorted(set(A) - set(B))
only_b = sorted(set(B) - set(A))
print(f'files: {len(A)} vs {len(B)}')
for f in only_a:
    print('  only in A (original):', f)
for f in only_b:
    print('  only in B (new):     ', f)
changed = 0
for f in sorted(set(A) & set(B)):
    ta, tb = norm(open(A[f], encoding='utf-8', errors='replace', newline='').read()), norm(open(B[f], encoding='utf-8', errors='replace', newline='').read())
    if ta != tb:
        changed += 1
        d = [l for l in difflib.unified_diff(ta.splitlines(), tb.splitlines(), lineterm='', n=0) if not l.startswith(('---', '+++', '@@'))]
        print(f'  CHANGED {f}:')
        for l in d[:6]:
            print('       ', l[:140])
print('changed files:', changed)

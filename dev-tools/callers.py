exec(open('dumpread.py').read().split("md=capstone.Cs")[0])
import re
out=b''
# image bytes in order of module range
parts=[]
for a,s,o in ranges:
    if a>=base and a<base+msize: parts.append((a-base,bytes(m[o:o+s])))
parts.sort()
img=bytearray(msize)
for off,b in parts: img[off:off+len(b)]=b
targets={'30071':tab[30071],'30072':tab[30072]}
print({k:hex(v) for k,v in targets.items()})
import struct
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
for name,t in targets.items():
    hits=[]
    i=0
    while True:
        i=img.find(b'\xE8',i+1)
        if i<0 or i>len(img)-5: break
        rel=struct.unpack_from('<i',img,i+1)[0]
        if i+5+rel==t: hits.append(i)
    print(name,'callers',[hex(h) for h in hits][:12])
    for h in hits[:4]:
        print('--- caller at',hex(h))
        start=h-90
        for ins in md.disasm(bytes(img[start:h+5]),base+start):
            print('   %x %s %s'%(ins.address-base,ins.mnemonic,ins.op_str))

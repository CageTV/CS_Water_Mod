import sys
exec(open('dumpread.py').read().split("md=capstone.Cs")[0])
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
def show(name,i,off=0,n=60,raw=False):
    rva_=tab[i]; code=read(base+rva_+off,3500)
    print('\n==',name,i,hex(rva_),'+',hex(off))
    k=0
    for ins in md.disasm(bytes(code),base+rva_+off):
        print('  %x  %s %s'%(ins.address-base,ins.mnemonic,ins.op_str)); k+=1
        if k>=n: break
for a in sys.argv[1:]:
    i,o,n=(a.split(':')+['0','40'])[:3]
    show('id',int(i),int(o,16) if o.startswith('0x') else int(o),int(n))

import mmap, struct, capstone, sys
P='E:/WorkSpace/skyrim-dumps/SkyrimSE.exe.35664.dmp'
f=open(P,'rb'); m=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
sig,ver,nstreams,dirrva=struct.unpack_from('<4sIII',m,0); print(sig,hex(ver&0xffff),nstreams)
streams={}
for i in range(nstreams):
    t,sz,rva=struct.unpack_from('<III',m,dirrva+12*i); streams[t]=(sz,rva)
# modules
sz,rva=streams[4]; n=struct.unpack_from('<I',m,rva)[0]; base=None
for i in range(n):
    o=rva+4+i*108; b,size,chk,ts,nrva=struct.unpack_from('<QIIII',m,o)
    nl=struct.unpack_from('<I',m,nrva)[0]; name=m[nrva+4:nrva+4+nl].decode('utf-16le')
    if name.lower().endswith('skyrimse.exe'):
        base=b; msize=size; print(name,hex(b),hex(size)); 
        ver=struct.unpack_from('<IIII',m,o+24+8) # VS_FIXEDFILEINFO: sig,strucver,filever_ms,filever_ls
        print('version',ver[2]>>16,ver[2]&0xffff,ver[3]>>16,ver[3]&0xffff)
# memory64
sz,rva=streams[9]; cnt,off=struct.unpack_from('<QQ',m,rva); ranges=[]; cur=off
for i in range(cnt):
    a,s=struct.unpack_from('<QQ',m,rva+16+16*i); ranges.append((a,s,cur)); cur+=s
def read(addr,n):
    for a,s,o in ranges:
        if a<=addr<a+s: return m[o+addr-a:o+addr-a+min(n,a+s-addr)]
    return None
dbin='E:/Tabula Rasa/mods/Address Library for SKSE Plugins/SKSE/Plugins/versionlib-1-7-104-0.bin'
d=open(dbin,'rb').read(); cnt=struct.unpack_from('<i',d,4+16+64+8)[0]; tab=struct.unpack_from('<%dI'%cnt,d,4+16+64+12)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
for name,i in (('GetStageDone',25011),('SetStage',25004),('GetLocation',25067)):
    rva_=tab[i]; code=read(base+rva_,200); print('\n==',name,i,hex(rva_), 'readable' if code else 'NOT IN DUMP')
    if not code: continue
    for k,ins in enumerate(md.disasm(bytes(code),base+rva_)):
        print('  %x  %s %s'%(ins.address-base,ins.mnemonic,ins.op_str))
        if k>=34: break

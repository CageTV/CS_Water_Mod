import numpy as np, os, shutil, struct, sys, time
from PIL import Image
D='C:/Modlists/Tabula/mods/CS Water Mod/textures/water/skyrim.esm/'
ts=time.strftime('%Y%m%d_%H%M%S')
def load(x,y): return np.asarray(Image.open(D+f'flow.{x}.{y}.dds').convert('RGBA')).copy()
def write_dds(path,a):
    h,w,_=a.shape
    # DDS header + DX10 header, R8G8B8A8_UNORM (28), single mip
    hdr=bytearray(128)
    hdr[0:4]=b'DDS '
    struct.pack_into('<I',hdr,4,124)
    struct.pack_into('<I',hdr,8,0x1|0x2|0x4|0x1000|0x8)      # caps, height, width, pixelformat, pitch
    struct.pack_into('<I',hdr,12,h); struct.pack_into('<I',hdr,16,w); struct.pack_into('<I',hdr,20,w*4)
    struct.pack_into('<I',hdr,28,1)                          # mip count
    struct.pack_into('<I',hdr,76,32)                         # pixel format size
    struct.pack_into('<I',hdr,80,0x4)                        # fourcc
    hdr[84:88]=b'DX10'
    struct.pack_into('<I',hdr,108,0x1000)                    # caps: texture
    dx10=struct.pack('<5I',28,3,0,1,0)                       # format, 2D, misc, array size, alpha mode
    open(path,'wb').write(bytes(hdr)+dx10+a.tobytes())
def speed(a): return np.sqrt(np.clip(1.01-a[...,2]/255.0,0,None))
def setspeed(a,s):
    b=np.clip(1.01-s*s,0,1)
    a[...,2]=np.round(b*255).astype(np.uint8)
CALM=0.10; WIDTH=20
apply = len(sys.argv)>1 and sys.argv[1]=='apply'
for (x,y) in ((0,-15),(0,-14)):
    a=load(x,y); s=speed(a); before=(s[:,0].mean(),s[:,5].mean(),s[:,10].mean(),s[:,20].mean())
    new=s.copy()
    for c in range(WIDTH):
        t=c/float(WIDTH); k=t*t*(3-2*t)           # smoothstep 0 at the edge .. 1 at WIDTH
        target=CALM+(s[:,c]-CALM)*k
        new[:,c]=np.minimum(s[:,c],np.maximum(target,CALM)) if False else target
    b=a.copy(); setspeed(b,new)
    after=(speed(b)[:,0].mean(),speed(b)[:,5].mean(),speed(b)[:,10].mean(),speed(b)[:,20].mean())
    print((x,y),'speed at columns 0,5,10,20 before',np.round(before,2),'after',np.round(after,2))
    if apply:
        p=D+f'flow.{x}.{y}.dds'
        shutil.copy2(p,p+f'.bak_pre_seamblend_{ts}')
        write_dds(p,b)
        chk=np.asarray(Image.open(p).convert('RGBA'))
        print('   written; reread speed cols 0,5,10,20',np.round([speed(chk)[:,c].mean() for c in (0,5,10,20)],2),'dir/alpha ok',chk[...,3].min())

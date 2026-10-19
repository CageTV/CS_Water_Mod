import struct, json, re, os, collections, sys
sys.path.insert(0,'.')
import esmcells as E
# per-master: cell fid -> (grid, xcwt, is exterior persistent?) ; parent world via GRUP tracking is complex, use XCLC (only exterior cells have it)
out={}
for m in E.MASTERS:
    data=open(E.DATA+m,'rb').read(); masters=[]
    def norm(raw):
        i=raw>>24; return '%06X:%s'%(raw&0xFFFFFF, masters[i] if i<len(masters) else m)
    def cb(t,fid,flags,body):
        if t=='TES4':
            for st,sb in E.subrecords(body):
                if st=='MAST': masters.append(sb.split(b'\0')[0].decode('latin1'))
            return
        if t!='CELL': return
        g=x=None
        for st,sb in E.subrecords(body):
            if st=='XCLC': g=struct.unpack_from('<ii',sb)
            elif st=='XCWT': x=norm(struct.unpack('<I',sb)[0])
        k=norm(fid)
        d=out.setdefault(k,{'grid':None,'xcwt':None,'src':[]})
        if g: d['grid']=g
        if x: d['xcwt']=x
        d['src'].append(m)
    E.walk(data,0,len(data),cb)
json.dump({k:v for k,v in out.items() if v['grid']},open('cellgrid.json','w'))
for k,v in out.items():
    if v['grid'] in ((31,3),(31,4)): print(k,v)
print(sum(1 for v in out.values() if v['grid']),'exterior cells with grid')

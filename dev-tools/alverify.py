import struct, pefile, capstone, sys
bin_='E:/Tabula Rasa/mods/Address Library for SKSE Plugins/SKSE/Plugins/versionlib-1-7-104-0.bin'
d=open(bin_,'rb').read()
fmt,v0,v1,v2,v3=struct.unpack_from('<5I',d,0); ps,df,cnt=struct.unpack_from('<iii',d,4+16+64)
print('format',fmt,'ver',v0,v1,v2,v3,'count',cnt)
tab=struct.unpack_from('<%dI'%cnt,d,4+16+64+12)
pe=pefile.PE('E:/Tabula Rasa/Stock Game/SkyrimSE.exe')
fi=pe.VS_FIXEDFILEINFO[0]; print('exe version', fi.FileVersionMS>>16, fi.FileVersionMS&0xffff, fi.FileVersionLS>>16, fi.FileVersionLS&0xffff)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); md.detail=False
for name,i in (('GetStageDone',24483),('SetStage',24482),('GetLocation',24538)):
    rva=tab[i]; print('\n==',name,i,hex(rva))
    code=pe.get_data(rva,160)
    n=0
    for ins in md.disasm(code,0x140000000+rva):
        print('  %x  %s %s'%(ins.address-0x140000000,ins.mnemonic,ins.op_str)); n+=1
        if n>=28: break

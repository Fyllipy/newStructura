import struct,re
def ustr_bytes(s): return struct.pack('<I',len(s))+s.encode('utf-16le')
def find_profile(d,series,name,limit=0x70000):
    """Return offset right after the profile-name ustr, searching series header then name."""
    sb=ustr_bytes(series); nb=ustr_bytes(name)
    pos=0; outs=[]
    while True:
        i=d.find(sb,pos,limit)
        if i<0: break
        pos=i+1
        # next ustr should be a description (len 20..100)
        j=i+len(sb)
        ln=struct.unpack_from('<I',d,j)[0] if j+4<=len(d) else 0
        if not (10<=ln<=120): continue
        end=j+4+2*ln
        k=d.find(nb,end,min(limit,end+60000))
        if k>=0:
            outs.append((i,k+len(nb)))
    return outs
def decode(d,p,nraw=4,nflag=12):
    vals=[]
    for _ in range(nraw):
        vals.append(struct.unpack_from('<d',d,p)[0]); p+=8
    for _ in range(nflag):
        f=d[p]; v=struct.unpack_from('<d',d,p+1)[0]; vals.append((f,v)); p+=9
    return vals

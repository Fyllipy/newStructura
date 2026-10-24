import struct
class PE:
    def __init__(self,p):
        self.d=open(p,'rb').read(); d=self.d
        pe=struct.unpack_from('<I',d,0x3c)[0]
        n=struct.unpack_from('<H',d,pe+6)[0]; opt=struct.unpack_from('<H',d,pe+20)[0]
        self.base=struct.unpack_from('<Q',d,pe+24+24)[0]
        o=pe+24+opt; self.secs=[]
        for i in range(n):
            vs,va,rs,ro=struct.unpack_from('<IIII',d,o+8); self.secs.append((va,vs,ro,rs)); o+=40
    def off(self,va):
        r=va-self.base
        for a,vs,ro,rs in self.secs:
            if a<=r<a+max(vs,rs): return ro+r-a
    def u8(self,va): return self.d[self.off(va)]
    def u16(self,va): o=self.off(va); return struct.unpack_from('<H',self.d,o)[0]
    def u32(self,va): o=self.off(va); return struct.unpack_from('<I',self.d,o)[0]

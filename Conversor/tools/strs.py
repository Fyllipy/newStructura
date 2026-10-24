import re,sys
d=open(sys.argv[1],'rb').read()
lo=int(sys.argv[2],0) if len(sys.argv)>2 else 0
hi=int(sys.argv[3],0) if len(sys.argv)>3 else len(d)
for m in re.finditer(rb'(?:[\x20-\x7e\xc0-\xff]\x00){3,}',d[lo:hi]):
    s=m.group().decode('utf-16le'); print(hex(lo+m.start()),repr(s))

import re,sys,json,collections
SP="C:/Users/JOO~1/AppData/Local/Temp/claude/C--Users-Jo-o-Documents-Repos-Conversor/62e829b0-04f7-4a4c-ac8d-ffd81bb16d8a/scratchpad"
def load(path):
    t=open(path,encoding='utf-8',errors='replace').read()
    funcs={}
    for b in t.split("\n===== ")[1:]:
        name=b.split(' ',1)[0]
        body=b.split('-- callees')[0]
        body=body.split('\n',1)[1]
        funcs[name]=body
    return funcs
if __name__=="__main__":
    funcs=load(SP+"/"+sys.argv[1])
    kinds=collections.Counter(); ext=collections.Counter()
    for n,b in funcs.items():
        for m in re.finditer(r'\b(\w+)\(',b):
            nm=m.group(1)
            if nm.startswith('cypesave_') or nm.startswith('FUN_') or nm in('if','while','for','return','sizeof'): kinds[nm]+=1
            elif re.search(r'(graba|salva)',nm): ext[nm]+=1
    print(kinds.most_common(60)); print(ext)
    ifs=[n for n,b in funcs.items() if re.search(r'\bif \(|\bwhile|\bfor \(',re.sub(r'if \([^)]*== (0|\(\w+ \*\)0x0)\) \{\s*error_interno','',b))]
    print(len(funcs),'with control flow:',len(ifs))

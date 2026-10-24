import json,os,subprocess,shutil,sys,re
sys.path.insert(0,'.')
from ed3schema import Lib,SP,APP
MAN='out/manifest.json'
SKIP=set()
GH="C:/Ghidra/support/analyzeHeadless.bat"
def manifest():
    if os.path.exists(MAN): return json.load(open(MAN))
    return {"ed3dlib.dll":{"txt":"closure.txt","names":[]},
     "ghipocal.dll":{"txt":"ext_ghipocal.txt","names":["hipocal_graba"]},
     "dathipfle.dll":{"txt":"ext_dathipfle.txt","names":["dathipfle_graba"]},
     "pilargeo.dll":{"txt":"ext_pilargeo.txt","names":["eppilob_graba","epseccion_graba"]},
     "seccviga.dll":{"txt":"ext_seccviga.txt","names":["svforjcype_graba","svseccion_graba"]},
     "piezamet.dll":{"txt":"ext_piezamet.txt","names":["pzmetstr_graba"]},
     "pzmadera.dll":{"txt":"ext_pzmadera.txt","names":["pzmadob_graba","pzmadinc_graba"]},
     "ambiente.dll":{"txt":"ext_ambiente.txt","names":["ambhorm_graba"]},
     "rstfuego.dll":{"txt":"ext_rstfuego.txt","names":["rfdatmet_graba","rfdathorm_graba"]}}
def save(m): json.dump(m,open(MAN,'w'),indent=1)
idx=json.load(open('out/exportidx.json'))
m=manifest()
for it in range(12):
    libs=[Lib(k,v['txt']) for k,v in m.items()]
    known=set()
    for l in libs: known|=set(l.funcs)
    miss={}
    for l in libs:
        for n in l.funcs:
            for f,a in l.ops(n)[1:]:
                cands=[f]+re.findall(r'(\w+_graba\w*)',a if isinstance(a,str) else '')
                for c in cands:
                    if re.search(r'_graba',c) and not c.startswith('cypesave') and c not in known and c not in SKIP:
                        miss.setdefault(c,l.dll)
    print('iter',it,'missing',len(miss),sorted(miss)[:40],flush=True)
    if not miss: break
    bydll={}
    for f in miss:
        dl=[d for d in idx.get(f,[])]
        if not dl: print('  no export for',f); continue
        bydll.setdefault(dl[0],set()).add(f)
    if not bydll: break
    for dll,names in bydll.items():
        ent=m.setdefault(dll,{"txt":"ext_"+dll[:-4]+".txt","names":[]})
        newnames=sorted(set(ent['names'])|names)
        # import if needed
        if not os.path.exists(SP+"/"+dll):
            shutil.copy(APP+dll,SP+"/"+dll)
            subprocess.run([GH,SP+"/gproj","cype","-import",SP+"/"+dll,"-overwrite"],capture_output=True)
        ent['names']=newnames
        subprocess.run(["bash","ghidra/run.sh",dll,ent['txt'],"name","+".join(newnames),"8"],capture_output=True)
        print('  closure',dll,newnames,flush=True)
    save(m)
save(m)

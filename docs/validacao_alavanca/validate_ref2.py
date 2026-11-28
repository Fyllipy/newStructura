import math,sys
sys.path.insert(0,'.')
from tstub_spec import *
print("== IDEA StatiCa (parametros restantes): CM publicado x especificacao EN (metodo 2; leff = min(b, padrao))")
def run(name,tf,fub,As,db,w,aw,b,cm,rm):
    m=(w-20)/2-0.8*aw*math.sqrt(2); e=(300-w)/2
    ftb=ft_rd_bolt(fub,As,1.25)
    cp,nc=leff_isolated(m,e)
    l1=min(b,cp,nc); l2=min(b,nc)
    dw={16:30,20:37,24:41.6,27:47}.get(db,41.6)
    F,mode,modes,n=tstub_modes(l1,l2,tf,235.0,1.0,m,e,2*ftb,ew=dw/4,method=2)
    print("  %-8s EN %4.0f kN modo %d | CM %4d | EN/CM %.3f | RM %s"%(name,F/1e3,mode,cm,F/1e3/cm,rm))
    return F/1e3/cm
ratios=[]
As={16:157,20:245,24:353,27:459}
for name,db,fub,cm,rm in (("M16 8.8",16,800,152,"-"),("M20 8.8",20,800,205,"-"),("M24 8.8",24,800,270,"-"),("M27 8.8",27,800,278,"-"),("M24 4.8",24,400,164,"-"),("M24 5.8",24,500,190,"-"),("M24 6.8",24,600,217,"-"),("M24 10.9",24,1000,273,"-")):
    ratios.append(run(name,25,fub,As[db],db,165,10,100,cm,rm))
for name,w,aw,b,cm,rm in (("w110",110,7,100,282,344),("w150",150,7,100,188,281),("w200",200,7,100,129,222),("w240",240,7,100,107,163),("b100",110,7,100,314,407),("b250",110,7,250,423,480),("b300",110,7,300,433,486),("b400",110,7,400,433,494)):
    ratios.append(run(name,20,800 if name.startswith('w') else 1000,353,24,w,aw,b,cm,rm))
print("  EN/CM: min %.3f max %.3f"%(min(ratios),max(ratios)))
print()
print("== P398 C.1 - chapa de extremidade (linha 1, extensao): mx=30,4; e=75; ex=50; w=100; tp=25; fy=265; bp=250")
mx=30.4; ex=50.0; e=75.0; w=100.0; bp=250.0
cp=min(2*math.pi*mx, math.pi*mx+w, math.pi*mx+2*e)
nc=min(4*mx+1.25*ex, e+2*mx+0.625*ex, 0.5*bp, 0.5*w+2*mx+0.625*ex)
ft=ft_rd_bolt(800,353,1.25)
F,mode,modes,n=tstub_modes(min(cp,nc),nc,25.0,265.0,1.0,mx,ex,2*ft,ew=39.55/4,method=2,emin=75.0)
print("  leff,cp=%.0f leff,nc=%.0f (publicado 191 e 125) -> F1=%.0f F2=%.0f F3=%.0f -> %.0f kN (publicado 901/377/406 -> 377)"%(cp,nc,modes[1]/1e3,modes[2]/1e3,modes[3]/1e3,F/1e3))

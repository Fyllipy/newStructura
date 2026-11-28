import math, sys
sys.path.insert(0,'.')
from tstub_spec import *
print("=== 1) SCI P398 Exemplo C.1 - mesa do pilar (UKC 254x254x107, S275, fy,c=265, M24 8.8, gM0=1,0, gM2=1,25)")
m=33.4; e=79.4; tf=20.5; fy=265.0; gm0=1.0; ft=ft_rd_bolt(800,353,1.25)/1e3  # kN
ew=39.55/4
def run(leff1,leff2,sumft,label,pub):
    F,mode,modes,n=tstub_modes(leff1,leff2,tf,fy,gm0,m,e,sumft*1e3,ew=ew,method=2,emin=75.0)
    F/=1e3
    print("  %-22s F1=%.0f F2=%.0f F3=%.0f -> Ft,fc,Rd=%.0f kN (modo %d)  publicado: %s  diferenca %.1f%%"%(label,modes[1]/1e3,modes[2]/1e3,modes[3]/1e3,F,mode,pub[0],100*(F/pub[0]-1)))
print("  Ft,Rd parafuso = %.1f kN (publicado 203)"%ft)
cp,nc=leff_isolated(m,e)
run(min(cp,nc),nc,2*ft,"fileira 1 isolada",(398,))
cpg,ncg=leff_group(m,e,[100.0]); run(min(cpg,ncg),ncg,4*ft,"fileiras 1-2",(697,))
cpg,ncg=leff_group(m,e,[100.0,90.0]); run(min(cpg,ncg),ncg,6*ft,"fileiras 1-2-3",(988,))
cpg,ncg=leff_group(m,e,[90.0]); run(min(cpg,ncg),ncg,4*ft,"fileiras 2-3",(691,))
print("  leff publicados: fileira isolada cp=210 nc=233; grupo 1-2: nc=332 cp=410; 1-2-3: Mpl=11,7e6")
print("  leff calculados: isolada cp=%.0f nc=%.0f; 1-2: cp=%.0f nc=%.0f"%(cp,nc,*leff_group(m,e,[100.0])))
print()
print("=== 2) IDEA StatiCa - verificacao de T-stub (MC = metodo dos componentes; RM = MEF solido validado em ensaios)")
# T-stub: S235, bf=300, w=165, tw=20, comprimento b=100, 2 bolts M24 8.8, aw=10 (13,5 para tf>=25)
rows=[("tf10",10,44,115),("tf12",12,63,144),("tf15",15,98,200),("tf20",20,174,269),("tf25",25,279,310),("tf30",30,305,329),("tf35",35,335,347),("tf40",40,371,371),("tf45",45,407,400),("tf50",50,407,407)]
ftb=ft_rd_bolt(800,353,1.25)/1e3
print("  Ft,Rd = %.1f kN por parafuso M24 8.8 (EN: 0,9 fub As/gM2)"%ftb)
for name,t,cm,rm in rows:
    aw=10.0 if t<25 else 13.5
    mm=(165-20)/2-0.8*aw*math.sqrt(2); ee=(300-165)/2
    best=None
    for dw in (41.6,44.0):
        F,mode,modes,n=tstub_modes(100.0,100.0,t,235.0,1.0,mm,ee,2*ftb*1e3,ew=dw/4,method=2)
        if best is None: best=(F/1e3,mode,dw)
    print("  %-5s m=%.2f  EN(Metodo 2, leff=100)= %.0f kN modo %d | MC publicado %d | MEF solido (RM) %d | EN/MC %.2f  EN/RM %.2f"%(name,mm,best[0],best[1],cm,rm,best[0]/cm,best[0]/rm))

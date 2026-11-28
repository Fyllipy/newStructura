import math, sys
sys.path.insert(0,'.')
from tstub_spec import *
ft=ft_rd_bolt(800,353,1.25)  # N, so a mecanica e comparada com a mesma resistencia de parafuso da referencia EN
print("A) NBR 8800:2024 6.3.5.4 aplicado ao T-stub da verificacao IDEA (bf=300, w=165, tw=20, 2 parafusos M24, faixa de 100 mm de comprimento ao longo da alma => p = 100 mm por parafuso; fu=360; gamma_a1=1,0 so para comparar a mecanica)")
rows=[("tf10",10,44,115),("tf12",12,63,144),("tf15",15,98,200),("tf20",20,174,269),("tf25",25,279,310),("tf30",30,305,329),("tf35",35,335,347),("tf40",40,371,371),("tf45",45,407,400),("tf50",50,407,407)]
print("  chapa   b=m(EN)  b=lit.   MC(EN)  RM(MEF)  | NBR 6.3.5.4 com b=m: kN  /MC  /RM | com b literal: kN  /MC  /RM | placa rigida (6.3.5.3, b=m): kN")
for name,t,cm,rm in rows:
    aw=10.0 if t<25 else 13.5
    m=(165-20)/2-0.8*aw*math.sqrt(2); blit=(165-20)/2; e=67.5
    out=[]
    for b in (m,blit):
        f=nbr_ft0_max(t,ft,e,b,100.0,24.0,26.0,360.0,1.0)
        out.append(2*f/1e3)
    # placa rigida: Ft0 max com t_rigid <= t
    lo,hi=1.0,ft
    for _ in range(80):
        mid=0.5*(lo+hi); tr=nbr_t_required(mid,ft,e,m,100.0,24.0,26.0,360.0,1.0)[1]
        (lo,hi)=(mid,hi) if tr<=t else (lo,mid)
    print("  %-5s  %5.1f   %5.1f   %4d   %4d   |  %6.0f   %.2f  %.2f  |  %6.0f   %.2f  %.2f  | %6.0f"%(name,m,blit,cm,rm,out[0],out[0]/cm,out[0]/rm,out[1],out[1]/cm,out[1]/rm,2*lo/1e3))
print()
print("B) NBR 6.3.5.4 aplicado a mesa do pilar do SCI P398 C.1 (UKC 254x254x107; tf=20,5; m=33,4; e=79,4; fu=410; w=100; M24 8.8; gamma_a1=1,0) - comparado ao EN publicado")
b=33.4; a=79.4; tf=20.5; fu=410.0
pk=nbr_tributary(b,(None,None),[])   # fileira isolada
f1=2*nbr_ft0_max(tf,ft,a,b,pk[0],24.0,26.0,fu,1.0)/1e3
print("  fileira 1 isolada: p = %.1f mm -> NBR %.0f kN | EN publicado 398 kN (%.1f%%)"%(pk[0],f1,100*(f1/398-1)))
# grupos: tributario com vizinhas; fileira topo/base com bordas 'infinitas'
for label,sp,pub in (("fileiras 1-2",[100.0],697),("fileiras 1-2-3",[100.0,90.0],988),("fileiras 2-3",[90.0],691)):
    ps=nbr_tributary(b,(None,None),sp)
    tot=sum(2*nbr_ft0_max(tf,ft,a,b,p,24.0,26.0,fu,1.0)/1e3 for p in ps)
    print("  %-15s p_i=%s -> NBR (soma das fileiras com largura tributaria) %.0f kN | EN publicado (grupo) %d kN (%.1f%%)"%(label,[round(x,1) for x in ps],tot,pub,100*(tot/pub-1)))
print("  soma das fileiras ISOLADAS (p=3,5b cada), para mostrar o erro de ignorar a interacao: 3 x %.0f = %.0f kN (EN grupo 988)"%(f1,3*f1))

print()
print("C) Varredura de espessura da mesa - fileira isolada (UKC 254x254x107; m=33,4; e=79,4; fu=410, fy=265): EN (P398, metodo 2) x NBR 6.3.5.4 (p = 3,5 b) , gamma = 1,0")
print("   tf    EN modo  EN kN | NBR kN  NBR/EN")
for tfx in (8,10,12,14,16,18,20.5,25):
    cp,nc=leff_isolated(33.4,79.4)
    F,mode,modes,n=tstub_modes(min(cp,nc),nc,tfx,265.0,1.0,33.4,79.4,2*ft,ew=39.55/4,method=2,emin=75.0)
    nb=2*nbr_ft0_max(tfx,ft,79.4,33.4,pk[0],24.0,26.0,410.0,1.0)
    print("  %5.1f   %d    %6.0f |  %6.0f   %.2f"%(tfx,mode,F/1e3,nb/1e3,nb/F))
print()
print("D) Varredura - grupo de 3 fileiras (p1-2=100, p2-3=90), tributario NBR por fileira x EN grupo (P398 metodo 2)")
print("   tf    EN grupo kN | NBR soma kN  NBR/EN")
ps=nbr_tributary(33.4,(None,None),[100.0,90.0])
for tfx in (8,10,12,14,16,18,20.5,25):
    cpg,ncg=leff_group(33.4,79.4,[100.0,90.0])
    F,mode,modes,n=tstub_modes(min(cpg,ncg),ncg,tfx,265.0,1.0,33.4,79.4,6*ft,ew=39.55/4,method=2,emin=75.0)
    nb=sum(2*nbr_ft0_max(tfx,ft,79.4,33.4,p,24.0,26.0,410.0,1.0) for p in ps)
    print("  %5.1f    %6.0f   |  %6.0f    %.2f"%(tfx,F/1e3,nb/1e3,nb/F))

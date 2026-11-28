import math,sys
sys.path.insert(0,'.')
from tstub_spec import *
tf=15.0; tw=9.0; r=18.0; bf=200.0; g=90.0; pitch=70.0
m=(g-tw)/2-0.8*r; blit=(g-tw)/2; e=(bf-g)/2
print("HEB200: m=%.1f (EN, com 0,8 r)  b literal=%.1f  e=%.1f"%(m,blit,e))
print("\n== Camada B (EN 1993-1-8, gM0=1,00, gM2=1,25; parafuso M20 8.8 [fub=800], Ft,Rd=0,9 fub As/gM2; ew=dw/4, dw=33 mm)")
ft=ft_rd_bolt(800,245,1.25)
print("   Ft,Rd por parafuso = %.1f kN"%(ft/1e3))
for fy in (250.0,355.0):
    cp,nc=leff_isolated(m,e)
    F,mode,modes,n=tstub_modes(min(cp,nc),nc,tf,fy,1.0,m,e,2*ft,ew=33/4,method=2)
    print("  fy=%.0f  1 fileira: leff1=%.1f leff2=%.1f  F1=%.0f F2=%.0f F3=%.0f -> %.0f kN modo %d"%(fy,min(cp,nc),nc,modes[1]/1e3,modes[2]/1e3,modes[3]/1e3,F/1e3,mode))
    cpg,ncg=leff_group(m,e,[pitch])
    Fg,mg,modesg,_=tstub_modes(min(cpg,ncg),ncg,tf,fy,1.0,m,e,4*ft,ew=33/4,method=2)
    print("          2 fileiras (grupo, p=70): leff1=%.1f leff2=%.1f F1=%.0f F2=%.0f F3=%.0f -> grupo %.0f kN modo %d; 2 x isolada = %.0f -> governa %.0f kN"%(min(cpg,ncg),ncg,modesg[1]/1e3,modesg[2]/1e3,modesg[3]/1e3,Fg/1e3,mg,2*F/1e3,min(Fg,2*F)/1e3))
print("\n== Camada A (NBR 8800:2024 6.3.5; Ft,Rd=0,75 Ab fub/1,35 = 144,86 kN; gamma_a1=1,10; fu=400 MPa, tf=15; df=22)")
ftn=0.75*314.159*830/1.35
for label,b in (("b = m (0,8 r; hipotese complementar)",m),("b literal (face da alma)",blit)):
    ps=nbr_tributary(b,(None,None),[])
    f1=2*nbr_ft0_max(tf,ftn,e,b,ps[0],20.0,22.0,400.0,1.10)
    ps2=nbr_tributary(b,(None,None),[pitch])
    f2=sum(2*nbr_ft0_max(tf,ftn,e,b,p,20.0,22.0,400.0,1.10) for p in ps2)
    print("  %-40s 1 fileira: p=%.1f -> %.0f kN | 2 fileiras (p_i=%s) -> %.0f kN"%(label,ps[0],f1/1e3,[round(x,1) for x in ps2],f2/1e3))
print("\n== Structura hoje (sonda anterior): 1 fileira aceita 289,7 kN; 2 fileiras 319,6 kN (5.7.2 contra T total)")

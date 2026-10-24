import sys
sys.path.insert(0, '..')
from cype2structura.profiles import to_section

tests = [("Anexo Veloo 2", [('W', 'W150X22.5'), ('W', 'W200X35.9')]),
         ("CoberAcadCambuci", [('L', 'L 5/8 X 1/8'), ('UPE', 'UPE 80'), ('U', 'U100X50X3.04')]),
         ("Restaurante", [('Maciça h160', 'S-160x40')]),
         ("Mercado JP", [('Barra redonda', '3/8"')])]
for f, lst in tests:
    d = open("D:/CYPE Ingenieros/Projetos/CYPE 3D/%s.ed3" % f, 'rb').read()
    for sn in lst:
        try:
            s = to_section(d, *sn)
            print(f, sn, s.family, s.params, "A=%.3f cm2" % (s.area * 1e4), "Iz=%.4g Iy=%.4g J=%.4g" % (s.iz, s.iy, s.j), s.source)
        except Exception as e:
            print(f, sn, 'ERRO', e)

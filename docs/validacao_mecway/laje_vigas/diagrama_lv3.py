# Diagrama de momento fletor (sagrante > 0) da viga y=0 do LV3: Structura (modos T e A, varias lajes) x pórtico analítico (carga uniforme e cargas nodais).
import os
import sys
import check_analitico as c

root, out = sys.argv[1], sys.argv[2]
L, h, w = 6.0, 3.0, 10000.0
ref = c.portal(w, L, h)
refn = c.portal_points([(float(k), 10000.0) for k in range(1, 6)], L, h)


def diagram(case, mode):
    bars = c.bars_of(os.path.join(root, case))
    f = c.forces_of(os.path.join(root, case), mode)
    ids = c.chain(bars, c.line_elements(bars, y=0.0))
    vals = [f[i][5] for i in ids] + [-f[ids[-1]][11]]
    s = 1.0 if vals[len(vals) // 2] >= 0 else -1.0
    return [s * v for v in vals]


series = {
    'T (quinhoes)': diagram('LV3_laje_unidirecional_2vigas', 'T'),
    'A t=150 mm': diagram('LV3_laje_unidirecional_2vigas', 'A'),
    'A t=20 mm': diagram('LV3t20', 'A'),
    'A t=20 mm nu=0': diagram('LV3t1020', 'A'),
}
with open(os.path.join(out, 'diagrama_lv3.csv'), 'w') as fo:
    fo.write('x,analitico_uniforme,analitico_nodal,' + ','.join(series) + '\n')
    for k in range(7):
        x = float(k)
        au = w * x * (L - x) / 2.0 - ref['M_apoio']
        # carga nodal: viga biapoiada com cargas de 10 kN em x=1..5 menos o momento de engaste
        pts = [(float(j), 10000.0) for j in range(1, 6)]
        m0 = 25000.0 * x - sum(P * (x - xp) for xp, P in pts if xp < x)
        an = m0 - refn['M_apoio']
        fo.write('%g,%.1f,%.1f,%s\n' % (x, au, an, ','.join('%.1f' % series[n][k] for n in series)))
print(open(os.path.join(out, 'diagrama_lv3.csv')).read())

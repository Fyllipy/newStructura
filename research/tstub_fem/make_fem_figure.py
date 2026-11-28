"""Gera docs/validation/figures/tstub_fem_stiffness.svg a partir de results_v2_variantes.json: rigidez MEF x Piluso/Faella x EN, em funcao de b, tf e m.
Uso: PYTHONPATH=<esta pasta>;<../tstub_piluso> python make_fem_figure.py"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tstub_piluso'))
HERE = os.path.dirname(os.path.abspath(__file__))
sys.argv = ['x', 'none', 'none']
import importlib.util
spec = importlib.util.spec_from_file_location('rs', os.path.join(HERE, 'run_study.py'))
rs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rs)

res = json.load(open(os.path.join(HERE, 'results_v2_variantes.json')))


def series(prefix, key):
    pts = []
    for name, v in res.items():
        if name.startswith(prefix):
            p = v['p']
            pts.append((p[key], v['fem']['K'], rs.formulas(p)))
    return sorted(pts, key=lambda t: t[0])


panels = [('b (mm)', 'var_b', 'b'), ('tf (mm)', 'var_tf', 'tf'), ('m (mm)', 'var_m', 'm')]
W, H = 1020, 330
svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="Segoe UI, Arial, sans-serif" font-size="11">' % (W, H), '<rect width="100%" height="100%" fill="#fff"/>',
       '<text x="10" y="18" font-size="14" font-weight="600">Rigidez do T-stub (kN/mm): MEF elástico 3D x Piluso/Faella x EN (Fase B) — placa sem filete, hipóteses declaradas</text>']
for idx, (label, prefix, key) in enumerate(panels):
    pts = series('v2_' + prefix, key) or series(prefix, key)
    if not pts:
        continue
    ox, oy, aw, ah = 50 + idx * 335, 50, 270, 220
    xmin, xmax = min(t[0] for t in pts), max(t[0] for t in pts)
    ymax = 1.1 * max(max(t[1], t[2]['EN_beff'], t[2]['Ki_P']) for t in pts)
    sx = lambda x: ox + aw * (x - xmin) / (xmax - xmin if xmax > xmin else 1)
    sy = lambda y: oy + ah - ah * y / ymax
    svg.append('<rect x="%d" y="%d" width="%d" height="%d" fill="none" stroke="#999"/>' % (ox, oy, aw, ah))
    for name, color, getter in (('MEF', '#222', lambda t: t[1]), ('Piluso K_i', '#1f6fb5', lambda t: t[2]['Ki_P']), ('EN (l_eff=b_eff)', '#c0392b', lambda t: t[2]['EN_beff'])):
        svg.append('<polyline fill="none" stroke="%s" stroke-width="2" points="%s"/>' % (color, ' '.join('%.1f,%.1f' % (sx(t[0]), sy(getter(t))) for t in pts)))
        for t in pts:
            svg.append('<circle cx="%.1f" cy="%.1f" r="3" fill="%s"/>' % (sx(t[0]), sy(getter(t)), color))
    svg.append('<text x="%d" y="%d" fill="#555">%s</text><text x="%d" y="%d" fill="#555">%.0f</text><text x="%d" y="%d" fill="#555" text-anchor="end">%.0f</text>' % (ox + aw // 2 - 20, oy + ah + 30, label, ox, oy + ah + 14, xmin, ox + aw, oy + ah + 14, xmax))
    svg.append('<text x="%d" y="%d" fill="#555" text-anchor="end">%.0f</text>' % (ox - 4, oy + 8, ymax))
for i, (name, color) in enumerate((('MEF (modelo v2: arruela em contato, furo)', '#222'), ('Piluso/Faella K_i (r=0)', '#1f6fb5'), ('EN Fase B (l_eff = b_eff)', '#c0392b'))):
    svg.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="2"/><text x="%d" y="%d">%s</text>' % (50 + i * 330, 305, 80 + i * 330, 305, color, 86 + i * 330, 309, name))
svg.append('</svg>')
os.makedirs(os.path.join(HERE, '..', '..', 'docs', 'validation', 'figures'), exist_ok=True)
open(os.path.join(HERE, '..', '..', 'docs', 'validation', 'figures', 'tstub_fem_stiffness.svg'), 'w', encoding='utf-8').write('\n'.join(svg))
print('ok')

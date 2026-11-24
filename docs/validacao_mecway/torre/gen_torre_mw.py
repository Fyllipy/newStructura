# Modelos EQUIVALENTES do Mecway 28 (.liml) da torre: mesma geometria, mesmos perfis (propriedades efetivas lidas do solver do Structura), mesmas cargas
# (peso proprio por gravidade, antenas e vento em forcas nodais), apoios articulados na base. Tres analises: estatica (S30), flambagem (B30) e vibracao (V30).
# uso: python gen_torre_mw.py <pasta_torre(geom.csv)> <pasta_resultados_structura(sections_effective.csv, material_effective.csv)> <pasta_saida>
import csv
import ctypes
import os
import sys

geom_dir, eff_dir, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)


def short_path(path):
    path = os.path.abspath(path)
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, 1024)
    return buf.value if n else path


nodes, members, ant, wind = {}, [], [], []
for r in csv.reader(open(os.path.join(geom_dir, 'torre_geom.csv'))):
    if r[0] == 'N':
        nodes[int(r[1])] = (float(r[2]), float(r[3]), float(r[4]))
    elif r[0] == 'M':
        members.append((int(r[1]), int(r[2]), int(r[3]), int(r[4])))
    elif r[0] == 'LA':
        ant.append((int(r[1]), float(r[2]), float(r[3]), float(r[4])))
    elif r[0] == 'LW':
        wind.append((int(r[1]), float(r[2]), float(r[3]), float(r[4])))
sec = {}
for r in csv.reader(open(os.path.join(eff_dir, 'sections_effective.csv'))):
    sec[int(r[0])] = [float(v) for v in r[1:5]]   # A, Iy, Iz, J
mat = [float(v) for v in next(csv.reader(open(os.path.join(eff_dir, 'material_effective.csv'))))]
E_kn, G_kn, density = mat[0], mat[1], mat[2]
E_pa = E_kn * 1000.0
nu = E_kn / (2.0 * G_kn) - 1.0
RHO = 7850.0
print('E %.4g Pa, nu %.4f, densidade do solver %.6g (usada no Mecway: %g kg/m3)' % (E_pa, nu, density, RHO))
base = [n for n, p in nodes.items() if p[2] < 1e-9]
forces = {}
for n, fx, fy, fz in ant + wind:
    f = forces.setdefault(n, [0.0, 0.0, 0.0])
    f[0] += fx * 1000.0
    f[1] += fy * 1000.0
    f[2] += fz * 1000.0


def liml(kind, maxmode, shift, with_loads, gravity):
    x = ['<liml version="27">', '  <analysis type="%s"%s />' % (kind, (' maxmode="%d" shift="%s"' % (maxmode, shift)) if maxmode else ''), '  <incompatiblehex8>true</incompatiblehex8>']
    for n, (px, py, pz) in nodes.items():
        x.append('  <node nid="%d" x="%.9g" y="%.9g" z="%.9g" />' % (n, px, py, pz))
    used = sorted(set(m[3] for m in members))
    for s in used:
        x.append('  <elset name="Perfil_%d" colorrgb="DADADA" material="Mat_%d">' % (s, s))
        for eid, (a, b, sid) in enumerate((m[1:4] for m in members), 1):
            pass
        for mid, a, b, sid in members:
            if sid == s:
                x.append('    <elem eid="%d" shape="line2" nodes="%d %d" />' % (mid, a, b))
        x.append('  </elset>')
    for ax, label in enumerate('xyz'):
        comp = ['0', '0', '0']
        comp[ax] = '1'
        x.append('  <displacement selection="Base"><value>0 m</value><x>%s</x><y>%s</y><z>%s</z></displacement>' % tuple(comp))
    if with_loads:
        for n, (fx, fy, fz) in forces.items():
            x.append('  <force selection="F_%d"><x>%.9g N</x><y>%.9g N</y><z>%.9g N</z></force>' % (n, fx, fy, fz))
        if gravity:
            x.append('  <gravity><x>0 m/s&#178;</x><y>0 m/s&#178;</y><z>-9.80665 m/s&#178;</z></gravity>')
    for s in used:
        a, iy, iz, j = sec[s]
        # regra de eixos do Mecway (conferida por experimento): deslocamento no plano vertical da barra usa iyy, na horizontal usa izz; no Structura e o contrario (Iz e o eixo do plano vertical)
        _mode = os.environ.get('TOWER_MW_MODE', 'swap')
        if _mode == 'swap':
            iy, iz = iz, iy
        elif _mode == 'min':
            iy = iz = min(iy, iz)
        elif _mode == 'max':
            iy = iz = max(iy, iz)
        x.append('  <mat name="Mat_%d"><geometric type="UniformGeneralSection" area="%.9e m&#178;" iyy="%.9e m&#8308;" izz="%.9e m&#8308;" torsionconstant="%.9e m&#8308;" />'
                 '<mechanical type="Isotropic" youngsmodulus="%.9e Pa" poissonratio="%.6f" /><density type="Simple" density="%g kg/m&#179;" /></mat>' % (s, a, iy, iz, j, E_pa, nu, RHO))
    x.append('  <nodeselection name="Base" direct="true">%s</nodeselection>' % ''.join('<node nid="%d" />' % n for n in base))
    if with_loads:
        for n in forces:
            x.append('  <nodeselection name="F_%d" direct="true"><node nid="%d" /></nodeselection>' % (n, n))
    x.append('  <fieldvariable name="displacement" />')
    x.append('  <fieldvariable name="rotation" />')
    x.append('  <fieldvariable name="reaction force" />')
    x.append('</liml>')
    return '\n'.join(x)


def extract(kind, name):
    d = short_path(out).replace('\\', '/')
    multi = kind != 'S30'
    py = '''import csv
mw.solve()
mw.set_solution_active()
modes = %s
res = open(r'%s/%s_mw_result.txt', 'w')
for k in modes:
    res.write(%s)
res.close()
for k in modes:
    f = open(r'%s/%s_mw_mode%%s.csv' %% (k if k is not None else 0), 'wb')
    w = csv.writer(f)
    for nid in sorted(solution.all_nodes()):
        row = [nid]
        for var in ('displx', 'disply', 'displz', 'rotx', 'roty', 'rotz', 'reactionforcex', 'reactionforcey', 'reactionforcez'):
            try:
                v = solution.node_value(var, nid, None, k)
            except Exception as e:
                v = None
            row.append('0' if v is None else repr(v))
        w.writerow(row)
    f.close()
''' % ('solution.modes()' if multi else '[None]', d, name,
       ("'fator%d=%r\\n' % (k, solution.buckling_factor(k))" if kind == 'B30' else ("'f%d=%r\\n' % (k, solution.frequency(k))" if kind == 'V30' else "'ok\\n'")), d, name)
    assert all(ord(ch) < 128 for ch in py)
    return py


for kind, name, maxmode, shift, loads, grav in (('S30', 'torre_estatica', 0, '', True, True), ('B30', 'torre_flambagem', 4, '1', True, True), ('V30', 'torre_modal', 6, '0', False, False)):
    open(os.path.join(out, name + '.liml'), 'w', encoding='utf-8').write(liml(kind, maxmode, shift, loads, grav))
    open(os.path.join(out, name + '_extract.py'), 'w').write(extract(kind, name))
    print('gerado', name)

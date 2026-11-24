# Modelo do Mecway 28 SO COM BARRAS 1D (line2) para o modo de quinhoes (T) do Structura: mesma geometria e perfil, mesmas cargas que a laje
# aplicou as barras (carga distribuida por barra e forcas nodais). A carga distribuida uniforme de cada barra e reproduzida com peso proprio
# por elemento (densidade x area x g = q), que o Mecway integra de forma consistente; as forcas nodais entram como forcas.
# uso: python gen_lb.py <pasta_cases> <pasta_saida>
import csv
import ctypes
import os
import sys

G = 9.81
OUT_FIELDS = ['displx', 'disply', 'displz', 'rotx', 'roty', 'rotz', 'reactionforcex', 'reactionforcey', 'reactionforcez', 'reactionmomx', 'reactionmomy', 'reactionmomz']


def short_path(path):
    path = os.path.abspath(path)
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, 1024)
    return buf.value if n else path


def read(path):
    if not os.path.exists(path):
        return []
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def meta(path):
    d = {}
    for line in open(path, encoding='utf-8'):
        if '=' in line:
            k, v = line.rstrip('\n').split('=', 1)
            d[k] = v
    return d


def key(x, y, z):
    return (round(float(x) * 1e6), round(float(y) * 1e6), round(float(z) * 1e6))


def generate(case_dir, out_dir):
    name = os.path.basename(case_dir.rstrip('/\\'))
    m = meta(os.path.join(case_dir, 'meta.txt'))
    side = float(m['side'])
    area = side * side
    bars = read(os.path.join(case_dir, 'bars.csv'))
    tdir = os.path.join(case_dir, 'modo_T')
    qz = {int(r[0]): float(r[3]) for r in read(os.path.join(tdir, 'bar_loads.csv'))}
    nodal = read(os.path.join(tdir, 'nodal_loads.csv'))
    bases = read(os.path.join(case_dir, 'bases.csv'))
    ids, coords = {}, []

    def nid(x, y, z):
        k = key(x, y, z)
        if k not in ids:
            coords.append((float(x), float(y), float(z)))
            ids[k] = len(coords)
        return ids[k]

    elems = []
    for b in bars:
        n1, n2 = nid(b[1], b[2], b[3]), nid(b[4], b[5], b[6])
        elems.append((int(b[0]), n1, n2))
    x = ['<liml version="27">', '  <analysis type="S30" />', '  <incompatiblehex8>true</incompatiblehex8>']
    for i, (cx, cy, cz) in enumerate(coords):
        x.append('  <node nid="%d" x="%r" y="%r" z="%r" />' % (i + 1, cx, cy, cz))
    # um conjunto (e um material) por elemento: a densidade carrega a carga distribuida da barra
    for eid, n1, n2 in elems:
        x.append('  <elset name="B%d" colorrgb="FFB75B" material="M%d">' % (eid, eid))
        x.append('    <elem eid="%d" shape="line2" nodes="%d %d" orientationvy="1" orientationvz="1" angle="0 &#176;" />' % (eid, n1, n2))
        x.append('  </elset>')
    first_node = {eid: n1 for eid, n1, n2 in elems}
    base_faces = []
    for bz in bases:
        n = ids[key(*bz)]
        for eid, n1, n2 in elems:
            if n1 == n:
                base_faces.append((eid, 2))   # face 2 = extremidade inicial (base do pilar)
                break
    x.append('  <fixed selection="faces_fixed" />')
    x.append('  <gravity><x>0 m/s&#178;</x><y>0 m/s&#178;</y><z>-%r m/s&#178;</z></gravity>' % G)
    # forcas nodais (cargas por clique)
    sel = []
    for i, r in enumerate(nodal):
        n = ids[key(r[1], r[2], r[3])]
        sel.append((i + 1, n))
        x.append('  <force selection="fn%d"><x>%r N</x><y>%r N</y><z>%r N</z></force>' % (i + 1, float(r[4]), float(r[5]), float(r[6])))
    for eid, n1, n2 in elems:
        q = -qz.get(eid, 0.0)   # N/m para baixo
        rho = q / (area * G)
        x.append('  <mat name="M%d"><geometric type="RectBar" recta="%r m" rectb="%r m" /><mechanical type="Isotropic" youngsmodulus="%r Pa" poissonratio="%r" /><density type="Simple" density="%r kg/m&#179;" /></mat>'
                 % (eid, side, side, float(m['E']), float(m['nu']), rho if rho > 0 else 1e-12))
    x.append('  <faceselection name="faces_fixed" direct="true">%s</faceselection>' % ''.join('<face eid="%d" faceid="%d" />' % bf for bf in base_faces))
    for i, n in sel:
        x.append('  <nodeselection name="fn%d" direct="true"><node nid="%d" /></nodeselection>' % (i, n))
    for v in ['displacement', 'rotation', 'reaction force', 'reaction moment']:
        x.append('  <fieldvariable name="%s" />' % v)
    x.append('</liml>')
    open(os.path.join(out_dir, name + '_bars.liml'), 'w', encoding='utf-8').write('\n'.join(x))
    nodes_csv = os.path.join(short_path(out_dir), name + '_bars_mw_nodes.csv').replace('\\', '/')
    py = '''import csv
mw.solve()
mw.set_solution_active()
def val(var, nid):
    try:
        v = solution.node_value(var, nid, None, None)
    except Exception as e:
        return ''
    return '' if v is None else repr(v)
OUT_FIELDS = %r
f = open(r'%s', 'wb')
w = csv.writer(f)
w.writerow(['nid', 'px', 'py', 'pz'] + OUT_FIELDS)
for nid in sorted(solution.all_nodes()):
    pos = solution.node(nid)
    w.writerow([nid, repr(pos.x), repr(pos.y), repr(pos.z)] + [val(v, nid) for v in OUT_FIELDS])
f.close()
''' % (OUT_FIELDS, nodes_csv)
    assert all(ord(ch) < 128 for ch in py)
    open(os.path.join(out_dir, name + '_bars_extract.py'), 'w').write(py)
    return name


if __name__ == '__main__':
    cases, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for d in sorted(os.listdir(cases)):
        if os.path.isdir(os.path.join(cases, d)) and os.path.exists(os.path.join(cases, d, 'modo_T', 'bar_loads.csv')):
            print('gerado', generate(os.path.join(cases, d), out))

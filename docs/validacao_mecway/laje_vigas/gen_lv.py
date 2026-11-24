# Gera o modelo EQUIVALENTE do Mecway 28 (.liml) de cada caso laje+vigas+pilares exportado pelo lv_dump:
# casca quad4 (laje) + barras line2 (vigas e pilares) compartilhando nos (ligacao monolitica = modo M do Structura).
# uso: python gen_lv.py <pasta_cases> <pasta_saida> [face_base]
import csv
import ctypes
import os
import sys


def short_path(path):
    path = os.path.abspath(path)
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, 1024)
    return buf.value if n else path


OUT_FIELDS = ['displx', 'disply', 'displz', 'rotx', 'roty', 'rotz', 'reactionforcex', 'reactionforcey', 'reactionforcez', 'reactionmomx', 'reactionmomy', 'reactionmomz']


def read_csv(path):
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


def generate(case_dir, out_dir, face_base):
    name = os.path.basename(case_dir.rstrip('/\\'))
    m = meta(os.path.join(case_dir, 'meta.txt'))
    nodes = read_csv(os.path.join(case_dir, 'nodes.csv'))
    quads = read_csv(os.path.join(case_dir, 'quads.csv'))
    bars = read_csv(os.path.join(case_dir, 'bars.csv'))
    bases = read_csv(os.path.join(case_dir, 'bases.csv'))
    ids = {}
    coords = []

    def nid(x, y, z):
        k = key(x, y, z)
        if k not in ids:
            coords.append((x, y, z))
            ids[k] = len(coords)
        return ids[k]

    shell_nodes = [nid(r[1], r[2], r[3]) for r in nodes]
    side = float(m['side'])
    t = float(m['thickness'])
    p = float(m['pressure'])
    x = ['<liml version="27">', '  <analysis type="S30" />', '  <incompatiblehex8>true</incompatiblehex8>']
    elems = []   # (eid, shape, nodes, material)
    eid = 0
    for q in quads:
        eid += 1
        elems.append((eid, 'quad4', [shell_nodes[int(v)] for v in q[1:5]], 'Laje'))
    shell_elems = list(range(1, eid + 1))
    column_first = {}
    for b in bars:
        n1 = nid(b[1], b[2], b[3])
        n2 = nid(b[4], b[5], b[6])
        eid += 1
        elems.append((eid, 'line2', [n1, n2], 'Barra'))
        column_first[n1] = eid
    for i, (cx, cy, cz) in enumerate(coords):
        x.append('  <node nid="%d" x="%r" y="%r" z="%r" />' % (i + 1, float(cx), float(cy), float(cz)))
    for mat, color in (('Laje', 'DADADA'), ('Barra', 'FFB75B')):
        x.append('  <elset name="%s" colorrgb="%s" expanded="true" material="%s">' % (mat, color, mat))
        for e, shape, ns, em in elems:
            if em != mat:
                continue
            if shape == 'quad4':
                x.append('    <elem eid="%d" shape="quad4" nodes="%s" />' % (e, ' '.join(str(v) for v in ns)))
            else:
                x.append('    <elem eid="%d" shape="line2" nodes="%s" orientationvy="1" orientationvz="1" angle="0 &#176;" />' % (e, ' '.join(str(v) for v in ns)))
        x.append('  </elset>')
    # engaste: face de extremidade (base) de cada pilar
    base_faces = []
    for bz in bases:
        n = ids[key(*bz)]
        base_faces.append((column_first[n], face_base))
    x.append('  <fixed selection="faces_fixed" />')
    x.append('  <surfacepressure selection="faces_pressure"><normal>%r Pa</normal></surfacepressure>' % p)
    x.append('  <mat name="Laje"><geometric type="Plate" thickness="%r m" /><mechanical type="Isotropic" youngsmodulus="%r Pa" poissonratio="%r" /></mat>' % (t, float(m['E']), float(m['nu'])))
    x.append('  <mat name="Barra"><geometric type="RectBar" recta="%r m" rectb="%r m" /><mechanical type="Isotropic" youngsmodulus="%r Pa" poissonratio="%r" /></mat>' % (side, side, float(m['E']), float(m['nu'])))
    x.append('  <faceselection name="faces_fixed" direct="true">%s</faceselection>' % ''.join('<face eid="%d" faceid="%d" />' % bf for bf in base_faces))
    x.append('  <faceselection name="faces_pressure" direct="true">%s</faceselection>' % ''.join('<face eid="%d" faceid="6" />' % e for e in shell_elems))
    for v in ['displacement', 'rotation', 'reaction force', 'reaction moment']:
        x.append('  <fieldvariable name="%s" />' % v)
    x.append('</liml>')
    liml_path = os.path.join(out_dir, name + '.liml')
    open(liml_path, 'w', encoding='utf-8').write('\n'.join(x))
    nodes_csv = os.path.join(short_path(out_dir), name + '_mw_nodes.csv').replace('\\', '/')
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
    open(os.path.join(out_dir, name + '_extract.py'), 'w').write(py)
    return name


if __name__ == '__main__':
    cases_dir, out_dir = sys.argv[1], sys.argv[2]
    face_base = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    os.makedirs(out_dir, exist_ok=True)
    for d in sorted(os.listdir(cases_dir)):
        if os.path.isdir(os.path.join(cases_dir, d)):
            print('gerado', generate(os.path.join(cases_dir, d), out_dir, face_base))

# Modelo EQUIVALENTE do Mecway 28 (.liml) da casca de Scordelis-Lo exportada pelo sl_dump: mesma malha, restricoes de translacao por no e a carga de
# area como PESO (densidade x espessura x g = 90 N/m2 de casca, gravidade em -Z). Script de extracao dos deslocamentos nodais.
# uso: python gen_sl.py <pasta_cases> <pasta_saida>
import csv
import ctypes
import os
import sys

G = 9.81
OUT_FIELDS = ['displx', 'disply', 'displz', 'rotx', 'roty', 'rotz']


def short_path(path):
    path = os.path.abspath(path)
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, 1024)
    return buf.value if n else path


def read(path):
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def meta(path):
    d = {}
    for line in open(path, encoding='utf-8'):
        if '=' in line:
            k, v = line.rstrip('\n').split('=', 1)
            d[k] = v
    return d


def generate(case_dir, out_dir):
    name = os.path.basename(case_dir.rstrip('/\\'))
    m = meta(os.path.join(case_dir, 'meta.txt'))
    nodes = read(os.path.join(case_dir, 'nodes.csv'))
    quads = read(os.path.join(case_dir, 'quads.csv'))
    fixed = read(os.path.join(case_dir, 'fixed.csv'))
    t = float(m['thickness'])
    rho = float(m['gravity']) / (t * G)
    per_axis = {0: [], 1: [], 2: []}
    for row in fixed:
        f = [int(v) for v in row[1:7]]
        if any(f[3:]):
            raise SystemExit('%s: restricao de rotacao nao suportada' % name)
        for ax in range(3):
            if f[ax]:
                per_axis[ax].append(int(row[0]) + 1)
    x = ['<liml version="27">', '  <analysis type="S30" />', '  <incompatiblehex8>true</incompatiblehex8>']
    for row in nodes:
        x.append('  <node nid="%d" x="%s" y="%s" z="%s" />' % (int(row[0]) + 1, row[1], row[2], row[3]))
    x.append('  <elset name="Default" colorrgb="DADADA" expanded="true" material="Material">')
    for row in quads:
        x.append('    <elem eid="%d" shape="quad4" nodes="%s" />' % (int(row[0]) + 1, ' '.join(str(int(v) + 1) for v in row[1:5])))
    x.append('  </elset>')
    for ax, label in enumerate('xyz'):
        if per_axis[ax]:
            comp = ['0', '0', '0']
            comp[ax] = '1'
            x.append('  <displacement selection="nodes_u%s"><value>0 m</value><x>%s</x><y>%s</y><z>%s</z></displacement>' % (label, comp[0], comp[1], comp[2]))
    x.append('  <gravity><x>0 m/s&#178;</x><y>0 m/s&#178;</y><z>-%r m/s&#178;</z></gravity>' % G)
    x.append('  <mat name="Material"><geometric type="Plate" thickness="%r m" /><mechanical type="Isotropic" youngsmodulus="%r Pa" poissonratio="%r" /><density type="Simple" density="%r kg/m&#179;" /></mat>'
             % (t, float(m['E']), float(m['nu']), rho))
    for ax, label in enumerate('xyz'):
        if per_axis[ax]:
            x.append('  <nodeselection name="nodes_u%s" direct="true">%s</nodeselection>' % (label, ''.join('<node nid="%d" />' % n for n in per_axis[ax])))
    for v in ['displacement', 'rotation']:
        x.append('  <fieldvariable name="%s" />' % v)
    x.append('</liml>')
    open(os.path.join(out_dir, name + '.liml'), 'w', encoding='utf-8').write('\n'.join(x))
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
    cases, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for d in sorted(os.listdir(cases)):
        if os.path.isdir(os.path.join(cases, d)):
            print('gerado', generate(os.path.join(cases, d), out))

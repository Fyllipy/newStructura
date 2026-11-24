# Modelos EQUIVALENTES do Mecway 28 (.liml) do cilindro de cb_dump: mesma malha (casca quad4), mesmas restricoes de translacao e as mesmas forcas axiais no anel do topo,
# em flambagem linear (B30) e vibracao (V30), e os scripts de extracao (fatores/frequencias, forma do modo 1, e salvar a solucao para os prints).
# uso: python gen_cb.py <pasta_case> <pasta_saida>
import csv
import ctypes
import os
import sys


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


def liml(case, kind, maxmode, shift):
    m = meta(os.path.join(case, 'meta.txt'))
    nodes = read(os.path.join(case, 'nodes.csv'))
    quads = read(os.path.join(case, 'quads.csv'))
    fixed = read(os.path.join(case, 'fixed.csv'))
    loads = read(os.path.join(case, 'loads.csv'))
    per_axis = {0: [], 1: [], 2: []}
    for row in fixed:
        for ax in range(3):
            if int(row[1 + ax]):
                per_axis[ax].append(int(row[0]) + 1)
    x = ['<liml version="27">', '  <analysis type="%s" maxmode="%d" shift="%s" />' % (kind, maxmode, shift), '  <incompatiblehex8>true</incompatiblehex8>']
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
    if kind == 'B30':
        fz = sum(float(r[1]) for r in loads)   # no Mecway a forca em uma selecao de nos e o TOTAL (dividido entre os nos)
        x.append('  <force selection="load_nodes"><x>0 N</x><y>0 N</y><z>%r N</z></force>' % fz)
        x.append('  <nodeselection name="load_nodes" direct="true">%s</nodeselection>' % ''.join('<node nid="%d" />' % (int(r[0]) + 1) for r in loads))
    x.append('  <mat name="Material"><geometric type="Plate" thickness="%r m" /><mechanical type="Isotropic" youngsmodulus="%r Pa" poissonratio="%r" /><density type="Simple" density="%r kg/m&#179;" /></mat>'
             % (float(m['thickness']), float(m['E']), float(m['nu']), float(m['rho'])))
    for ax, label in enumerate('xyz'):
        if per_axis[ax]:
            x.append('  <nodeselection name="nodes_u%s" direct="true">%s</nodeselection>' % (label, ''.join('<node nid="%d" />' % n for n in per_axis[ax])))
    for v in ['displacement']:
        x.append('  <fieldvariable name="%s" />' % v)
    x.append('</liml>')
    return '\n'.join(x)


def extract(kind, out_dir, name):
    d = short_path(out_dir).replace('\\', '/')
    py = '''import csv
mw.solve()
mw.set_solution_active()
res = open(r'%s/%s_mw_result.txt', 'w')
modes = solution.modes()
for k in modes:
    if %r == 'B30':
        res.write('fator%%d=%%r\\n' %% (k, solution.buckling_factor(k)))
    else:
        res.write('f%%d=%%r\\n' %% (k, solution.frequency(k)))
res.close()
f = open(r'%s/%s_mw_mode1.csv', 'wb')
w = csv.writer(f)
for nid in sorted(solution.all_nodes()):
    pos = solution.node(nid)
    row = [repr(pos.x), repr(pos.y), repr(pos.z)]
    for var in ('displx', 'disply', 'displz'):
        v = solution.node_value(var, nid, None, modes[0])
        row.append('' if v is None else repr(v))
    w.writerow(row)
f.close()
mw.save_as(r'%s/%s_solved.liml')
''' % (d, name, kind, d, name, d, name)
    assert all(ord(ch) < 128 for ch in py)
    return py


if __name__ == '__main__':
    case, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for kind, name, maxmode, shift in (('B30', 'cil_buckling', 6, '1E7'), ('V30', 'cil_modal', 8, '0')):
        open(os.path.join(out, name + '.liml'), 'w', encoding='utf-8').write(liml(case, kind, maxmode, shift))
        open(os.path.join(out, name + '_extract.py'), 'w').write(extract(kind, out, name))
        print('gerado', name)

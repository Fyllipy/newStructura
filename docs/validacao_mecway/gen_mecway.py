# Gera, a partir da malha/apoios/cargas exportados pelo Structura, o modelo EQUIVALENTE do Mecway 28 (.liml) e o script de extracao.
# uso: python gen_mecway.py <pasta_cases> <pasta_saida> [ccx]
import csv
import ctypes
import os
import sys


def short_path(path):
    # caminho 8.3 (so ASCII): o IronPython do Mecway le o script como ASCII e quebra com acentos (ex.: 'Joao' com til)
    path = os.path.abspath(path)
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, 1024)
    return buf.value if n else path

OUT_FIELDS = ['displx', 'disply', 'displz', 'rotx', 'roty', 'rotz', 'reactionforcex', 'reactionforcey', 'reactionforcez', 'reactionmomx', 'reactionmomy', 'reactionmomz']
ELEM_FIELDS = ['vonmisesu', 'vonmisesb', 'vonmisesm', 'momentu', 'momentv', 'momentuv', 'momprincmax', 'momprincmin', 'shearuw', 'shearvw']


def read_csv(path):
    with open(path, newline='') as f:
        return [row for row in csv.reader(f) if row]


def meta(path):
    d = {}
    for line in open(path, encoding='utf-8'):
        if '=' in line:
            k, v = line.rstrip('\n').split('=', 1)
            d[k] = v
    return d


def generate(case_dir, out_dir, ccx):
    name = os.path.basename(case_dir.rstrip('/\\'))
    m = meta(os.path.join(case_dir, 'meta.txt'))
    nodes = read_csv(os.path.join(case_dir, 'nodes.csv'))
    quads = read_csv(os.path.join(case_dir, 'quads.csv'))
    fixed = read_csv(os.path.join(case_dir, 'fixed.csv'))
    pressure = read_csv(os.path.join(case_dir, 'pressure.csv'))
    thickness = set(round(float(q[5]), 12) for q in quads)
    if len(thickness) != 1:
        raise SystemExit('%s: espessura nao uniforme (%s)' % (name, thickness))
    t = thickness.pop()
    pvals = set()
    for p in pressure:
        vals = [float(v) for v in p[1:5]]
        if max(vals) - min(vals) > 1e-9 * max(1.0, abs(vals[0])):
            raise SystemExit('%s: pressao nao uniforme no elemento' % name)
        pvals.add(round(vals[0], 9))
    if len(pvals) != 1 or len(pressure) != len(quads):
        raise SystemExit('%s: pressao nao uniforme no modelo' % name)
    p = pvals.pop()

    # restricoes: todos os 6 graus -> "fixed"; senao so translacoes (rotacoes isoladas nao sao usadas nestes casos)
    all6, per_axis = [], {0: [], 1: [], 2: []}
    for row in fixed:
        nid = int(row[0]) + 1
        f = [int(v) for v in row[1:7]]
        if all(f):
            all6.append(nid)
            continue
        if any(f[3:]):
            raise SystemExit('%s: restricao de rotacao isolada no no %d nao suportada pelo gerador' % (name, nid))
        for ax in range(3):
            if f[ax]:
                per_axis[ax].append(nid)

    x = ['<liml version="27">', '  <analysis type="S30"%s />' % (' solver="ccx"' if ccx else ''), '  <incompatiblehex8>true</incompatiblehex8>']
    for row in nodes:
        x.append('  <node nid="%d" x="%s" y="%s" z="%s" />' % (int(row[0]) + 1, row[1], row[2], row[3]))
    x.append('  <elset name="Default" colorrgb="DADADA" expanded="true" material="Material">')
    for row in quads:
        x.append('    <elem eid="%d" shape="quad4" nodes="%s" />' % (int(row[0]) + 1, ' '.join(str(int(v) + 1) for v in row[1:5])))
    x.append('  </elset>')
    # engaste pelas FACES DE ARESTA dos elementos (face i = aresta entre os nos i e i+1): "fixed" em faces restringe os 6 graus dos nos da aresta
    edge_faces = []
    if all6:
        s6 = set(all6)
        for q in quads:
            ns = [int(v) + 1 for v in q[1:5]]
            for i in range(4):
                if ns[i] in s6 and ns[(i + 1) % 4] in s6:
                    edge_faces.append((int(q[0]) + 1, i + 1))
        if not edge_faces:
            raise SystemExit('%s: nos engastados sem aresta de elemento' % name)
        x.append('  <fixed selection="faces_fixed" />')
    for ax, label in enumerate('xyz'):
        if per_axis[ax]:
            comp = ['0', '0', '0']
            comp[ax] = '1'
            x.append('  <displacement selection="nodes_u%s"><value>0 m</value><x>%s</x><y>%s</y><z>%s</z></displacement>' % (label, comp[0], comp[1], comp[2]))
    if abs(p) > 0:
        x.append('  <surfacepressure selection="faces_pressure"><normal>%r Pa</normal></surfacepressure>' % p)
    x.append('  <mat name="Material"><geometric type="Plate" thickness="%r m" /><mechanical type="Isotropic" youngsmodulus="%s Pa" poissonratio="%s" /></mat>'
             % (t, repr(float(m['E'])), repr(float(m['nu']))))
    if all6:
        x.append('  <faceselection name="faces_fixed" direct="true">%s</faceselection>' % ''.join('<face eid="%d" faceid="%d" />' % ef for ef in edge_faces))
    for ax, label in enumerate('xyz'):
        if per_axis[ax]:
            x.append('  <nodeselection name="nodes_u%s" direct="true">%s</nodeselection>' % (label, ''.join('<node nid="%d" />' % n for n in per_axis[ax])))
    # face 6 = superficie no sentido +W (normal do elemento): pressao positiva "em direcao ao elemento" empurra no sentido +normal, como no Structura
    x.append('  <faceselection name="faces_pressure" direct="true">%s</faceselection>' % ''.join('<face eid="%d" faceid="6" />' % (int(q[0]) + 1) for q in quads))
    for v in ['displacement', 'rotation', 'reaction force', 'reaction moment', 'stress', 'von Mises stress', 'moment per length', 'force per length']:
        x.append('  <fieldvariable name="%s" />' % v)
    x.append('</liml>')
    tag = name + ('_ccx' if ccx else '')
    liml_path = os.path.join(out_dir, tag + '.liml')
    open(liml_path, 'w', encoding='utf-8').write('\n'.join(x))

    # script de extracao (roda dentro do Mecway: mecway <liml> script <py>)
    nodes_csv = os.path.join(short_path(out_dir), tag + '_mw_nodes.csv').replace('\\', '/')
    elem_csv = os.path.join(short_path(out_dir), tag + '_mw_elem.csv').replace('\\', '/')
    py = '''import csv
mw.solve()
mw.set_solution_active()
def val(var, nid):
    try:
        v = solution.node_value(var, nid, None, None)
    except Exception as e:
        return ''
    return '' if v is None else repr(v)
def eval_(var, eid, k):
    try:
        v = solution.element_node_value(var, eid, k, None, None)
    except Exception as e:
        return ''
    return '' if v is None else repr(v)
OUT_FIELDS = %r
ELEM_FIELDS = %r
f = open(r'%s', 'wb')
w = csv.writer(f)
w.writerow(['nid', 'px', 'py', 'pz'] + OUT_FIELDS)
for nid in sorted(solution.all_nodes()):
    pos = solution.node(nid)
    w.writerow([nid, repr(pos.x), repr(pos.y), repr(pos.z)] + [val(v, nid) for v in OUT_FIELDS])
f.close()
f = open(r'%s', 'wb')
w = csv.writer(f)
w.writerow(['eid', 'k'] + ELEM_FIELDS)
for eid in sorted(solution.all_elements()):
    for k in (1, 2, 3, 4):
        w.writerow([eid, k] + [eval_(v, eid, k) for v in ELEM_FIELDS])
f.close()
''' % (OUT_FIELDS, ELEM_FIELDS, nodes_csv, elem_csv)
    assert all(ord(ch) < 128 for ch in py), 'script de extracao com caractere nao ASCII'
    open(os.path.join(out_dir, tag + '_extract.py'), 'w').write(py)
    return tag


if __name__ == '__main__':
    cases_dir, out_dir = sys.argv[1], sys.argv[2]
    ccx = len(sys.argv) > 3 and sys.argv[3] == 'ccx'
    os.makedirs(out_dir, exist_ok=True)
    for d in sorted(os.listdir(cases_dir)):
        p = os.path.join(cases_dir, d)
        if os.path.isdir(p):
            print('gerado', generate(p, out_dir, ccx))

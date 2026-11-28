"""Utilitarios para gerar modelos .liml (Mecway 28, solver CalculiX) com hexaedros e rodar/extrair resultados em modo headless.
Formato (.liml) baseado nos exemplos instalados com o Mecway (Samples/BoltedJoint.liml) e em docs/validacao_mecway/gen_mecway.py.
Unidades SI (m, N, Pa)."""
import ctypes
import os
import subprocess

MECWAY = r"C:\Program Files\Mecway\mecway.exe"


def short_path(path):
    path = os.path.abspath(path)
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, 1024)
    return buf.value if n else path


class Model:
    def __init__(self):
        self.nodes = {}          # chave -> (id, x, y, z)
        self.elems = []          # (eid, elset, [n1..n8])
        self.elsets = {}         # nome -> [eids]
        self.elsets_mat = {}     # nome -> material
        self.mats = {}           # nome -> (E, nu)
        self.fixed_nodes = {0: [], 1: [], 2: []}   # eixo -> lista de nids com u_eixo = valor
        self.prescribed = []     # (eixo, valor, [nids])
        self.face_sets = {}      # nome -> [(eid, faceid)]
        self.contacts = []       # (master, slave, tipo)
        self.forces = []         # ((fx, fy, fz) em N, [nids])
        self.next_nid = 1
        self.next_eid = 1

    def node(self, key, x, y, z):
        if key not in self.nodes:
            self.nodes[key] = (self.next_nid, x, y, z)
            self.next_nid += 1
        return self.nodes[key][0]

    def hex(self, elset, nids):
        eid = self.next_eid
        self.next_eid += 1
        self.elems.append((eid, elset, nids))
        self.elsets.setdefault(elset, [])
        self.elsets[elset].append(eid)
        return eid

    def fix(self, axis, nids):
        self.fixed_nodes[axis].extend(nids)

    def force(self, vec, nids):
        self.forces.append((vec, list(nids)))

    def prescribe(self, axis, value, nids):
        self.prescribed.append((axis, value, list(nids)))

    def to_liml(self, path):
        x = ['<liml version="27">', '  <analysis type="S30" solver="ccx" />', '  <incompatiblehex8>true</incompatiblehex8>']
        for key, (nid, px, py, pz) in sorted(self.nodes.items(), key=lambda kv: kv[1][0]):
            x.append('  <node nid="%d" x="%r" y="%r" z="%r" />' % (nid, px, py, pz))
        by_set = {}
        for eid, es, nids in self.elems:
            by_set.setdefault(es, []).append((eid, nids))
        for es, items in by_set.items():
            x.append('  <elset name="%s" colorrgb="DADADA" expanded="true" material="%s">' % (es, self.elsets_mat[es]))
            for eid, nids in items:
                x.append('    <elem eid="%d" shape="hex8" nodes="%s" />' % (eid, ' '.join(str(n) for n in nids)))
            x.append('  </elset>')
        # restricoes
        sel_i = 0
        for axis, nids in self.fixed_nodes.items():
            if nids:
                comp = ['0', '0', '0']
                comp[axis] = '1'
                x.append('  <displacement selection="fix_%s"><value>0 m</value><x>%s</x><y>%s</y><z>%s</z></displacement>' % ('xyz'[axis], comp[0], comp[1], comp[2]))
        for i, (axis, value, nids) in enumerate(self.prescribed):
            comp = ['0', '0', '0']
            comp[axis] = '1'
            x.append('  <displacement selection="pres_%d"><value>%r m</value><x>%s</x><y>%s</y><z>%s</z></displacement>' % (i, value, comp[0], comp[1], comp[2]))
        for i, (vec, nids) in enumerate(self.forces):
            x.append('  <force selection="load_%d"><x>%r N</x><y>%r N</y><z>%r N</z></force>' % (i, vec[0], vec[1], vec[2]))
        for master, slave, kind, kn in self.contacts:
            x.append('  <contact2 selectionmaster="%s" selectionslave="%s" type="%s">' % (master, slave, kind))
            x.append('    <tolerance>0 m</tolerance>')
            x.append('    <normalstiffness>%r GPa/m</normalstiffness>' % kn)
            x.append('    <tangentialstiffness>0 Pa/m</tangentialstiffness>')
            x.append('    <frictioncoefficient>0</frictioncoefficient>')
            x.append('    <conductanceperarea>0 W/m²/K</conductanceperarea>')
            x.append('  </contact2>')
        x.append('  <ccxnoextracontactstep />')
        for name, (E, nu) in self.mats.items():
            x.append('  <mat name="%s"><mechanical type="Isotropic" youngsmodulus="%r Pa" poissonratio="%r" /></mat>' % (name, E, nu))
        for axis, nids in self.fixed_nodes.items():
            if nids:
                x.append('  <nodeselection name="fix_%s" direct="true">%s</nodeselection>' % ('xyz'[axis], ''.join('<node nid="%d" />' % n for n in nids)))
        for i, (axis, value, nids) in enumerate(self.prescribed):
            x.append('  <nodeselection name="pres_%d" direct="true">%s</nodeselection>' % (i, ''.join('<node nid="%d" />' % n for n in nids)))
        for i, (vec, nids) in enumerate(self.forces):
            x.append('  <nodeselection name="load_%d" direct="true">%s</nodeselection>' % (i, ''.join('<node nid="%d" />' % n for n in nids)))
        for name, faces in self.face_sets.items():
            x.append('  <faceselection name="%s" direct="true">%s</faceselection>' % (name, ''.join('<face eid="%d" faceid="%d" />' % f for f in faces)))
        for v in ('displacement', 'reaction force'):
            x.append('  <fieldvariable name="%s" />' % v)
        x.append('</liml>')
        with open(path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(x))


EXTRACT = '''mw.solve()
mw.set_solution_active()
f = open(r'%(out)s', 'w')
f.write('nid,ux,uy,uz,rx,ry,rz\\n')
for nid in sorted(solution.all_nodes()):
    vals = []
    for var in ('displx', 'disply', 'displz', 'reactionforcex', 'reactionforcey', 'reactionforcez'):
        try:
            v = solution.node_value(var, nid, None, None)
        except Exception:
            v = None
        vals.append('' if v is None else repr(v))
    f.write('%%d,%%s\\n' %% (nid, ','.join(vals)))
f.close()
'''


def run(model_path, out_csv, timeout=600):
    """Roda o Mecway em modo headless e devolve o CSV de deslocamentos/reacoes por no."""
    mp, op = short_path(model_path), os.path.join(short_path(os.path.dirname(os.path.abspath(out_csv))), os.path.basename(out_csv))
    script = os.path.splitext(model_path)[0] + '_extract.py'
    with open(script, 'w') as f:
        f.write(EXTRACT % {'out': op})
    if os.path.exists(out_csv):
        os.remove(out_csv)
    try:
        r = subprocess.run([MECWAY, mp, 'script', short_path(script)], capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None
    return out_csv if os.path.exists(out_csv) else None


def read_results(csv_path):
    res = {}
    with open(csv_path) as f:
        next(f)
        for line in f:
            p = line.rstrip('\n').split(',')
            res[int(p[0])] = [float(v) if v else 0.0 for v in p[1:]]
    return res

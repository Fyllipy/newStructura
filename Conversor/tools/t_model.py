import sys
sys.path.insert(0, '..')
from cype2structura.model import build_model

for f in sys.argv[1:]:
    d = open("D:/CYPE Ingenieros/Projetos/CYPE 3D/%s.ed3" % f, 'rb').read()
    m = build_model(d)
    print("==", f, m.program, "nos", len(m.nodes), "barras", len(m.bars), "pecas", len(m.pieces), "secoes", len(m.sections),
          "paineis", len(m.panels), "hip", m.hypothesis_names)
    for w in m.warnings[:8]:
        print("   aviso:", w)
    print("   total avisos", len(m.warnings))
    for pn in m.panels[:3]:
        print("   painel", pn.id, pn.loads, pn.span_dir)

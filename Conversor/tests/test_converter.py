"""Testes de regressao do conversor (usam os projetos do usuario em D:\CYPE Ingenieros; ignorados se ausentes)."""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from cype2structura.model import build_model  # noqa: E402
from cype2structura.writer import write_dat  # noqa: E402

BASE = r"D:\CYPE Ingenieros\Projetos\CYPE 3D"

# (arquivo, versao, nos, barras)
CASES = [
    ("Tesouras Recepção Cambuci.ed3", "CYPE-2026-a", 75, 150),
    ("Restaurante.ed3", "CYPE-2026-a", 140, 304),
    ("Anexo Veloo.ed3", "CYPE-2022-h", 4, 3),
    ("Metalicas Restaurante.ed3", "CYPE-2022-h", 108, 161),
    ("TEste.ed3", "CYPE-2021-g", 4, 3),
    ("Anexo Veloo 2 teste.ed3", "CYPE-2020-f", 46, 70),
]


class ConverterTest(unittest.TestCase):
    def test_projects(self):
        for name, version, nodes, bars in CASES:
            path = os.path.join(BASE, name)
            if not os.path.exists(path):
                continue
            with self.subTest(name):
                model = build_model(open(path, "rb").read())
                self.assertEqual(model.program, version)
                self.assertEqual(len(model.nodes), nodes)
                self.assertEqual(len(model.bars), bars)
                with tempfile.TemporaryDirectory() as tmp:
                    out = os.path.join(tmp, "m.dat")
                    report = write_dat(model, out, name)
                    text = open(out, encoding="cp1252").read()
                    for section in ("[MATERIALS]", "[SECTIONS]", "[NODES]", "[MEMBERS]"):
                        self.assertIn(section, text)
                    self.assertGreater(report["members"], 0)


if __name__ == "__main__":
    unittest.main()

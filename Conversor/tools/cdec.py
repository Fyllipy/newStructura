"""Parser de um subconjunto do C decompilado pelo Ghidra e interpretador das funcoes de leitura (*_lee) do CYPE 3D."""
from __future__ import annotations

import json
import re
import struct

TYPE_WORDS = {"undefined", "undefined1", "undefined2", "undefined3", "undefined4", "undefined5", "undefined6", "undefined7",
              "undefined8", "byte", "short", "ushort", "uint", "int", "long", "ulong", "longlong", "ulonglong", "char", "uchar",
              "bool", "float", "double", "code", "void", "float10", "sbyte", "dword", "word", "qword", "size_t", "LPCSTR",
              "HWND", "FARPROC", "POINT", "uint3", "undefined16", "unkbyte3", "unkint3", "unkuint3", "int3"}

TOKEN = re.compile(r"""
    (?P<ws>\s+)|(?P<cmt>/\*.*?\*/)|(?P<str>"(?:\\.|[^"\\])*")|(?P<chr>'(?:\\.|[^'\\])*')
   |(?P<num>0[xX][0-9a-fA-F]+(?:[uUlL]*)|\d+\.\d*(?:[eE][-+]?\d+)?|\d+(?:[uUlL]*))
   |(?P<id>[A-Za-z_][A-Za-z_0-9]*)
   |(?P<op>\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|->|[-+*/%&|^~!<>=?:.,;(){}\[\]])
""", re.X | re.S)


def tokenize(src: str):
    pos = 0
    out = []
    while pos < len(src):
        m = TOKEN.match(src, pos)
        if not m:
            raise SyntaxError("token invalido: %r" % src[pos:pos + 30])
        pos = m.end()
        kind = m.lastgroup
        if kind in ("ws", "cmt"):
            continue
        out.append((kind, m.group()))
    return out


class Parser:
    def __init__(self, toks):
        self.t = toks
        self.i = 0

    def peek(self, k=0):
        return self.t[self.i + k] if self.i + k < len(self.t) else ("eof", "")

    def next(self):
        tok = self.t[self.i]
        self.i += 1
        return tok

    def accept(self, v):
        if self.peek()[1] == v and self.peek()[0] in ("op", "id"):
            self.i += 1
            return True
        return False

    def expect(self, v):
        if not self.accept(v):
            raise SyntaxError("esperado %r, obtido %r (pos %d)" % (v, self.peek(), self.i))

    # ---- statements
    def block(self):
        self.expect("{")
        out = []
        while not self.accept("}"):
            s = self.stmt()
            if s is not None:
                out.append(s)
        return ["block", out]

    def is_decl(self):
        k, v = self.peek()
        if k != "id" or v not in TYPE_WORDS:
            return False
        # tipo seguido de [*]* identificador
        j = 1
        while self.peek(j)[1] == "*" or self.peek(j)[1] in TYPE_WORDS:
            j += 1
        return self.peek(j)[0] == "id" and self.peek(j + 1)[1] in (";", "[", ",", "=")

    def stmt(self):
        k, v = self.peek()
        if v == "{" and k == "op":
            return self.block()
        if v == ";" and k == "op":
            self.next()
            return None
        if k == "id":
            if v == "if":
                self.next(); self.expect("(")
                c = self.expr(); self.expect(")")
                th = self.stmt()
                el = None
                if self.accept("else"):
                    el = self.stmt()
                return ["if", c, th, el]
            if v == "return":
                self.next()
                e = None if self.peek()[1] == ";" else self.expr()
                self.expect(";")
                return ["ret", e]
            if v == "for":
                self.next(); self.expect("(")
                init = None if self.peek()[1] == ";" else self.expr()
                self.expect(";")
                cond = None if self.peek()[1] == ";" else self.expr()
                self.expect(";")
                step = None if self.peek()[1] == ")" else self.expr()
                self.expect(")")
                return ["for", init, cond, step, self.stmt()]
            if v == "while":
                self.next(); self.expect("(")
                c = self.expr(); self.expect(")")
                return ["while", c, self.stmt()]
            if v == "do":
                self.next()
                body = self.stmt()
                self.expect("while"); self.expect("(")
                c = self.expr(); self.expect(")"); self.expect(";")
                return ["do", body, c]
            if v == "switch":
                self.next(); self.expect("(")
                e = self.expr(); self.expect(")")
                body = self.block()
                return ["switch", e, body]
            if v in ("case", "default"):
                self.next()
                if v == "case":
                    self.expr()
                self.expect(":")
                return None
            if v in ("break", "continue", "goto"):
                self.next()
                while not self.accept(";"):
                    self.next()
                return ["break"]
            if v in ("do",):
                pass
            if self.is_decl():
                # declaracao: ignora ate ';' (pode ter inicializador)
                depth = 0
                init = None
                while True:
                    tk = self.next()
                    if tk[1] in ("(", "["):
                        depth += 1
                    elif tk[1] in (")", "]"):
                        depth -= 1
                    elif tk[1] == ";" and depth == 0:
                        break
                return None
        e = self.expr()
        self.expect(";")
        return ["expr", e]

    # ---- expressions
    PREC = [["||"], ["&&"], ["|"], ["^"], ["&"], ["==", "!="], ["<", ">", "<=", ">="], ["<<", ">>"], ["+", "-"], ["*", "/", "%"]]

    def expr(self):
        return self.comma()

    def comma(self):
        e = self.assign()
        while self.peek()[1] == "," and self.peek()[0] == "op" and self.comma_ok:
            self.next()
            e = ["comma", e, self.assign()]
        return e

    comma_ok = True

    def assign(self):
        lhs = self.ternary()
        k, v = self.peek()
        if k == "op" and v in ("=", "+=", "-=", "*=", "/=", "|=", "&=", "^=", "<<=", ">>=", "%="):
            self.next()
            rhs = self.assign()
            return ["assign", v, lhs, rhs]
        return lhs

    def ternary(self):
        c = self.binary(0)
        if self.peek()[1] == "?" and self.peek()[0] == "op":
            self.next()
            a = self.assign()
            self.expect(":")
            b = self.assign()
            return ["tern", c, a, b]
        return c

    def binary(self, lvl):
        if lvl == len(self.PREC):
            return self.unary()
        e = self.binary(lvl + 1)
        while self.peek()[0] == "op" and self.peek()[1] in self.PREC[lvl]:
            op = self.next()[1]
            e = ["bin", op, e, self.binary(lvl + 1)]
        return e

    def is_cast(self):
        if self.peek()[1] != "(" or self.peek()[0] != "op":
            return False
        k, v = self.peek(1)
        if k == "id" and v in TYPE_WORDS:
            j = 2
            while self.peek(j)[1] == "*" or self.peek(j)[1] in TYPE_WORDS:
                j += 1
            return self.peek(j)[1] == ")"
        return False

    def unary(self):
        k, v = self.peek()
        if k == "op" and v in ("!", "-", "~", "+"):
            self.next()
            return ["un", v, self.unary()]
        if k == "op" and v == "*":
            self.next()
            return ["deref", self.unary()]
        if k == "op" and v == "&":
            self.next()
            return ["addr", self.unary()]
        if k == "op" and v in ("++", "--"):
            self.next()
            return ["un", v, self.unary()]
        if self.is_cast():
            self.next()
            while self.peek()[1] != ")":
                self.next()
            self.next()
            return self.unary()
        return self.postfix()

    def args(self):
        out = []
        self.comma_ok = False
        if self.peek()[1] != ")":
            out.append(self.assign())
            while self.accept(","):
                out.append(self.assign())
        self.comma_ok = True
        self.expect(")")
        return out

    def postfix(self):
        e = self.primary()
        while True:
            k, v = self.peek()
            if k == "op" and v == "(":
                self.next()
                saved = self.comma_ok
                e = ["call", e, self.args()]
                self.comma_ok = saved
            elif k == "op" and v == "[":
                self.next()
                saved = self.comma_ok
                self.comma_ok = True
                idx = self.expr()
                self.comma_ok = saved
                self.expect("]")
                e = ["idx", e, idx]
            elif k == "op" and v in (".", "->"):
                self.next()
                self.next()
                e = ["field", e]
            elif k == "op" and v in ("++", "--"):
                self.next()
                e = ["post", v, e]
            else:
                return e

    def primary(self):
        k, v = self.next()
        if k == "num":
            txt = v.rstrip("uUlL")
            if txt.lower().startswith("0x"):
                return ["num", int(txt, 16)]
            if "." in txt or "e" in txt.lower():
                return ["num", float(txt)]
            return ["num", int(txt)]
        if k == "id":
            return ["var", v]
        if k == "str":
            return ["str", v]
        if k == "chr":
            return ["num", 0]
        if k == "op" and v == "(":
            saved = self.comma_ok
            self.comma_ok = True
            e = self.expr()
            self.comma_ok = saved
            self.expect(")")
            return e
        raise SyntaxError("expressao inesperada: %r" % (v,))


def split_functions(text: str):
    """texto do dump -> {nome: (params, ast)}"""
    funcs = {}
    for blk in text.split("\n===== ")[1:]:
        name = blk.split(" ", 1)[0]
        body = blk.split("-- callees")[0].split("\n", 1)[1]
        m = re.search(r"\)\s*\n\s*\{", body)
        if not m:
            continue
        header = body[:m.start() + 1]
        params = re.findall(r"\b(param_\d+)\b", header)
        src = body[m.end() - 1:]
        # remove comentarios de bloco e declaracoes de variaveis locais
        try:
            toks = tokenize(src)
            ast = Parser(toks).block()
        except SyntaxError as exc:
            funcs[name] = (params, ["error", str(exc)])
            continue
        funcs[name] = (params, ast)
    return funcs


if __name__ == "__main__":
    import sys
    SP = "C:/Users/JOO~1/AppData/Local/Temp/claude/C--Users-Jo-o-Documents-Repos-Conversor/62e829b0-04f7-4a4c-ac8d-ffd81bb16d8a/scratchpad"
    text = open(SP + "/" + sys.argv[1], encoding="utf-8", errors="replace").read()
    fs = split_functions(text)
    bad = [n for n, (p, a) in fs.items() if a[0] == "error"]
    print(len(fs), "funcoes;", len(bad), "com erro de parse:", [(n, fs[n][1][1][:60]) for n in bad[:8]])

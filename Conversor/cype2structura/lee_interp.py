"""Interpretador das funcoes de leitura (*_lee) do CYPE 3D, com ramificacao por versao de registro.

As funcoes foram extraidas do proprio CYPE 3D (ver tools/) e empacotadas em lee_funcs.json.gz como arvores sintaticas."""
from __future__ import annotations

import re
import struct
import gzip
import json
import os


class Unsupported(Exception):
    pass


class Stream:
    def __init__(self, buf, pos):
        self.buf = buf
        self.pos = pos

    def take(self, fmt):
        size = struct.calcsize(fmt)
        if self.pos + size > len(self.buf):
            raise Unsupported("fim de arquivo")
        v = struct.unpack_from(fmt, self.buf, self.pos)[0]
        self.pos += size
        return v

    def skip(self, n):
        if self.pos + n > len(self.buf):
            raise Unsupported("fim de arquivo")
        self.pos += n


class Obj:
    """Resultado de uma chamada que nao le o fluxo (construtor etc.): guarda nome e argumentos avaliados."""
    __slots__ = ("name", "args")

    def __init__(self, name, args):
        self.name = name
        self.args = args

    def __repr__(self):
        return "Obj(%s,%s)" % (self.name, self.args)


class FuncRef:
    __slots__ = ("name", "lib")

    def __init__(self, name, lib):
        self.name = name
        self.lib = lib

    def __repr__(self):
        return "FuncRef(%s)" % self.name


class Ptr:
    """Ponteiro para variavel local (parametro de saida)."""
    __slots__ = ("env", "name", "const")

    def __init__(self, env=None, name=None, const=None):
        self.env = env
        self.name = name
        self.const = const

    @property
    def value(self):
        return self.env.get(self.name) if self.env is not None else self.const

    @value.setter
    def value(self, v):
        if self.env is not None:
            self.env[self.name] = v
        else:
            self.const = v

    def __repr__(self):
        return "Ptr(%r)" % (self.value,)


class Cells(list):
    """Variavel local usada como array/ponteiro (ex.: local_18[0] = ...)."""


class Return(Exception):
    def __init__(self, v):
        self.v = v


class Break(Exception):
    pass


PRIM = {"uchar": "<B", "char": "<b", "bool": "<H", "ushort": "<H", "short": "<h", "ulong": "<I", "long": "<i",
        "ulong64": "<Q", "long64": "<q", "double": "<d", "float": "<f", "enum": "<I", "int": "<i", "uint": "<I"}
# cypesave_lee_bool le 1 byte (ver bin_lee_bool); ushort/short 2; ulong/long 4
PRIM["bool"] = "<B"

MAX_ITER = 200000


class Interp:
    def __init__(self, libs: dict[str, dict]):
        """libs: dll -> {nome: (params, ast)}"""
        self.libs = libs
        self.ext = {}
        for dll, fs in libs.items():
            for n in fs:
                if not n.startswith("FUN_") and not n.startswith("DelayLoad_"):
                    self.ext.setdefault(n, dll)
        self.depth = 0
        self.trace = []

    # ---- chamada de funcao do usuario
    def call_user(self, lib, name, args, want_env=False):
        params, ast = self.libs[lib][name]
        if ast[0] == "error":
            raise Unsupported("parse: %s" % name)
        env = {}
        for p, a in zip(params, args):
            env[p] = a
        frame = {"env": env, "lib": lib, "last": None, "name": name}
        self.stack = getattr(self, "stack", []) + [name]
        self.depth += 1
        if self.depth > 60:
            raise Unsupported("recursao")
        try:
            self.exec(ast, frame)
        except Return as r:
            self.depth -= 1
            self.stack.pop()
            return (r.v, env) if want_env else r.v
        except Unsupported as exc:
            exc.stack = getattr(exc, "stack", []) + [name]
            exc.env = env                     # o quadro mais externo prevalece (colecoes ja lidas)
            raise
        self.depth -= 1
        self.stack.pop()
        return (frame["last"], env) if want_env else frame["last"]

    # ---- execucao
    def exec(self, s, fr):
        k = s[0]
        if k == "block":
            for x in s[1]:
                self.exec(x, fr)
        elif k == "expr":
            self.eval(s[1], fr)
        elif k == "if":
            if self.truthy(self.eval(s[1], fr)):
                if s[2] is not None:
                    self.exec(s[2], fr)
            elif s[3] is not None:
                self.exec(s[3], fr)
        elif k == "ret":
            raise Return(fr["last"] if s[1] is None else self.eval(s[1], fr))
        elif k == "for":
            if s[1] is not None:
                self.eval(s[1], fr)
            n = 0
            while s[2] is None or self.truthy(self.eval(s[2], fr)):
                try:
                    self.exec(s[4], fr)
                except Break:
                    break
                if s[3] is not None:
                    self.eval(s[3], fr)
                n += 1
                if n > MAX_ITER:
                    raise Unsupported("laco longo demais")
        elif k == "while":
            n = 0
            while self.truthy(self.eval(s[1], fr)):
                try:
                    self.exec(s[2], fr)
                except Break:
                    break
                n += 1
                if n > MAX_ITER:
                    raise Unsupported("laco longo demais")
        elif k == "do":
            n = 0
            while True:
                try:
                    self.exec(s[1], fr)
                except Break:
                    break
                n += 1
                if n > MAX_ITER or not self.truthy(self.eval(s[2], fr)):
                    break
        elif k == "switch":
            raise Unsupported("switch")
        elif k == "break":
            raise Break()
        else:
            raise Unsupported("stmt " + k)

    @staticmethod
    def truthy(v):
        if isinstance(v, (Ptr, Obj, FuncRef)):
            return True
        if v is None:
            raise Unsupported("condicao indeterminada")
        return bool(v)

    def num(self, v):
        if isinstance(v, bool):
            return int(v)
        if isinstance(v, (int, float)):
            return v
        raise Unsupported("operando indeterminado: %r" % (v,))

    def eval(self, e, fr):
        k = e[0]
        if k == "num":
            return e[1]
        if k == "var":
            name = e[1]
            env = fr["env"]
            if name in env:
                return env[name]
            if name.startswith("DAT_"):
                return 1 << 30
            if name in self.libs[fr["lib"]] or name.startswith("FUN_"):
                return FuncRef(name, fr["lib"])
            if name in self.ext:
                return FuncRef(name, self.ext[name])
            if name.endswith("_exref") and name[:-6] in self.ext:
                return FuncRef(name[:-6], self.ext[name[:-6]])
            return None
        if k == "str":
            return e[1]
        if k == "assign":
            op, lhs, rhs = e[1], e[2], e[3]
            val = self.eval(rhs, fr)
            if op != "=":
                cur = self.eval(lhs, fr)
                val = self.binop(op[:-1], cur, val)
            if lhs[0] == "var":
                fr["env"][lhs[1]] = val
            elif lhs[0] == "idx" and lhs[1][0] == "var":
                try:
                    i = int(self.eval(lhs[2], fr))
                except Exception:
                    i = 0
                cur = fr["env"].get(lhs[1][1])
                if not isinstance(cur, Cells):
                    cur = Cells()
                    fr["env"][lhs[1][1]] = cur
                while len(cur) <= i:
                    cur.append(None)
                cur[i] = val
            elif lhs[0] == "deref" and lhs[1][0] == "var":
                p = fr["env"].get(lhs[1][1])
                if isinstance(p, Ptr):
                    p.value = val
            return val
        if k == "comma":
            self.eval(e[1], fr)
            return self.eval(e[2], fr)
        if k == "bin":
            op = e[1]
            if op == "&&":
                a = self.eval(e[2], fr)
                return self.truthy(a) and self.truthy(self.eval(e[3], fr))
            if op == "||":
                a = self.eval(e[2], fr)
                return self.truthy(a) or self.truthy(self.eval(e[3], fr))
            return self.binop(op, self.eval(e[2], fr), self.eval(e[3], fr))
        if k == "un":
            op = e[1]
            v = self.eval(e[2], fr)
            if op == "!":
                return not self.truthy(v)
            if op == "-":
                return -self.num(v)
            if op == "~":
                return ~int(self.num(v))
            return v
        if k == "tern":
            return self.eval(e[2] if self.truthy(self.eval(e[1], fr)) else e[3], fr)
        if k == "addr":
            if e[1][0] == "var":
                fr["env"].setdefault(e[1][1], None)
                return Ptr(env=fr["env"], name=e[1][1])
            return Ptr(const=self.eval(e[1], fr))
        if k == "deref":
            try:
                p = self.eval(e[1], fr)
            except Unsupported:
                return None
            return p.value if isinstance(p, Ptr) else None
        if k in ("idx", "field"):
            try:
                self.eval(e[1], fr)
            except Unsupported:
                pass
            return None
        if k == "post":
            if e[2][0] == "var":
                cur = self.eval(e[2], fr)
                fr["env"][e[2][1]] = cur + (1 if e[1] == "++" else -1)
                return cur
            return None
        if k == "call":
            return self.call(e, fr)
        raise Unsupported("expr " + k)

    def binop(self, op, a, b):
        if isinstance(a, (Ptr, FuncRef, Obj)) or isinstance(b, (Ptr, FuncRef, Obj)):
            if op in ("==", "!="):
                other = b if isinstance(a, (Ptr, FuncRef, Obj)) else a
                same = (other is a or other is b) and isinstance(other, (Ptr, FuncRef, Obj))
                return (op == "!=") if not same else (op == "==")
            return None
        if a is None or b is None or isinstance(a, (Obj, FuncRef)) or isinstance(b, (Obj, FuncRef)):
            if op in ("==", "!="):
                if a is None and b is None:
                    return op == "=="
                return op == "!="        # desconhecido (ponteiro nao inicializado) tratado como diferente de zero
            return None
        a, b = self.num(a), self.num(b)
        if op == "+": return a + b
        if op == "-": return a - b
        if op == "*": return a * b
        if op == "/": return a / b if isinstance(a, float) or isinstance(b, float) else int(a / b) if b else 0
        if op == "%": return a % b if b else 0
        if op == "<": return a < b
        if op == ">": return a > b
        if op == "<=": return a <= b
        if op == ">=": return a >= b
        if op == "==": return a == b
        if op == "!=": return a != b
        if op == "&": return int(a) & int(b)
        if op == "|": return int(a) | int(b)
        if op == "^": return int(a) ^ int(b)
        if op == "<<": return int(a) << int(b)
        if op == ">>": return int(a) >> int(b)
        raise Unsupported("op " + op)

    # ---- chamadas
    def call(self, e, fr):
        fn = e[1]
        if fn[0] != "var":
            self.eval(fn, fr)
            for a in e[2]:
                self.eval(a, fr)
            return None
        name = fn[1]
        args = [self.eval(a, fr) for a in e[2]]
        has_stream = any(isinstance(a, Stream) for a in args)
        res = self.read_call(name, args, fr)
        if res is not NotImplemented:
            return res
        lib = fr["lib"]
        if name in self.libs[lib]:
            if has_stream:
                res = self.call_user(lib, name, args)
                if isinstance(res, Obj):
                    fr["last"] = res
                return res
        elif name in self.ext and has_stream:
            res = self.call_user(self.ext[name], name, args)
            if isinstance(res, Obj):
                fr["last"] = res
            return res
        if has_stream:
            raise Unsupported("leitor desconhecido: %s" % name)
        b = self.builtin(name, args)
        if b is not NotImplemented:
            return b
        obj = Obj(name, args)
        fr["last"] = obj
        return obj

    def builtin(self, name, args):
        # contenedores de arrays do CYPE (arr_*): emulados com listas Python
        if not name.startswith("arr_"):
            return NotImplemented
        if name.startswith("arr_Crea"):
            n = args[0] if args and isinstance(args[0], int) and not isinstance(args[0], bool) else 0
            return [None] * min(n, MAX_ITER)
        if name.startswith("arr_Set") and len(args) >= 3 and isinstance(args[0], list) and isinstance(args[1], int):
            if args[1] < len(args[0]):
                args[0][args[1]] = args[2]
            return None
        if name.startswith("arr_Append") and len(args) >= 2 and isinstance(args[0], list):
            args[0].append(args[1])
            return None
        if name.startswith("arr_Get") and len(args) >= 2 and isinstance(args[0], list) and isinstance(args[1], int):
            return args[0][args[1]] if args[1] < len(args[0]) else None
        if name.startswith("arr_NumElems") and args and isinstance(args[0], list):
            return len(args[0])
        return NotImplemented

    def invoke(self, ref, args):
        if not isinstance(ref, FuncRef):
            raise Unsupported("ponteiro de funcao indeterminado")
        return self.call_user(ref.lib, ref.name, args)

    def read_call(self, name, args, fr):
        stream = next((a for a in args if isinstance(a, Stream)), None)
        m = re.match(r"cypesave_(lee|salta)_(\w+)$", name)
        if m and stream is None:
            # o decompilador as vezes omite o argumento do fluxo: usa o fluxo do quadro atual
            stream = next((v for v in fr["env"].values() if isinstance(v, Stream)), None)
        if m and stream is not None:
            mode, kind = m.group(1), m.group(2)
            return self.read_prim(mode, kind, args, stream)
        if name.startswith("cypesave_nousar_lee_apunorde") or name.startswith("ebaarray_nousar_lee_apunorde"):
            return self.read_apunorde(name, args)
        m2 = re.match(r"cypesave_(?:nousar_)?salta_(estructura_opcional\w*|arr_punteros\w*)$", name)
        if m2 and stream is not None:
            fn = next((a for a in args if isinstance(a, FuncRef)), None)
            what = m2.group(1)
            if "opcional" in what:
                if not stream.take("<B"):
                    return None
            if what.startswith("arr_punteros"):
                n = stream.take("<I")
                if n > MAX_ITER:
                    raise Unsupported("array grande")
                for _ in range(n):
                    self.invoke(fn, [stream])
                return None
            self.invoke(fn, [stream])
            return None
        return NotImplemented

    def read_prim(self, mode, kind, args, st):
        kind = re.sub(r"_con_posible_error\w*", "", kind)
        opt = kind.endswith("_opcional")
        base = kind[:-9] if opt else kind
        if base in PRIM and mode == "lee" and not kind.startswith("arr"):
            return st.take(PRIM[base])
        if mode == "salta":
            if base in PRIM:
                st.skip(struct.calcsize(PRIM[base]))
                return None
            raise Unsupported("salta_" + kind)
        if base == "cadena_unicode":
            n = st.take("<I")
            raw = st.buf[st.pos:st.pos + 2 * n]
            st.skip(2 * n)
            return raw.decode("utf-16le", errors="replace")
        if base == "cadena":
            n = st.take("<H")
            raw = st.buf[st.pos:st.pos + n]
            st.skip(n)
            return raw.decode("latin-1")
        if base in ("cadena_ASCII",):
            n = st.take("<H")
            raw = st.buf[st.pos:st.pos + n]
            st.skip(n)
            return raw.decode("latin-1")
        if opt:
            flag = st.take("<B")
            if not flag:
                return None
        if base.startswith("arr_") and not base.startswith("arr_punteros"):
            sub = base[4:]
            if sub in PRIM:
                n = st.take("<I")
                if n > MAX_ITER:
                    raise Unsupported("array grande")
                return [st.take(PRIM[sub]) for _ in range(n)]
            if sub in ("puntos2D", "puntos3D", "puntos2DXPriv"):
                dim = 3 if "3D" in sub else 2
                n = st.take("<I")
                if n > MAX_ITER:
                    raise Unsupported("array grande")
                return [tuple(st.take("<d") for _ in range(dim)) for _ in range(n)]
            if sub == "cadenas_unicode" or sub == "cadenas":
                n = st.take("<I")
                out = []
                for _ in range(n):
                    out.append(self.read_prim("lee", "cadena_unicode" if sub == "cadenas_unicode" else "cadena", [], st))
                return out
            raise Unsupported("arr " + sub)
        if base.startswith("arr_punteros"):
            reader = next(a for a in args if isinstance(a, FuncRef))
            extras = [a for a in args if not isinstance(a, (Stream, FuncRef))]
            n = st.take("<I")
            if n > MAX_ITER:
                raise Unsupported("array grande")
            res = [self.invoke(reader, extras + [st]) for _ in range(n)]
            return res
        if base.startswith("estructura_opcional2"):
            refs = [a for a in args if isinstance(a, FuncRef)]
            extras = [a for a in args if not isinstance(a, (Stream, FuncRef))]
            flag = st.take("<B")
            if not flag:
                return None
            return self.invoke(refs[0], extras + [st])
        raise Unsupported("leitura " + kind)

    def read_apunorde(self, name, args):
        st = next(a for a in args if isinstance(a, Stream))
        refs = [a for a in args if isinstance(a, FuncRef)]
        # extras = argumentos que nao sao ponteiro de funcao nem o fluxo (ordem preservada)
        extras = [a for a in args if not isinstance(a, (Stream, FuncRef))]
        reader = refs[0]
        n = st.take("<I")
        if n > MAX_ITER:
            raise Unsupported("array grande")
        out = []
        for _ in range(n):
            out.append(self.invoke(reader, extras + [st]))
        return out


def load_funcs():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "lee_funcs.json.gz")
    with gzip.open(path, "rt", encoding="utf-8") as fh:
        pack = json.load(fh)
    return {dll: {n: (p, a) for n, (p, a) in fs.items()} for dll, fs in pack.items()}

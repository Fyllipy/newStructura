"""Leitor minimo de PBM (P4) e localizacao dos paineis (molduras) da Fig. 8 de [P2]. Python puro."""
import sys


def read_pbm(path):
    data = open(path, "rb").read()
    assert data[:2] == b"P4"
    # cabecalho: P4\nW H\n
    parts = []
    i = 2
    while len(parts) < 2:
        while data[i:i + 1].isspace():
            i += 1
        j = i
        while not data[j:j + 1].isspace():
            j += 1
        parts.append(int(data[i:j]))
        i = j
    i += 1
    w, h = parts
    stride = (w + 7) // 8
    rows = []
    for y in range(h):
        row = data[i + y * stride:i + (y + 1) * stride]
        rows.append(row)
    return w, h, rows


def px(rows, x, y):
    """1 = preto."""
    return 1 - ((rows[y][x >> 3] >> (7 - (x & 7))) & 1)


def black_run_rows(w, h, rows, min_len):
    """linhas com corrida horizontal de pixels pretos >= min_len."""
    out = []
    for y in range(h):
        best = run = 0
        for x in range(w):
            if px(rows, x, y):
                run += 1
                if run > best:
                    best = run
            else:
                run = 0
        if best >= min_len:
            out.append((y, best))
    return out


if __name__ == "__main__":
    w, h, rows = read_pbm(sys.argv[1])
    print(w, h)
    for y, b in black_run_rows(w, h, rows, int(sys.argv[2])):
        print(y, b)

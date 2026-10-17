"""Monta icons/app/app.ico (PNGs embutidos) a partir de icons/app/app-<tamanho>.png."""
import os
import struct

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
folder = os.path.join(root, "icons", "app")
sizes = [16, 24, 32, 48, 64, 128, 256]
blobs = []
for size in sizes:
    with open(os.path.join(folder, f"app-{size}.png"), "rb") as f:
        blobs.append((size, f.read()))

header = struct.pack("<HHH", 0, 1, len(blobs))
offset = 6 + 16 * len(blobs)
entries = b""
data = b""
for size, blob in blobs:
    dimension = 0 if size >= 256 else size
    entries += struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(blob), offset + len(data))
    data += blob
with open(os.path.join(folder, "app.ico"), "wb") as f:
    f.write(header + entries + data)
print("app.ico gerado com", len(blobs), "tamanhos")

#!/usr/bin/env python3
"""Makes the demo basketball, original geometry and texture, CC0. No extra packages needed."""
import json
import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "Craig_Vulkan" / "data" / "models"
rows, columns = 32, 64
positions, normals, uvs, indices = [], [], [], []
for row in range(rows + 1):
    theta = math.pi * row / rows
    for column in range(columns + 1):
        phi = 2 * math.pi * column / columns
        normal = (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))
        positions.extend(normal)
        normals.extend(normal)
        uvs.extend((column / columns, row / rows))
for row in range(rows):
    for column in range(columns):
        a = row * (columns + 1) + column
        b = a + columns + 1
        # the poles only need one triangle per slice, the other would have zero area
        if row > 0:
            indices.extend((a, a + 1, b))
        if row < rows - 1:
            indices.extend((a + 1, b + 1, b))

# black channels on orange rubber, the little alternating dots give it some grain
width, height = 512, 256
pixels = bytearray()
for y in range(height):
    pixels.append(0)  # PNG filter: none
    theta = math.pi * (y + 0.5) / height
    for x in range(width):
        phi = 2 * math.pi * (x + 0.5) / width
        nx, ny, nz = math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi)
        seam = min(abs(nx), abs(nz), abs((ny + nz) / math.sqrt(2)), abs((ny - nz) / math.sqrt(2))) < 0.018
        grain = 12 if (x % 4 == 0 and y % 4 == 0) else 0
        pixels.extend((24, 20, 17) if seam else (195 - grain, 83 - grain, 22))

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b"")
blob, views, accessors = bytearray(), [], []

def view(data, target=None):
    blob.extend(b"\0" * (-len(blob) % 4))
    entry = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
    if target is not None:
        entry["target"] = target
    views.append(entry)
    blob.extend(data)
    return len(views) - 1

def accessor(values, components, kind, fmt, component_type):
    entry = {"bufferView": view(struct.pack("<" + fmt * len(values), *values), 34963 if kind == "SCALAR" else 34962),
             "componentType": component_type, "count": len(values) // components, "type": kind}
    if values is positions:
        entry.update(min=[-1, -1, -1], max=[1, 1, 1])
    accessors.append(entry)
    return len(accessors) - 1

attributes = {"POSITION": accessor(positions, 3, "VEC3", "f", 5126),
              "NORMAL": accessor(normals, 3, "VEC3", "f", 5126),
              "TEXCOORD_0": accessor(uvs, 2, "VEC2", "f", 5126)}
index_accessor = accessor(indices, 1, "SCALAR", "H", 5123)
image_view = view(png)
model = {"asset": {"version": "2.0", "generator": "Craig basketball generator (CC0)"},
         "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
         "meshes": [{"primitives": [{"attributes": attributes, "indices": index_accessor, "material": 0}]}],
         "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0, "roughnessFactor": 0.9}}],
         "images": [{"bufferView": image_view, "mimeType": "image/png"}], "textures": [{"source": 0}],
         "buffers": [{"byteLength": len(blob)}], "bufferViews": views, "accessors": accessors}
text = json.dumps(model, separators=(",", ":")).encode()
text += b" " * (-len(text) % 4)
blob.extend(b"\0" * (-len(blob) % 4))
body = struct.pack("<I4s", len(text), b"JSON") + text + struct.pack("<I4s", len(blob), b"BIN\0") + blob
ROOT.mkdir(parents=True, exist_ok=True)
path = ROOT / "Basketball.glb"
path.write_bytes(struct.pack("<III", 0x46546C67, 2, len(body) + 12) + body)
print(path)

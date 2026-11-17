#!/usr/bin/env python3
"""Makes the hall textures and textured copies of BlankBox.glb, original artwork, CC0.

Uses the existing box geometry and material loader, no extra Python packages.
Run again after changing a hall's dimensions to rebuild its UV tiling.
"""
import copy
import json
import math
import struct
import zlib
from pathlib import Path

DATA = Path(__file__).resolve().parents[1] / "Craig_Vulkan" / "data"
MODELS = DATA / "models" / "halls"
TEXTURES = DATA / "textures" / "halls"
MODELS.mkdir(parents=True, exist_ok=True)
TEXTURES.mkdir(parents=True, exist_ok=True)
TILES = {"Wood": 2.4, "Brick": 1.2, "Plaster": 1.0, "Carpet": 1.2, "Glass": 1.0}


def png(path, width, height, pixel):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    pixels = bytearray()
    for y in range(height):
        pixels.append(0)
        for x in range(width):
            pixels.extend(max(0, min(255, round(v))) for v in pixel(x, y, width, height))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    path.write_bytes(data + chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b""))


def wood(x, y, footprint):
    board = math.floor(x / 0.18)
    grain = 4 * math.sin(x * 650 + 2 * math.sin(y * 3)) + 2 * math.sin(x * 1900 + math.sin(y * 8))
    tone = 8 * math.sin(board * 7.3) + grain
    # sub-pixel board gaps used to alias into dark stripes across the larger floors
    across = min(x % 0.18, 0.18 - x % 0.18)
    along = (y + (board % 3) * 0.8) % 2.4
    along = min(along, 2.4 - along)
    seam = max(0.0, min(1.0, (0.003 + footprint * 0.5 - min(across, along)) / footprint))
    return tuple(value * (1 - 0.25 * seam) for value in (193 + tone, 146 + tone, 88 + tone * 0.7))


def surface(material, x, y, width, height):
    u, v = x / width, y / height
    noise = ((x * 73 ^ y * 151 ^ x * y * 3) % 17) - 8
    if material == "Wood":
        return wood(u * 2.4, v * 2.4, 2.4 / width)
    if material == "Brick":
        row = int(v * 8)
        across = (u * 4 + (row % 2) * 0.5) % 1
        if across < 0.035 or (v * 8) % 1 < 0.09:
            return (177 + noise, 168 + noise, 150 + noise)
        tone = 12 * math.sin(math.floor(u * 4 + (row % 2) * 0.5) * 17 + row * 11)
        return (156 + tone + noise, 79 + tone + noise, 54 + tone + noise)
    if material == "Plaster":
        return (221 + noise * 0.3, 218 + noise * 0.3, 205 + noise * 0.3)
    if material == "Carpet":
        if x % (width // 2) < 2 or y < 2:
            return (42, 60, 69)
        weave = 3 if (x + y) % 3 == 0 else -2
        return (92 + weave + noise * 0.3, 119 + weave + noise * 0.3, 130 + weave + noise * 0.3)
    # opaque frosted backboards, the current renderer doesn't blend architectural glass
    border = min(u, 1 - u, v, 1 - v) < 0.025
    target = abs(abs(u - 0.5) - 0.2) < 0.012 and 0.25 < v < 0.65
    target |= abs(abs(v - 0.45) - 0.2) < 0.012 and 0.3 < u < 0.7
    return (243, 243, 233) if border or target else (153 + noise * 0.2, 183 + noise * 0.2, 191 + noise * 0.2)


def court(scale):
    half_x, _, half_z = scale
    # markings fit each room, they're visual guides rather than regulation-sized courts
    edge_x, edge_z = half_x - 0.6, half_z - 0.6
    radius = min(1.8, edge_x * 0.2, edge_z * 0.35)
    def pixel(x, y, width, height):
        px, pz = (x / width * 2 - 1) * half_x, (y / height * 2 - 1) * half_z
        colour = wood(px + half_x, pz + half_z, max(half_x * 2 / width, half_z * 2 / height))
        inside = abs(px) < edge_x and abs(pz) < edge_z
        boundary = (abs(abs(px) - edge_x) < 0.04 and abs(pz) <= edge_z + 0.04)
        boundary |= abs(abs(pz) - edge_z) < 0.04 and abs(px) <= edge_x + 0.04
        centre = inside and (abs(px) < 0.035 or abs(math.hypot(px, pz) - radius) < 0.035)
        key_depth, key_half = min(5.8, edge_x * 0.4), min(2.45, edge_z * 0.45)
        key = inside and ((abs(abs(px) - (edge_x - key_depth)) < 0.035 and abs(pz) < key_half)
                         or (abs(abs(pz) - key_half) < 0.035 and abs(px) > edge_x - key_depth))
        return (239, 234, 207) if boundary or centre or key else colour
    return pixel


for material in TILES:
    png(TEXTURES / f"{material}.png", 256, 256,
        lambda x, y, w, h, material=material: surface(material, x, y, w, h))

try:
    base = (DATA / "models" / "BlankBox.glb").read_bytes()
    json_size = struct.unpack_from("<I", base, 12)[0]
    original = json.loads(base[20:20 + json_size])
except (OSError, ValueError, struct.error) as error:
    raise SystemExit(f"Couldn't read BlankBox.glb: {error}") from error
bin_size = struct.unpack_from("<I", base, 20 + json_size)[0]
original_blob = base[28 + json_size:28 + json_size + bin_size]
made = set()
for scene_name in ("Hall", "MediumHall", "SmallHall", "SportsHall"):
    try:
        scene = json.loads((DATA / "scenes" / f"SoundPropagation{scene_name}.json").read_text())
    except (OSError, ValueError) as error:
        raise SystemExit(f"Couldn't read {scene_name}: {error}") from error
    for obj in scene["gameObjects"]:
        material = obj.get("components", {}).get("model", {}).get("acousticMaterial")
        if material not in TILES:
            continue
        scale = obj["scale"]
        floor = obj["name"] in ("Floor", "Sprung floor")
        label = "Court" if floor else material
        key = label + "_" + "_".join(format(s, ".6g").replace(".", "p") for s in scale)
        if key in made:
            continue
        made.add(key)
        image = f"{key}.png" if floor else f"{material}.png"
        if floor:
            png(TEXTURES / image, 1024, 1024, court(scale))
        model, blob = copy.deepcopy(original), bytearray(original_blob)
        primitive = model["meshes"][0]["primitives"][0]
        def attribute(name, index, count, model=model, primitive=primitive, blob=blob):
            accessor = model["accessors"][primitive["attributes"][name]]
            view = model["bufferViews"][accessor["bufferView"]]
            offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0) + index * view.get("byteStride", count * 4)
            return offset, struct.unpack_from("<" + "f" * count, blob, offset)
        for i in range(24):
            _, pos = attribute("POSITION", i, 3)
            _, normal = attribute("NORMAL", i, 3)
            uv_offset, _ = attribute("TEXCOORD_0", i, 2)
            axis = max(range(3), key=lambda j: abs(normal[j]))
            a, b = ((2, 1), (0, 2), (0, 1))[axis]
            uv = [(pos[j] + 1) * scale[j] / TILES[material] for j in (a, b)]
            if floor or material == "Glass":
                uv = [(pos[j] + 1) / 2 for j in (a, b)]
            struct.pack_into("<ff", blob, uv_offset, *uv)
        model["asset"]["generator"] = "Craig hall textures (CC0), existing BlankBox geometry"
        model["materials"] = [{"doubleSided": True, "pbrMetallicRoughness": {
            "baseColorTexture": {"index": 0}, "metallicFactor": 0,
            "roughnessFactor": 0.4 if material == "Wood" else 0.9}}]
        model["images"] = [{"uri": "../../textures/halls/" + image}]
        model["textures"] = [{"source": 0}]
        text = json.dumps(model, separators=(",", ":")).encode()
        text += b" " * (-len(text) % 4)
        body = struct.pack("<I4s", len(text), b"JSON") + text + struct.pack("<I4s", len(blob), b"BIN\0") + blob
        (MODELS / f"{key}.glb").write_bytes(struct.pack("<III", 0x46546C67, 2, len(body) + 12) + body)
print(f"Made {len(made)} textured boxes for the four halls")

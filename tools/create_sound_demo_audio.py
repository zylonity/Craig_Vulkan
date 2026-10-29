#!/usr/bin/env python3
"""Makes the demo sounds: a quiet tone, clicks and an impact. All made up here, CC0.

No recordings or samples from anywhere else. The tone uses whole periods so the two seconds loop
without a click, and the noise uses a fixed seed so the files come out the same every time.
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 48000
ROOT = Path(__file__).resolve().parents[1] / "Craig_Vulkan" / "data" / "audio"
ROOT.mkdir(parents=True, exist_ok=True)
partials = ((220, 0.30), (440, 0.15), (880, 0.15), (1760, 0.20), (3520, 0.20))
frames = bytearray()
for i in range(RATE * 2):
    sample = 0.3 * sum(weight * math.sin(2 * math.pi * hz * i / RATE) for hz, weight in partials)
    frames.extend(struct.pack("<h", round(sample * 32767)))
with wave.open(str(ROOT / "door_loop.wav"), "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(RATE)
    wav.writeframes(frames)
print(ROOT / "door_loop.wav")

# one decaying noise burst every 0.6 s, long enough apart that a 100 ms echo sits on its own
noise = random.Random(7)
period = round(RATE * 0.6)
frames = bytearray()
for i in range(period * 3):
    t = (i % period) / RATE
    sample = 0.8 * math.exp(-t / 0.004) * (noise.random() * 2.0 - 1.0) if t < 0.03 else 0.0
    frames.extend(struct.pack("<h", round(sample * 32767)))
with wave.open(str(ROOT / "clicks_loop.wav"), "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(RATE)
    wav.writeframes(frames)
print(ROOT / "clicks_loop.wav")

# a broad crack plus a low thump lasts longer than a click, without baking in any room echo
noise = random.Random(29)
samples = []
for i in range(round(RATE * 0.35)):
    t = i / RATE
    attack = 1.0 - math.exp(-t / 0.001)
    crack = 0.75 * math.exp(-t / 0.035) * (noise.random() * 2.0 - 1.0)
    thump = 0.55 * math.exp(-t / 0.075) * math.sin(2 * math.pi * (95 * t + 1.8 * (1.0 - math.exp(-t / 0.025))))
    body = 0.15 * math.exp(-t / 0.05) * math.sin(2 * math.pi * 430 * t)
    ending = min(1.0, (0.35 - t) / 0.02)
    samples.append(attack * ending * (crack + thump + body))
peak = max(abs(sample) for sample in samples)
frames = b"".join(struct.pack("<h", round(sample / peak * 0.8 * 32767)) for sample in samples)
with wave.open(str(ROOT / "impact.wav"), "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(RATE)
    wav.writeframes(frames)
print(ROOT / "impact.wav")

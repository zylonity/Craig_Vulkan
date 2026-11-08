#!/usr/bin/env python3
"""Makes the demo sounds: a quiet tone, clicks, an impact and a basketball. All made up here, CC0.

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

# a basketball dropped from 1.4 m: ballistic gaps between hits, each rebound keeps 84% of the speed
# (FIBA's 1.8 m drop / 1.2-1.4 m rebound rule works out at roughly 0.82-0.88), so the gaps leave room to hear the hall
noise = random.Random(53)
gravity, restitution = 9.81, 0.84
speed = first_speed = math.sqrt(2 * gravity * 1.4)
impacts, t = [], 0.02
while speed > 0.35:
    impacts.append((t, speed / first_speed))
    t += 2 * restitution * speed / gravity
    speed *= restitution
length = round(RATE * (impacts[-1][0] + 0.3))
samples = [0.0] * length
for start, strength in impacts:
    # softer hits stay in contact a bit longer, so they come out duller as well as quieter
    slap_decay = 0.0018 + 0.0015 * (1.0 - strength)
    smoothing = 0.35 + 0.45 * (1.0 - strength)
    filtered = 0.0
    first = round(start * RATE)
    for i in range(first, min(length, first + round(RATE * 0.25))):
        age = i / RATE - start
        attack = 1.0 - math.exp(-age / 0.0004)
        filtered += (1.0 - smoothing) * ((noise.random() * 2.0 - 1.0) - filtered)
        slap = 0.9 * math.exp(-age / slap_decay) * filtered
        shell = 0.55 * math.exp(-age / 0.022) * math.sin(2 * math.pi * 170 * age)
        floor = 0.45 * math.exp(-age / 0.06) * math.sin(2 * math.pi * (68 * age + 0.4 * (1.0 - math.exp(-age / 0.02))))
        ring = 0.08 * math.exp(-age / 0.05) * math.sin(2 * math.pi * 960 * age)
        samples[i] += strength * attack * (slap + shell + floor + ring)
peak = max(abs(sample) for sample in samples)
fade = round(RATE * 0.02)
frames = b"".join(struct.pack("<h", round(sample / peak * 0.8 * min(1.0, (length - i) / fade) * 32767)) for i, sample in enumerate(samples))
with wave.open(str(ROOT / "basketball.wav"), "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(RATE)
    wav.writeframes(frames)
print(ROOT / "basketball.wav", f"{len(impacts)} bounces, {length / RATE:.2f} s")

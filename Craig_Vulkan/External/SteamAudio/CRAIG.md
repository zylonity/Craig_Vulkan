# Steam Audio (runtime slice)

Valve Corporation, Apache-2.0: https://github.com/ValveSoftware/steam-audio

Vendored release: v4.8.1, commit `0da18255cca520771f363ee01f100572b39a308e`,
from `core/src/core/`. Licence retained in `LICENSE.md`; upstream has no NOTICE file.

Only the energy-field to impulse-response path is included: reconstruction,
energy fields, impulse responses, IIR filters, air absorption and their support
code. The simulator, scene, HRTF, FlatBuffers and profiler code are not.

Local changes, each marked in the file:
- `energy_field.*`, `impulse_response.cpp`: no baked-data serialisation, channel
  counting via `sh_order.h` instead of the full SH library.
- `reconstructor.cpp`: no profiler/log dependency; arithmetic unchanged.
- `sh_order.h`, `standalone_memory.cpp`: new, replace the SH library's channel
  count and the global context's allocator.

The listener-side ray tracing (`reflection_simulator.cpp`) is translated to GLSL in
`data/shaders/SteamAudioPropagation.glsl` / `SoundPropagation.comp`, and
`sampling.cpp` to `Craig/Renderer/Craig_SteamAudioSampling.hpp`.

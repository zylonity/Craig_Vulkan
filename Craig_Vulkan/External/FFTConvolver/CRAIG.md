# FFTConvolver

HiFi-LoFi, MIT: https://github.com/HiFi-LoFi/FFTConvolver

Vendored revision: `f2cdeb04c42141d2caec19ca4f137398b2a76b85`.
Licence retained in `COPYING.txt` and source headers. Only the library source,
headers, upstream README and licence are included, not its test executable.

Local integration change: `Utilities.h` includes `<xmmintrin.h>` outside
`namespace fftconvolver`. Upstream's namespace-scoped include hid SIMD types
and intrinsics from Jolt when the reverb header was included first.

Craig uses `TwoStageFFTConvolver` with 128/2048-sample partition sizes. Filter
initialisation/destruction happens on the preparation worker (or after SDL
has retired its callback); `process()` runs on the audio callback. The default
synchronous convolution path is used, not the optional background overrides.

Demo sounds
-----------

door_loop.wav   - two second harmonic tone, loops without a click
clicks_loop.wav - decaying noise clicks every 0.6 s, so echoes have room to stand on their own
impact.wav      - 0.35 s crack/thump, no echo or reverb baked into it
basketball.wav  - 5.5 s ball dropped from 1.4 m, 16 bounces. Restitution 0.84 (FIBA's rebound rule
                  works out at about 0.82-0.88) so the gaps are proper ballistic ones, but the
                  tones are tuned by ear, not measured off a real ball. Best in the sports hall

All made by tools/create_sound_demo_audio.py, run it again to remake them:
  python3 tools/create_sound_demo_audio.py

No recordings, samples or music from anywhere else. Dedicated to the public domain under CC0 1.0:
https://creativecommons.org/publicdomain/zero/1.0/

48 kHz, mono, 16-bit PCM. They're for comparing the propagation, not calibrated sources.

Trying the sound propagation
---------------------------
Run normally in CLion, press Play, then pick a reference and sound in Moving-door demo.
Impact/Basketball wait for the Play sound button. Clicks/Tone loop automatically.
Up to 16 presses overlap at the same source position, not 16 separate sound sources.
Pause, mute and scene changes cancel them. Stop returns to the editor scene snapshot.

Direct and Reflections let you listen to each part separately; they aren't GPU timing baselines.
The GPU reflections checkbox in Sound propagation actually switches reflection tracing off.
Purple bouncing rays start at the listener; they're search rays, not confirmed source-to-ear paths.
White is the direct centre line, yellow is the direct sampling overlay. Lines show through walls.

Reflections use Steam Audio 4.8.1's listener-side tracing method, ported to Vulkan GLSL.
The CPU adds the returned energy into 200 bins of 10 ms each, timed after the direct sound.
Steam Audio's reconstruction turns those bins into a response on a worker thread, then
FFTConvolver applies it to the dry sound. Direct travel time gets added back once.
New filters fade their input in while the old one's echoes finish, instead of cutting them off.
See External/SteamAudio/CRAIG.md and its Apache-2.0 licence, and FFTConvolver/COPYING.txt (MIT).

Audio timing logs once a second. The callback has one buffer's duration to finish its work.
Over-budget counts and long callback gaps are clues, not hardware underrun measurements.
logs/reverb-energy.csv is the last PREPARED reflection response's input bins, not necessarily
what's currently playing. It records direct delay, sample rate and total reconstructed IR energy.
Debug keeps the FFT libraries and Audio/Reverb wrappers at -O2, with symbols/asserts still on.
The output gain is 1.2, and stacking lots of sounds can hit the output clamp.

Limits: 2 s/64 bounces, one source, three bands, centred mono reflections, no HRTF or diffraction.
Direct through-wall gain uses the nearest material per ray, not thickness or multiple walls.
The offline check in tools/steam_audio_parity.cpp checks ported maths and tail lifetime, not
full stock Steam Audio performance or measured room accuracy. Listening checks are still needed.

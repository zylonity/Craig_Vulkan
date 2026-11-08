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

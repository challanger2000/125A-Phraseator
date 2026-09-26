# Sample I/O Baseline

## Realtime boundary

Sample decoding and file I/O are non-realtime operations.

The audio callback receives only immutable audio-buffer views that have already been decoded and validated.

## WAV baseline

The initial internal decoder supports standard little-endian RIFF/WAVE:

- mono or stereo;
- PCM 16-bit;
- PCM 24-bit;
- PCM 32-bit;
- IEEE float 32-bit.

Unsupported or malformed data is rejected explicitly.

Non-finite float samples are sanitized to zero during decoding.

## Ownership

Decoded audio is stored in OwnedAudioSource.

It owns its sample memory with std::vector and can expose a lightweight AudioBufferView for realtime playback.

OwnedAudioSource allocation/resizing is never intended to happen in the audio callback.

## Not yet implemented

- file chooser / browser;
- disk-path persistence;
- processor/controller transfer;
- safe bank swap into the realtime processor;
- AIFF or compressed audio;
- automatic transient analysis.

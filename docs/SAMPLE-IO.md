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

## File loading

SampleFileLoader performs the complete disk read outside the realtime path and only returns a decoded OwnedAudioSource after the WAV data validates successfully.

Failure states distinguish:
- file open failure;
- invalid/unrepresentable file size;
- read failure;
- WAV decode failure.

No partial decoded source is published after a failure.

## Ownership

Decoded audio is stored in OwnedAudioSource.

It owns its sample memory with std::vector and can expose a lightweight AudioBufferView for realtime playback.

OwnedAudioSource allocation/resizing is never intended to happen in the audio callback.

## Realtime handoff

Decoded sources are staged in the inactive SampleBank and atomically published to the audio thread only after validation.

## Implemented loading path

The VST3 editor can select WAV files for visible source slots. Paths are sent to the processor, decoded asynchronously, and published through the non-realtime sample-bank exchange.

Loop loads prefer transient-based slicing when reliable onsets are found. If onset analysis is inconclusive, Phraseator falls back to the requested equal grid so the load remains musically usable.

## Not yet implemented

- AIFF or compressed audio.

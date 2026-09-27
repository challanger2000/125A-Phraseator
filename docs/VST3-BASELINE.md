# VST3 Layer Baseline

## Architecture

Phraseator uses the standard Steinberg processor/controller split.

Processor:
- owns realtime state;
- exposes stereo instrument output;
- accepts an event input for future MIDI interaction;
- reads host tempo and project sample position;
- performs no file I/O in process();
- serializes deterministic project state;
- executes GENERATE / VARIATE on explicit trigger edges;
- honors LOCK PATTERN for those generation actions.

Controller:
- exposes stable parameter IDs;
- restores visible parameter values from component state;
- owns the VSTGUI editor and file-selection workflow;
- receives the current 16-step pattern through hidden read-only output parameters, avoiding realtime UI messages from the processor.

## Product category

The processor is registered as:

Instrument|Sampler

It intentionally has:
- zero audio inputs;
- one stereo audio output;
- one event input.

## Audio processing baseline

The current realtime core supports:
- deterministic 16-step playback;
- host-tempo synchronization;
- exact host-block boundary triggering;
- sample-synchronized ratchets;
- fragment pitch playback;
- stereo pan variation;
- source-balanced fragment selection: generation chooses a loaded source approximately uniformly first, then a fragment/slice inside that source, preventing sliced loops from dominating one-shots purely by slice count.

The processor currently advertises 32-bit sample processing only. 64-bit audio processing will not be claimed until the realtime playback path actually supports it.

## Current editor / sample-loading baseline

The VST3 layer now includes:
- a functional VSTGUI editor;
- explicit ONE / LOOP WAV loading for eight V0.1 source slots;
- WAV drag-and-drop on every source status field, accepting both VSTGUI file-path payloads and host-browser text/file-URL payloads on Windows, with conservative AUTO one-shot/loop classification;
- per-slot clear action that removes the source from the published bank and fragment pool;
- asynchronous non-realtime sample decoding and bank publication;
- transient-preferred LOOP slicing with equal-grid fallback;
- conservative offline pitch detection with persisted resolved root metadata;
- 16-step pattern visualization driven by hidden processor output parameters;
- source-bay EMPTY / ONE / LOOP status driven by the actually active realtime sample bank, so failed loads are not shown as successful;
- Generate / Variate / Lock controls;
- generation macro controls;
- key / scale controls;
- Delay / Filter finishing controls.

The technical source bank retains capacity for 16 slots, while V0.1 intentionally exposes 8 user-facing slots. Additional paging and an internal sample browser are not part of the V0.1 product surface.

## SDK baseline

The VST3 target uses Steinberg VST3 SDK:

v3.8.1_build_84

This matches the current baseline already used by the active 125A Final repository inspected during implementation.

## State

State format begins with:
- magic: PHR1
- version: 4

The component state includes generation macros, key/scale state, pattern lock state and the current 16-step pattern.

No project reload generates a fresh pattern automatically.

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
- stereo pan variation.

The processor currently advertises 32-bit sample processing only. 64-bit audio processing will not be claimed until the realtime playback path actually supports it.

## Current editor / sample-loading baseline

The VST3 layer now includes:
- a functional VSTGUI editor;
- explicit ONE / LOOP WAV loading for the first visible eight source slots;
- asynchronous non-realtime sample decoding and bank publication;
- transient-preferred LOOP slicing with equal-grid fallback;
- conservative offline pitch detection with persisted resolved root metadata;
- 16-step pattern visualization driven by hidden processor output parameters;
- source-bay EMPTY / ONE / LOOP status driven by the actually active realtime sample bank, so failed loads are not shown as successful;
- Generate / Variate / Lock controls;
- musical macro controls;
- key / scale controls;
- Delay / Filter finishing controls.

The technical source bank remains 16 slots. The first editor baseline exposes slots 1-8; bank/page access for slots 9-16 is a later UI task.

## SDK baseline

The VST3 target uses Steinberg VST3 SDK:

v3.8.1_build_84

This matches the current baseline already used by the active 125A Final repository inspected during implementation.

## State

State format begins with:
- magic: PHR1
- version: 3

The component state includes generation macros, key/scale state, pattern lock state and the current 16-step pattern.

No project reload generates a fresh pattern automatically.

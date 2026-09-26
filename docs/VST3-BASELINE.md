# VST3 Layer Baseline

## Architecture

Phraseator uses the standard Steinberg processor/controller split.

Processor:
- owns realtime state;
- exposes stereo instrument output;
- accepts an event input for future MIDI interaction;
- reads host tempo and project sample position when available;
- performs no file I/O in process();
- serializes deterministic project state.

Controller:
- exposes stable parameter IDs;
- restores visible parameter values from component state;
- currently has no editor.

## Product category

The processor is registered as:

Instrument|Sampler

It intentionally has:
- zero audio inputs;
- one stereo audio output;
- one event input.

## Current limitation

The VST3 shell is an infrastructure milestone, not yet a usable sampler build.

The file-loader / decoded audio ownership layer is not connected yet, so the processor has no user-loaded audio to play.

## SDK baseline

The VST3 target uses Steinberg VST3 SDK:

v3.8.1_build_84

This matches the current baseline already used by the active 125A Final repository inspected during implementation.

## State

State format begins with:
- magic: PHR1
- version: 1

The component state includes generation macros, key/scale state, pattern lock state and the current 16-step pattern.

No project reload generates a fresh pattern automatically.

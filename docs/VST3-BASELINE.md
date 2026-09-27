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


## MIDI phrase control

- MIDI note-on is consumed by the processor at the host-provided sample offset.
- If loaded material exists and the phrase pattern is still empty, the first note-on generates a phrase automatically.
- MIDI note 60 is the neutral transpose reference; incoming notes transpose phrase playback within a conservative +/-24 semitone input range.
- Overlapping notes are tracked without allocation; on note release, another held note remains in control.
- MIDI transpose is playback-only and does not rewrite the stored/generated base pattern.
- Processor deactivate/stop clears held-note state and returns transpose to neutral.

## AUTO source classification

AUTO classification is intentionally conservative:
- multiple transient divisions alone are not sufficient for LOOP;
- candidate loop events must be distributed into the later part of the file;
- strongly decaying energy profiles are treated as ONE even when reverb reflections create additional transient-like peaks;
- manual ONE/LOOP loading remains authoritative.


## MIDI phrase gate and phase mode

- Phrase playback is gated by incoming MIDI notes; host transport alone does not sound the phrase.
- RESTART is the default: every new note-on resets phrase phase so Step 1 starts at the note's exact host sample position.
- CONTINUE keeps phrase phase tied to the host timeline; incoming notes gate and transpose the continuing phrase without forcing Step 1.
- MIDI note 60 remains neutral transpose; higher/lower notes transpose phrase playback.
- Generate/Variate custom buttons use explicit controller action pulses so each click reaches the processor deterministically.
- Loading additional sources preserves the current pattern; pressing GENERATE rebuilds source assignments from all currently loaded sources.


## 2026-09-27 measured regression fixes

- Generate/Variate GUI actions use direct controller-to-processor VST3 messages; the processor consumes them on the audio thread via lock-free pending flags.
- RESTART phase uses a monotonic local phrase clock. DAW loop-wrap/project-time jumps cannot clamp restart phase back to zero and create rapid retriggers.
- SampleBank copy semantics explicitly rebind AudioBufferView pointers to copied OwnedAudioSource storage, preventing earlier sources from referencing the previous bank after a later source is loaded.
- Regression coverage verifies copied multi-source banks keep both source views valid and independent.
- Generation macro regression coverage verifies high Density produces more than twice the active-step population of low Density across deterministic fixtures, and Pitch/Pan/Groove produce measurable non-zero effects after generation.
- Existing source-balanced generation regression continues to verify comparable source-level use of a one-shot versus a 16-slice loop.


## 2026-09-27 measured FX and pattern-edit update

- DELAY is a true dry/wet crossfade: 0% = dry, 100% = wet after the existing click-free 20 ms smoothing settles.
- Regression coverage verifies settled 100% DELAY suppresses the dry impulse and produces the tempo-synced delayed response.
- FILTER uses a musical macro mapping rather than the previous overly dark 800 Hz endpoint:
  - 0% bypass
  - 25% approximately 14 kHz
  - 50% approximately 8 kHz
  - 75% approximately 3.5 kHz
  - 100% approximately 1.2 kHz
- Filter regression coverage measures response at 1 kHz and 10 kHz and verifies strong high-frequency attenuation while retaining useful midrange.
- The 16 visible pattern steps are directly editable by left click. A click toggles the step on/off while retaining its current fragment assignment; activating an empty step starts with fragment 1.
- Pattern edits are sent as direct controller-to-processor messages and applied through fixed-size atomic pending state on the audio thread.
- Existing pitch-to-key regression tests verify deterministic quantization to the selected root/scale.


## 2026-09-27 source-number pattern editing

- Pattern step labels now represent visible source slots 1-8, not internal flat fragment indices.
- Left-click cycles through currently loaded sources only.
- Right-click clears the step.
- Manual selection of a loop source targets that source's first slice; generated/varied patterns may still use any valid slice internally.
- Pattern display maps internal fragments back to their owning source before showing the step number.
- Core regression verifies source-to-flat-fragment and flat-fragment-to-source mapping when earlier sources contain multiple slices.

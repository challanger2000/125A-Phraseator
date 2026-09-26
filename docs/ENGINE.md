# Phrase Engine - Initial Technical Model

## Core entities

### Source
One imported audio file plus metadata. File decoding and disk access belong to a non-realtime layer.

### Fragment
A playable region of a source:
- entire one-shot, or
- one slice of a loop.

The realtime engine only receives validated, preloaded source/fragment metadata and audio buffers. No file I/O is permitted in the audio callback.

### Step
A phrase event containing:
- fragment index,
- active/rest state,
- velocity,
- pitch offset,
- pan,
- gate length,
- repeat count,
- timing offset.

### Pattern
A fixed-size sequence of steps. Initial target: 16 steps.

## Generation philosophy

Generation must use weighted musical decisions instead of independent random values.

Initial rules:

Evidence classification for the musical generation heuristics below:
**EMPIRICALLY TUNED**. They are product-behaviour choices that must later be
validated with listening fixtures and usage tests; they are not presented as
music-theory laws.

- Strong beats receive higher event probability than weak subdivisions.
- Density scales the number of occupied steps without destroying pulse.
- Repeats prefer short contiguous groups.
- Variation mutates an existing pattern rather than always replacing it.
- Avoid pathological same-fragment repetition unless Repeat is high.
- Use a local motif anchor per quarter-note group so active steps are not sixteen unrelated sample choices.
- Prefer recent/local fragment continuity, with Repeat increasing the probability of deliberate adjacent reuse.
- VARIATE preserves an existing active step's fragment more often than it replaces it, so variation behaves like mutation rather than a fresh random pattern.
- Pan is bounded by the PAN macro and defaults near center.
- Pitch changes are constrained when key/scale mode is enabled.
- Fixed random seeds are supported for deterministic recall.

## Source/fragment model

Initial limits:
- 16 active source slots.
- One-shot = one fragment.
- Loop = up to 64 validated slices.
- Fragment lookup is deterministic and allocation-free.
- Source sample rate is converted against the current host sample rate during playback, before creative pitch transposition is applied.
- Invalid/overlapping slice definitions are rejected before realtime use.

These limits are implementation baselines, not final product claims.

## Determinism

Project recall requires deterministic output.

Store:
- random seed,
- current pattern,
- source/slice references,
- generation parameters.

No background randomization may occur merely from opening a project.

## Realtime boundary

The audio callback must not:
- open files,
- decode audio,
- allocate memory,
- acquire blocking locks,
- log or format strings.

Loading, decoding, transient analysis and future pitch analysis must happen outside the realtime callback. Results are published to the processor only after validation.

# Phrase Engine - Initial Technical Model

## Core entities

### Source
One imported audio file plus metadata.

### Fragment
A playable region of a source:
- entire one-shot, or
- one slice of a loop.

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
- Strong beats receive higher event probability than weak subdivisions.
- Density scales the number of occupied steps without destroying pulse.
- Repeats prefer short contiguous groups.
- Variation mutates an existing pattern rather than always replacing it.
- Avoid pathological same-fragment repetition unless Repeat is high.
- Pan is bounded by the PAN macro and defaults near center.
- Pitch changes are constrained when key/scale mode is enabled.
- Fixed random seeds are supported for deterministic recall.

## Determinism

Project recall requires deterministic output.

Store:
- random seed,
- current pattern,
- source/slice references,
- generation parameters.

No background randomization may occur merely from opening a project.

# State / Parameter Contract

> **Current state:** Phraseator V1.0.0 currently serializes **state version 9**. This document records the compatibility evolution; earlier version sections remain for migration history. The current release pointer is [../CURRENT.md](../CURRENT.md), while the implementation and regression tests remain authoritative for exact field order and migration behavior.


This file defines the compatibility baseline for Phraseator.

## Stable parameter IDs

The V0.1 parameter ID ranges are reserved as follows:

- 1000-1099: musical generation macros
- 1100-1199: key / scale / pitch options
- 1200-1299: internal finishing FX
- 1300-1399: actions / pattern locking
- 1400-1415: hidden read-only pattern-view mirrors (UI only; not authoritative project state)

Once a public build exists, IDs and meanings must not be silently reused or reinterpreted.

## Project state

### Version 1

Stores:
- deterministic random seed;
- musical macro values;
- key / scale settings;
- lock state;
- current 16-step pattern;
- internal FX amounts.

### Version 2

Adds fixed per-slot source metadata and UTF-8 source paths:
- occupied state;
- source ID;
- one-shot / equal-sliced-loop mode;
- slice division count;
- tonal flag;
- detected root note;
- external source path.

### Version 3

Adds a per-source compatibility flag indicating whether loop reload should prefer transient slicing before falling back to its stored equal-division grid.

Version 1 remains readable. It simply restores with no remembered sample paths.
Version 2 remains readable and defaults the transient-preference flag to false, preserving the original equal-slice behavior.

Decoded audio itself is never serialized into project state.

## Recall rule

Opening a project must not generate a fresh pattern.

The stored pattern and random seed remain authoritative.

For State v2/v3, existing source files are queued as one non-realtime batch after state restoration. Missing files are skipped without substituting another sample; their remembered path remains in the saved state.

For newly loaded material, source recall metadata is committed only after decode/slicing/bank publication succeeds. New LOOP loads prefer transient slicing and deterministically fall back to the stored equal-division count when no reliable boundaries are found. When automatic pitch analysis resolves a stable tonal root, that resolved tonal flag and root note are persisted too, so later project reload does not depend on re-running a potentially changed detector.

## Realtime boundary

Variable-length source paths are stored outside the realtime ProjectState object and protected by a mutex that is never touched from process().

The audio callback does not perform path lookup, filesystem access, decoding or state-string allocation.

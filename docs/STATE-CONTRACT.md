# State / Parameter Contract

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

Version 1 remains readable. It simply restores with no remembered sample paths.

Decoded audio itself is never serialized into project state.

## Recall rule

Opening a project must not generate a fresh pattern.

The stored pattern and random seed remain authoritative.

For State v2, existing source files are queued as one non-realtime batch after state restoration. Missing files are skipped without substituting another sample; their remembered path remains in the saved state.

For newly loaded material, source recall metadata is committed only after decode/slicing/bank publication succeeds. When automatic pitch analysis resolves a stable tonal root, that resolved tonal flag and root note are persisted too, so later project reload does not depend on re-running a potentially changed detector.

## Realtime boundary

Variable-length source paths are stored outside the realtime ProjectState object and protected by a mutex that is never touched from process().

The audio callback does not perform path lookup, filesystem access, decoding or state-string allocation.

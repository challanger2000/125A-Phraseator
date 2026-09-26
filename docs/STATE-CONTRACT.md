# State / Parameter Contract

This file defines the compatibility baseline before the VST3 layer is added.

## Stable parameter IDs

The V0.1 parameter ID ranges are reserved as follows:

- 1000-1099: musical generation macros
- 1100-1199: key / scale / pitch options
- 1200-1299: internal finishing FX
- 1300-1399: actions / pattern locking

Once a public build exists, IDs and meanings must not be silently reused or reinterpreted.

## Project-state baseline

State version 1 stores:

- deterministic random seed;
- musical macro values;
- key / scale settings;
- lock state;
- current pattern;
- source metadata references;
- internal FX amounts.

Actual decoded sample audio is not serialized into the realtime state structure.

## Recall rule

Opening a project must not generate a fresh pattern.

The stored pattern and random seed are authoritative until the user explicitly presses GENERATE / VARIATE or changes a control whose documented behavior intentionally regenerates material.

## Source persistence

The future file-loading layer must store enough non-realtime metadata to relocate user-selected source files safely.

If a source file cannot be found, the plugin must fail gracefully and preserve the rest of the project state rather than silently substituting another file.

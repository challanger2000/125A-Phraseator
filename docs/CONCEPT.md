# 125A Phraseator - V0.1 Concept

## Product goal

Phraseator turns arbitrary audio material into immediately useful rhythmic phrases.

The user should not need to program a sampler in detail. The primary workflow is:

1. Load one-shots and/or loops.
2. Slice loops when useful.
3. Press **GENERATE**.
4. Adjust a few musical macro controls.
5. Lock what works.
6. Optionally add simple internal FX.

## Design principles

- Musical results before feature count.
- Controlled variation, not arbitrary chaos.
- The source material is chosen by the user.
- Any sophisticated analysis stays under the hood.
- No requirement for Kontakt or another sampler platform.
- External FX remain fully valid; internal FX are convenience tools only.

## V1 source model

- Up to 16 active source slots.
- A slot may contain a one-shot or a loop.
- Loops can expose slices as playable fragments.
- Fragments share one phrase-generation pool.
- Sample loading is from external files; Phraseator does not require a bundled library.

## V1 macro controls

- DENSITY - how busy the phrase is.
- VARIATION - how far generation may deviate from the current phrase.
- REPEAT - probability/intensity of repeats and ratchets.
- PITCH - amount of tonal movement.
- PAN - amount of stereo placement variation.
- GROOVE - timing feel / controlled microtiming.

Primary actions:

- GENERATE
- VARIATE
- LOCK PATTERN
- Per-source lock

## Pitch behavior

Pitch correction is optional.

For clearly tonal material, Phraseator may:
- detect or accept a root note,
- transpose toward a chosen key,
- constrain generated notes to a selected scale.

Percussive/noisy material must remain usable without pitch analysis.

## Slicing

Loop slicing supports:
- equal divisions,
- transient-based slicing,
- manual correction later.

The slices become ordinary phrase fragments for the generator.

## Internal FX

Keep deliberately simple:
- tempo-synced Delay,
- Reverb,
- Drive,
- Filter.

These are finishing tools, not a second product inside the product.

## Non-goals for V1

- Full workstation sampler
- Deep synthesis engine
- Automatic analysis/indexing of the user's entire sample library
- Multi-output routing matrix
- Complex modulation system
- Cloud/AI dependency

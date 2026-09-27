# 125A Phraseator - V0.1 Concept

## Product goal

Phraseator turns arbitrary audio material into immediately useful rhythmic phrases.

The user should not need to program a sampler in detail. The primary workflow is:

1. Load one-shots and/or loops.
2. Adjust a few generation macro controls.
3. Press **GENERATE**.
4. Use **VARIATE** when useful.
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

- 8 user-facing source slots in V0.1.
- The engine reserves technical capacity for up to 16 sources without requiring a second UI page in V0.1.
- A slot may contain a one-shot or a loop and can be replaced or cleared.
- Loops can expose slices as playable fragments.
- Fragments share one phrase-generation pool.
- Sample loading is from external files; Phraseator does not require a bundled library.
- WAV drag-and-drop onto any visible source slot is a primary fast path where the host supports file-path drops.
- ONE / LOOP file-dialog actions remain explicit host-independent fallbacks.
- No internal sample browser is required for V0.1.

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

## Pitch behavior

Pitch correction is optional.

For clearly tonal material, Phraseator may:
- detect or accept a root note,
- transpose toward a chosen key,
- constrain generated notes to a selected scale.

Percussive/noisy material must remain usable without pitch analysis.

## Slicing

Loop slicing supports:
- transient-preferred automatic slicing,
- deterministic equal-division fallback when reliable onsets are not found.

The resolved slice boundaries are stored in project state so recall does not depend on re-running analysis. The slices become ordinary phrase fragments for the generator.

## Internal FX

Keep deliberately simple:
- tempo-synced Delay,
- Filter.

Reverb and Drive remain reserved research items until separately validated.

These are finishing tools, not a second product inside the product.

## Non-goals for V1

- Full workstation sampler
- Deep synthesis engine
- Automatic analysis/indexing of the user's entire sample library
- Multi-output routing matrix
- Complex modulation system
- Cloud/AI dependency
- Manual waveform/slice editor
- Source-bank paging beyond the 8-slot V0.1 surface

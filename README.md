# 125A Phraseator

Generative sample phrase engine for VST3.

## Product idea

Load arbitrary one-shots or loops, optionally slice and tune them, then generate musically useful rhythmic phrases with very few controls.

**Core promise:** material in -> GENERATE -> usable phrase -> shape -> done.

## V1 scope

- One-shots and loop import
- WAV drag-and-drop directly onto the 8 source slots, with AUTO one-shot/loop classification
- Loop slicing
- Musical phrase generation
- Controlled variation rather than blind randomization
- Optional pitch-to-key / scale behavior for tonal material
- Density, Variation, Repeat, Pitch, Pan and Groove controls
- Simple built-in Delay and Filter; Reverb/Drive remain reserved research items until separately validated
- Pattern locking
- Host-sync transport and tempo
- Windows x64 VST3 first

The UI should remain simple even when the internal rules are sophisticated. Phraseator deliberately does not include an internal sample browser; host/OS drag-and-drop plus explicit ONE / LOOP file loading cover the V0.1 workflow.

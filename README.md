# 125A Phraseator

Generative sample phrase engine for VST3.

## Product idea

Load arbitrary one-shot WAV samples, then generate musically useful rhythmic phrases with very few controls.

**Core promise:** material in -> GENERATE -> usable phrase -> shape -> done.

## V1 scope

- Eight one-shot sample slots
- WAV drag-and-drop directly onto the source slots plus explicit LOAD buttons
- Conservative automatic source level matching on load
- Musical phrase generation without forcing every loaded source into every pattern
- Controlled variation rather than blind randomization
- Optional pitch-to-key behavior with Chromatic / Major / Minor
- Discrete octave selector: OFF / +1 / -1 / +/-1
- Density, Variate Depth, Repeat, Velocity, Pitch, Pan and Groove controls
- PAN 0 is true mono/center; higher PAN settings create generated placement
- Simple built-in Delay and Cut processing
- Pattern locking and manual source assignment per step
- Host-sync transport and tempo
- Windows x64 VST3 first

Phraseator V1 deliberately does not act as a loop slicer. Loop import, AUTO loop classification and visible LOOP controls are excluded so the workflow stays focused and predictable.

The UI should remain simple even when the internal rules are sophisticated. Phraseator deliberately does not include an internal sample browser; host/OS drag-and-drop plus explicit LOAD buttons cover the V0.1 workflow.

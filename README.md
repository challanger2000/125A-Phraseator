# 125A Phraseator

## Current release

**125A Phraseator V1.0.0 - Windows x64 VST3**

- Shipping source: `release-v1.0.0`
- Shipping commit: `b01e7e9f58bcaf45572b311754711c2fa2ba6c6e`
- Verified CI run: **#431**
- Core tests: **16/16 PASS**
- Steinberg VST3 Validator: **47/47 PASS**
- Gumroad package: `125A_Phraseator_v1.0.0_Windows_x64_Gumroad.zip`
- Gumroad package SHA-256: `4f3d5a42edf5e75ba571001b350292c81bd2b0c6747009dd20b26da1088da3bb`

See [CURRENT.md](CURRENT.md) for the authoritative release pointer and verification notes.


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
- Density, Variate Depth, Motif Reuse, Velocity, Pitch, Pan and Groove controls
- PAN 0 preserves the source format: stereo WAVs keep their native L/R image and mono WAVs remain centered; higher PAN settings create generated placement
- Simple built-in Delay and Cut processing
- Pattern locking and manual source assignment per step
- Host-sync transport and tempo
- Windows x64 VST3 first

Phraseator V1 deliberately does not act as a loop slicer. Loop import, AUTO loop classification and visible LOOP controls are excluded so the workflow stays focused and predictable.

The UI should remain simple even when the internal rules are sophisticated. Phraseator deliberately does not include an internal sample browser; host/OS drag-and-drop plus explicit LOAD buttons cover the V1 workflow.

## Control model

- **CREATE / CHANGE PHRASE:** Density, Motif Reuse and Variate Depth define or mutate the phrase structure.
- **LIVE SHAPE:** Pitch, Octave, Velocity, Pan, Groove and Pitch To Key do not require Generate and do not replace the current pattern. PAN is continuous on active voices; trigger-bound pitch, octave, velocity, groove and key changes apply at the next musical event.


## Step articulation

- **MOTIF REUSE** controls how strongly source choices repeat across the generated phrase.
- **RATCHET · HITS / STEP** is edited independently per step as 1x / 2x / 3x / 4x.
- Long samples may ring through empty steps. The next active step, regardless of source, chokes the previous phrase voice with a short de-click release.
- **PHRASE MODE — CONTINUE** keeps the phrase timeline running with the host through MIDI note gaps and only gates the direct phrase audio; **RETRIGGER** restarts the phrase from step 1 on each new MIDI note.

- Regression coverage verifies long-sample choke across empty steps and the CONTINUE/RETRIGGER timing contract.

- Audit coverage includes exact 1x-4x ratchet timing, long-sample choke, CONTINUE/RETRIGGER timing, delay OFF history isolation, multi-rate step timing, 2 ms choke release, auto-level bounds/stereo, and realtime parameter-capacity/state checks.

- Live PAN now updates already-running voices without retriggering; Pitch To Key is applied after MIDI transpose so Root/Scale remain the final pitch authority.

- Second audit checkpoint: stale fragment rejection, authoritative mute spans, WAVE_FORMAT_EXTENSIBLE, overlap-safe MIDI note tracking, delay bypass/re-enable, denormal guards, source-specific mute choke, bank-swap voice reset, ratchet independence, and recall-status cleanup.

- Third audit checkpoint: async recall publication tagging/gating, stale-command cleanup, parameter-flush edge preservation, fragment-boundary interpolation, legacy slice-release composition, and overlap-safe state transitions.

- Deep audit: component setState/getState now use a fixed lock-free snapshot mailbox so UI-thread project save/recall does not race the realtime-owned ProjectState.

- Timing audit: phrase scheduling now uses musical 16th-step phase; CONTINUE prefers host PPQ/projectTimeMusic and RETRIGGER keeps a local musical phase, preventing historical sample-time reinterpretation across tempo changes.

- Fourth deep audit: held-note mode switching, source-recall save coherence, setup-allocation containment, and signed preroll/negative-PPQ scheduling.

- Fifth audit checkpoint: latest host-mode, source-recall coherence, preroll continuity, non-automatable edit actions, ABI exception containment, and scheduler runaway-rate guard validated together.

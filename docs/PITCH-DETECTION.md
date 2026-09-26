# Automatic Pitch Detection Baseline

## Purpose

Phraseator needs Pitch-To-Key to work without forcing the user to manually tag every sample.

The first implementation performs pitch analysis only in the non-realtime sample-loader thread.

## Method

The detector is an independently written implementation based on the published YIN fundamental-frequency estimation method:

- time-domain difference function;
- cumulative mean normalized difference function;
- thresholded local-minimum search;
- parabolic lag refinement.

No third-party source code is copied.

Evidence class for the algorithmic basis: **DOCUMENTED**.

## Product classification layer

The following thresholds are **EMPIRICALLY TUNED** product behavior:

- accepted F0 range: 55 Hz to 1760 Hz;
- YIN CMND threshold: 0.15;
- per-window confidence >= 0.88;
- stable windows must agree within 0.35 semitones;
- aggregate confidence >= 0.90;
- single-window detections require >= 0.94 confidence.

These values are intentionally conservative. A false negative leaves the source unpitched; a false positive would be musically more damaging.

## Stability policy

Longer sources are sampled at several positions.

A source is marked tonal only when a strong majority of valid windows agrees on approximately the same pitch. This intentionally rejects many:

- drum/noise sources;
- changing melodic phrases;
- polyphonic or harmonically ambiguous loops.

## Realtime

Pitch analysis:
- never runs in process();
- may allocate on the loader thread;
- does not block the audio callback;
- stores only the detected tonal flag/root note into the published source metadata.

## Future QA

Add fixed fixtures for:
- clean sine tones;
- bass/guitar single notes;
- pitched percussion;
- noisy percussion;
- chords;
- melodic loops;
- distorted material.

Measure:
- cents error;
- octave-error rate;
- false-positive tonal classification;
- false-negative rate;
- analysis time by source duration/sample rate.

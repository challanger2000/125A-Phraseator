# Internal Phrase FX Baseline

V0.1 currently implements two exposed finishing effects.

## Delay
- host-tempo synchronized;
- fixed 1/8-note division;
- ping-pong feedback;
- smoothed wet amount;
- smoothed delay-time changes on tempo changes;
- 0% produces no wet contribution.
- disabling Delay invalidates the previous delay history in O(1) using a generation marker; no multi-second buffer clear occurs in the realtime callback.

## Filter
- stereo low-pass finishing control;
- 0% is exact bypass;
- increasing amount lowers cutoff logarithmically toward approximately 800 Hz;
- smoothed parameter changes.

## Reverb / Drive
Parameter IDs remain reserved, but these controls are not currently exposed.

Reverb will receive a separately validated architecture. Nonlinear drive requires an explicit anti-alias / oversampling decision and measurement before it is presented as a functional control.

## Realtime
Delay memory is allocated in prepare() / setupProcessing(), never in process(). The process path performs no heap allocation, file I/O, logging or blocking synchronization.

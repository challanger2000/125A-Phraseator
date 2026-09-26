# Sample Load Worker

## Purpose

Sample loading is deliberately separated from the audio callback.

The worker:
- accepts non-realtime load requests;
- reads and decodes WAV files;
- optionally creates equal loop slices;
- clones the currently active sample bank into the inactive bank;
- replaces only the requested source slot;
- publishes the new bank atomically after full validation.

## Realtime rule

The audio thread never:
- opens files;
- allocates decoded sample memory;
- slices files;
- waits for the worker;
- locks the worker mutex.

It only calls SampleBankExchange::consumePending() at the start of a process block.

## Failure behavior

A failed file load or invalid slice request does not alter the active bank.

A load result is reported separately to the non-realtime caller.

## Current limitation

The worker currently supports:
- WAV input through SampleFileLoader;
- one-shot mode;
- equal-division loop slicing.

Transient slicing and automatic pitch detection remain separate future analysis stages.

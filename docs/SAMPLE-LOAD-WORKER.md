# Sample Load Worker

## Purpose

Sample loading is deliberately separated from the audio callback.

The worker:
- accepts non-realtime load requests;
- accepts batched load requests for project recall / multi-slot changes;
- reads and decodes WAV files;
- for new loop loads, prefers offline transient slices and falls back to the requested equal grid when no reliable onsets are found;
- clones the currently active sample bank into the inactive bank;
- replaces only the requested source slots;
- publishes the new bank atomically after the full request or batch validates.

## Why batches exist

A two-bank exchange can hold only one unpublished bank at a time.

Project recall may need to restore many source slots while the transport is stopped and the audio callback is not consuming pending banks.

A batch therefore prepares all requested slots in one inactive bank and performs one publication. No intermediate audio-thread consumption is required.

## Realtime rule

The audio thread never:
- opens files;
- allocates decoded sample memory;
- slices files;
- waits for the worker;
- locks the worker mutex.

It only calls SampleBankExchange::consumePending() at the start of a process block.

## Failure behavior

A failed file load or invalid slice request rejects the entire batch and does not alter the active bank.

A load result is reported separately to the non-realtime caller.

## Current behavior

The worker currently supports:
- WAV input through SampleFileLoader;
- one-shot mode;
- equal-division loop slicing;
- transient-preferred loop slicing with deterministic equal-grid fallback;
- conservative automatic pitch detection off the audio thread.

The resolved slice count is returned with the completed request so state/UI metadata can reflect the actual number of generated fragments.

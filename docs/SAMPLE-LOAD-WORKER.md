# Sample Load Worker

## Purpose

Sample loading is deliberately separated from the audio callback.

The worker:
- accepts non-realtime load requests;
- accepts batched load requests for project recall / multi-slot changes;
- reads and decodes WAV files;
- optionally creates equal loop slices;
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

## Current limitation

The worker currently supports:
- WAV input through SampleFileLoader;
- one-shot mode;
- equal-division loop slicing.

Transient slicing and automatic pitch detection remain separate future analysis stages.

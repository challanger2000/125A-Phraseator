# Realtime Sample Bank Handoff

Phraseator uses two owned sample banks.

## Non-realtime writer

A non-realtime loading path:

1. acquires the inactive bank;
2. decodes / prepares samples there;
3. validates SourcePool metadata and buffer views;
4. publishes the bank atomically.

Only one writer is allowed at a time.

## Audio thread

At the start of a process block the processor:

1. atomically consumes a pending bank index, if one exists;
2. switches the active bank index;
3. reads only immutable SourcePool metadata and AudioBufferView pointers from that bank.

The audio thread does not:
- allocate;
- free decoded sample vectors;
- lock a mutex;
- perform file I/O.

The previously active bank is not modified until the non-realtime side can acquire it as inactive after the swap.

This is the initial two-bank handoff baseline and must be stress-tested before release use.

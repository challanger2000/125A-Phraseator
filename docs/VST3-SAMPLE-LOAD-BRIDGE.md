# VST3 Sample Load / Clear Message Bridge

The VSTGUI controller and processor communicate source-bay actions through VST3 IMessage messages.

## Load message

Message ID:
- Phraseator.LoadSample

Attributes:
- Path
- SourceIndex
- SourceId
- Mode (0=one-shot, 1=loop)
- Divisions
- Tonal
- DetectedRootMidi

The controller owns the WAV file chooser. The processor validates the message and forwards the request to SampleLoadWorker.

The worker performs outside the audio callback:
- file I/O;
- WAV decoding;
- conservative pitch analysis when needed;
- transient-preferred loop slicing with equal-grid fallback;
- inactive-bank staging and atomic publication.

Resolved pitch metadata and exact slice boundaries are retained for deterministic project recall.

## Clear message

Message ID:
- Phraseator.ClearSample

Attribute:
- SourceIndex

Clear is also executed through the non-realtime worker/bank path. The active audio bank changes only after the clear operation has been staged and atomically published.

## Realtime rule

The audio callback never opens files or handles variable-length paths. It only consumes an already completed pending bank at block start.

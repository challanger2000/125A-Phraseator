# VST3 Sample Load Message Bridge

The controller and processor communicate sample-load requests through a VST3 IMessage.

Message ID:
- Phraseator.LoadSample

Attributes:
- Path
- SourceIndex
- SourceId
- Mode (0=one-shot, 1=equal-sliced loop)
- Divisions
- Tonal
- DetectedRootMidi

The processor does not decode the file inside notify().

notify() validates and forwards the request to SampleLoadWorker. The worker performs file I/O, WAV decoding, optional equal slicing and inactive-bank publication outside the audio callback.

The audio callback only consumes a completed pending bank at block start.

This is the infrastructure bridge for a future file chooser/browser. No GUI is implied by this layer.

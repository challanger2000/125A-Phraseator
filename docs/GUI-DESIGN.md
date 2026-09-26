# 125A Phraseator — Static Product Design Baseline

## Product identity

Phraseator should look like a compact creative instrument, not a laboratory tool and not a drum machine clone.

Primary visual priorities:

1. The user immediately understands where samples live.
2. GENERATE is visually dominant.
3. Musical macro controls are secondary but always visible.
4. Pattern state is readable at a glance.
5. Finishing FX are available without visually competing with generation.
6. No deep-page navigation is required for the V0.1 workflow.

## Window

Initial fixed design target:

- content size: 1040 x 640 logical px
- dark technical base panel
- clear module separation
- restrained hardware influence rather than photorealistic rack imitation
- scalable implementation later through VSTGUI zoom; layout ratios should remain stable

The authoritative 125A logo geometry must come from:

`challanger2000/125A-Branding/125A_Logo_Master_FINAL.svg`

No logo redraw or font reconstruction is permitted.

## Layout

### Header — 64 px

Left:
- 125A master logo

Center:
- PHRASEATOR
- short descriptor: GENERATIVE SAMPLE PHRASE ENGINE

Right:
- project/version area
- future settings/help access

### Source bay — left column, 300 px wide

Purpose:
show the user's source material without becoming a browser.

Visible elements:
- 8 source slots initially visible
- two banks/pages for the 16-source technical capacity
- each slot shows:
  - source number
  - short file name
  - ONE SHOT / LOOP
  - slice count when applicable
  - tonal marker only when relevant
  - load/replace action
  - lock state

The source bay is not a library index. Loading remains explicit and user-driven.

### Pattern area — center/top

A 16-step horizontal phrase strip.

Each step communicates:
- active/rest
- currently selected fragment/source identity
- repeat/ratchet indication
- lock state if step-level locking is later exposed

The first implementation may use simple drawn cells rather than rendered assets.
The pattern strip must not become a full piano-roll or deep sequencer editor in V0.1.

### Generate area — center

Dominant primary action:

- GENERATE — largest action control in the interface
- VARIATE — clearly secondary
- LOCK — persistent pattern lock

GENERATE should remain visually obvious even when the user is not reading labels.

### Macro controls — center/bottom

Six primary musical controls:

- DENSITY
- VARIATION
- REPEAT
- PITCH
- PAN
- GROOVE

Control hierarchy:
- equal size
- concise labels
- value readout
- actual defined default visible through control state
- 0% follows the 125A neutral/off rule where applicable
- default-reset gesture must map to the actual parameter default

Do not add technical sub-controls unless a proven musical need appears.

### Tonal section — right/top

Compact controls:
- PITCH TO KEY
- ROOT
- SCALE

This module should visually recede when PITCH TO KEY is off.

### Finish section — right/bottom

V0.1 exposed finishing tools:
- DELAY
- FILTER

Reserved but not exposed until technically validated:
- REVERB
- DRIVE

The UI must not display controls that do not have validated functional DSP behind them.

## Interaction model

Primary workflow:

1. Load arbitrary samples.
2. Press GENERATE.
3. Adjust macro controls.
4. Press VARIATE when useful.
5. LOCK a good phrase.
6. Optionally add DELAY / FILTER.

The UI must support this without opening additional pages.

## Visual language

- dark base materials
- slightly lighter inset modules
- subtle depth/shadows, not exaggerated 3D
- restrained blue/cool accent family is acceptable for module framing
- red reserved for the established 125A logo stroke and critical states
- active steps/locks must remain distinguishable without relying on red alone
- labels should prioritize legibility over decoration

## Controls and assets

Until a Phraseator-specific approved package exists in 125A-Knob-Designer:

- do not reuse MixEngine rendered controls merely because they exist;
- use neutral VSTGUI-drawn placeholders only during functional editor development;
- replace placeholders only with an explicitly approved Phraseator asset package;
- no placeholder asset may become an accidental final design.

## V0.1 non-goals

- deep sample browser
- waveform editor occupying major screen area
- multi-page modulation matrix
- full piano-roll
- multi-output mixer
- decorative meters without direct musical value
- exposed Reverb/Drive before DSP validation

## Static layout coordinates

Logical coordinate baseline:

- Header: x 0–1040, y 0–64
- Source Bay: x 20–300, y 84–620
- Pattern: x 320–1018, y 84–180
- Generate: x 320–1018, y 196–292
- Macros: x 320–790, y 312–500
- Tonal: x 810–1018, y 312–402
- Finish: x 810–1018, y 420–560
- Footer/status: x 320–1018, y 578–620

This layout is the baseline for the first VSTGUI editor implementation.

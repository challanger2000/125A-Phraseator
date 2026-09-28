# Current Release - 125A Phraseator

## Authoritative released product

- Product: **125A Phraseator**
- Version: **1.0.0**
- Platform: **Windows x64**
- Format: **VST3 Instrument / Sampler**
- Shipping source branch: **release-v1.0.0**
- Shipping source commit: **b01e7e9f58bcaf45572b311754711c2fa2ba6c6e**
- Development branch at release: **v1.0.0**
- VST3 SDK: **3.8.1_build_84**
- Serialized state version: **9**

## Verified build

GitHub Actions run **#431** built the shipping commit above.

- Core regression suite: **16/16 PASS**
- Steinberg VST3 Validator: **47/47 PASS, 0 failed**
- CI artifact: **125A-Phraseator-v1.0.0-Windows-x64**
- CI artifact SHA-256: `3ddc6fd0fd9556963c0be063409622d4148f6a8bb300b1ab5bf25d7a61b57e76`

The generated VST3 `moduleinfo.json` was checked after the build:
- package version: **1.0.0**
- processor class version: **1.0.0**
- controller class version: **1.0.0**

## Gumroad package

- File: **125A_Phraseator_v1.0.0_Windows_x64_Gumroad.zip**
- SHA-256: `4f3d5a42edf5e75ba571001b350292c81bd2b0c6747009dd20b26da1088da3bb`

Package contents:
- `125A-Phraseator.vst3`
- German PDF manual
- English PDF manual
- bilingual README
- 125A single-user EULA
- third-party notices
- build information
- SHA-256 component manifest

## Host verification

The Studio One acceptance pass immediately before release covered:
- WAV drag & drop and LOAD/CLEAR
- manual step source cycling
- per-step 1x-4x ratchets
- ratchet right-click isolation from the host Macro Controller menu
- Root / Scale / Delay Division popup selectors
- Octave cycling
- Generate / Variate / Pattern Lock / Phrase Mode
- stronger LP range
- Groove behavior
- native stereo preservation at PAN 0

The final follow-up changes after that acceptance pass were limited to:
1. enforcing the 125A Ctrl+left-click default-reset rule on MacroKnob controls;
2. correcting VST3 factory version metadata from the stale development value 0.1.0 to 1.0.0.

No DSP, parameter ID, state-layout or sample-recall semantic change was made after the accepted stereo/LP/Groove build.

## Historical branches

- `v0.1.0` is development history only.
- `v1.0.0` is the completed V1 development line.
- `release-v1.0.0` is the explicit shipping-source pointer for the Gumroad V1.0.0 package.

Do not infer the current release from old ZIP names, workflow artifacts or the historical `v0.1.0` branch. Start here, then verify the shipping branch/commit and package hash.

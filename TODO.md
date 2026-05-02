# Boceto — TODO

## Open

- [ ] **Voice recording** — record from device microphone to a clip, load into the active pad.
  - .jucer: flip `MICROPHONE_PERMISSION_ENABLED` to `TRUE` on the Android exporter (and iOS later)
  - AndroidManifest: ensure `<uses-permission android:name="android.permission.RECORD_AUDIO"/>` is present after Projucer save
  - Runtime permission flow: `juce::RuntimePermissions::request(recordAudio, callback)` before opening the input device
  - Open input via `deviceManager.initialiseWithDefaultDevices(2, 2)` (currently `0, 2`)
  - New `Recorder` class: `juce::AudioIODeviceCallback` that writes incoming samples to a `juce::WavAudioFormat` writer at device sample rate
  - REC button in the calculator grid (probably replace EXPORT or share a row); status bar shows elapsed seconds while recording
  - On stop: load the recorded WAV into the active voice via the existing `loadFile` path

## Phased song mode (in progress)

- [x] Phase A — polyphonic SamplerEngine refactor (6 voices mixed)
- [ ] Phase B — per-pad source clip (each pad holds its own audio)
- [ ] Phase C — sequencer grid UI

## Future / parked

- [ ] CROP function — replace source buffer with current loop slice; clear pads with warning
- [ ] Auto-save live nudges to last-triggered pad (currently nudges are temp until SAVE)
- [ ] Effects chain (`juce::dsp::ProcessorChain`) — at minimum a low/high-pass filter, maybe per-voice
- [x] iOS exporter — `NSAppTransportSecurity` `NSAllowsArbitraryLoads` + `NSLocalNetworkUsageDescription` wired via `PLIST_TO_MERGE` in `app/CMakeLists.txt`

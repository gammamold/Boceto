# YouTube Sampler

Personal-use YouTube → varispeed sampler. JUCE app + FastAPI server.

## Layout
- `server/` — FastAPI + yt-dlp endpoint (runs on iMac / Pi / VPS)
- `app/` — JUCE C++ app, builds for **macOS, iOS (iPad), Android**

## How it fits together

```
┌────────────┐  POST /fetch  ┌──────────────┐  yt-dlp + ffmpeg
│ JUCE app   │ ─────────────▶│ FastAPI      │ ─────────▶ youtube
│ (iPad/etc) │ ◀───── WAV ───│ server       │
└────────────┘               └──────────────┘
      │
      ▼ load WAV into SamplerEngine, varispeed loop playback
```

The app never talks to YouTube directly. Only the server does. When YouTube
breaks the extractor (every couple of months) you `pip install -U yt-dlp` on
the server and the app keeps working.

---

## 1. Run the server

```bash
cd server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
# install ffmpeg if you don't have it: brew install ffmpeg
uvicorn main:app --host 0.0.0.0 --port 8000
```

Test:
```bash
curl -X POST http://localhost:8000/fetch \
  -H "Content-Type: application/json" \
  -d '{"url":"https://youtube.com/watch?v=...","sample_rate":44100}' \
  --output test.wav
```

For the iPad to reach it, server and iPad must be on the same LAN, or expose
the server via Tailscale / a tunnel.

---

## 2. Build the app

JUCE 8 is fetched automatically by CMake.

### Desktop (macOS — develop here on the iMac)
```bash
cd app
cmake -B build -G Xcode
cmake --build build --config Debug
open build/YouTubeSampler_artefacts/Debug/YouTube\ Sampler.app
```

Or use Ninja/Make if you prefer the CLI. This is where you'll do 95% of dev
since you have no iPhone — the macOS build mirrors iPad behaviour closely.

### iOS / iPadOS
```bash
cd app
cmake -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=YOUR_TEAM_ID
open build-ios/YouTubeSampler.xcodeproj
```

In Xcode: select your iPad as target, Signing & Capabilities → set team,
Run. With a free Apple ID the provisioning lasts 7 days. With a paid
Developer account, 1 year. For zero-friction reinstalls, look at AltStore
PAL (officially sanctioned in the EU under the DMA).

Background audio + microphone permissions are off by default — flip
`BACKGROUND_AUDIO_ENABLED` / `MICROPHONE_PERMISSION_ENABLED` in
`CMakeLists.txt` if needed.

### Android
JUCE Android needs the Projucer route or an Android Studio Gradle wrapper —
CMake-only Android JUCE works but is fiddly. Cleanest path:

1. Open `app/CMakeLists.txt` once on desktop to confirm sources compile.
2. Install **Projucer** (built from JUCE/extras/Projucer or downloaded).
3. New project → Audio Application → import the `Source/` files.
4. Add Android exporter, set NDK + SDK paths, set min API 24.
5. Hit Save → opens Android Studio → Build → APK → install via ADB.

The same `Source/*.cpp` compiles unchanged. JUCE handles the JNI plumbing
and Oboe audio backend automatically.

---

## What works in the scaffold

- Fetch any YouTube URL via the server, returns WAV
- Load WAV into the sampler buffer
- Play / stop, looping between two normalised positions
- Varispeed slider 0.25× – 4× (tape-style: pitch and speed linked)
- Linear interpolation, stereo out, mono auto-duped

## What to add next

- **Independent pitch/time-stretch**: link Rubber Band Library, replace the
  inner read loop in `SamplerEngine::getNextAudioBlock`. Add a second slider
  for pitch in semitones.
- **Waveform display**: `juce::AudioThumbnail` + a custom Component, drag
  loop markers on it.
- **Multi-pad / chop mode**: array of `SamplerEngine` instances, MIDI
  triggers.
- **Persistence**: save loop points + speed alongside the cached WAV so a
  re-fetched clip keeps your edits.
- **AUv3 wrapper** (iPad): host inside AUM / Drambo for routing into other
  apps.

## Notes

- yt-dlp ToS: this is for **personal use**. Don't ship it.
- The server caches WAVs under `/tmp/yt_sampler_cache` keyed by URL+SR. Set
  `CACHE_DIR` env var to keep them across reboots.
- Audio behaviour in the iOS Simulator is not representative. Test on the
  iPad.

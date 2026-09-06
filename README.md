# DrawEQ — Drawable EQ

You draw the frequency response you want, and the plugin works out how to be it.

Two ways of being it, both shipping:

- **Spectral** — the drawn curve becomes an FFT-domain filter directly. Exact,
  arbitrarily sharp, and costs either latency (linear phase) or phase rotation
  (minimum phase).
- **Analog** — a background optimiser fits a cascade of biquads to the stroke.
  Zero latency, musical phase, and the fit hands back *labelled, draggable,
  automatable bands*. You drew a gesture and got an editable band stack.

The interface shows both what you asked for and what you got: a graphite ghost
where your hand went, a warm plotter line for the actual response, and a shaded
ribbon between them that vanishes when the fit is exact and blooms at precisely
the frequency where it is not.

The full specification is [CONTEXT.md](CONTEXT.md). Where the implementation
departs from it, and why, is [DECISIONS.md](DECISIONS.md).

---

## Installing on Windows

Grab `DrawEQ.msi` from the [latest
release](https://github.com/whysush/DrawEQ/releases) and run it. It installs
the plugin to `C:\Program Files\Common Files\VST3`, creating that folder if
your machine does not already have it, and offers the standalone app as an
optional extra.

Windows will show a blue "Windows protected your PC" box first, because the
installer is not signed by a certificate authority — see
[Code signing](#code-signing) for why. Click **More info**, then **Run
anyway**.

Then point your host at it:

- **FL Studio** — Options → Manage plugins → set the VST3 search path to
  `C:\Program Files\Common Files\VST3` if it is not there already, then
  **Find more plugins**. DrawEQ appears under Effects.
- **Ableton, Reaper, Bitwig, Studio One** — rescan plugins; the Common Files
  path is scanned by default.

Prefer to place the folder yourself? The release also carries
`DrawEQ-Windows-VST3.zip` — unzip it and drop `DrawEQ.vst3` into any folder
your host scans.

### Code signing

Release binaries are unsigned, so Windows SmartScreen warns on first run. That
is a statement about paperwork rather than about the code: a signature that
suppresses the warning requires a code-signing certificate issued by a
certificate authority against a verified legal identity, which costs money
annually and cannot be self-issued. A self-signed certificate does not help —
it satisfies nobody's trust store but your own.

The build is ready for one whenever there is one. CI signs the plugin, the
standalone and the installer, with timestamps, if `WINDOWS_CERT_BASE64` and
`WINDOWS_CERT_PASSWORD` are set in the repository secrets, and skips signing
entirely when they are absent.

---

## Building

Needs CMake 3.22+ and a C++20 compiler. JUCE 8.0.9, Eigen 3.4 and Catch2 v3 are
fetched and pinned automatically.

```bash
./Tools/fetch_fonts.sh          # once: the two bundled SIL OFL typefaces
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

On Linux you also need the usual JUCE packages:

```bash
sudo apt-get install libasound2-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxinerama-dev libxcomposite-dev libfreetype6-dev \
  libfontconfig1-dev libgl1-mesa-dev
```

Outputs land in `build/DrawEQ_artefacts/` — a VST3 and a standalone.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

The tests are headless: Core and DSP link only `juce_dsp`, so no display server
is involved and they run anywhere. What they cover:

| File | What it protects |
|---|---|
| `TestLogGrid` | the one frequency↔index mapping; an octave being the same width everywhere |
| `TestCurveModel` | brushes, gesture-level undo, state round-trip, corrupt-state rejection |
| `TestIRBuilder` | flat curve → exact unit impulse; built IR matches its target within 0.1 dB; deep notches stay finite |
| `TestConvolution` | UPOLS against direct convolution at every block size; latency; unit-impulse null |
| `TestBiquadCascade` | that the closed form **is** the running filter, and the analytic Jacobian is right |
| `TestCurveFitter` | a regression corpus with recorded error bounds, plus both timing budgets |
| `TestBandMatcher` | 500 frames of a morphing curve with no band moving more than one position |
| `TestStateRing` | the publish/consume/recycle protocol, including concurrently |
| `TestNull` | flat curve nulls against dry audio in all three modes |
| `TestSweeps` | every sample rate × block size, prepare/release cycles, automation thrash |
| `TestRealtimeSafety` | a global allocation trap proving the audio path never allocates |

Validation, as CONTEXT.md 11 requires:

```bash
pluginval --strictness-level 10 --validate build/.../DrawEQ.vst3
```

## Tools

Neither is shipped; both exist to answer questions the tests cannot.

```bash
# Loads the built VST3 through JUCE's own VST3 host and drives it the way a
# DAW does: odd buffer lengths, the sample rate changing under it, a latency
# change mid-playback, state across a save/reload, four instances at once,
# bypass, and a window opened and closed repeatedly.
./build/DrawEQHostSim_artefacts/*/DrawEQHostSim \
    build/DrawEQ_artefacts/*/VST3/DrawEQ.vst3

# Fit quality and timing across a corpus - "did that change actually help?"
./build/DrawEQFitBench_artefacts/*/DrawEQFitBench 24

# Render the editor to a PNG with no window and no audio device.
# modes: demo | ribbon | flat
./build/DrawEQSnapshot_artefacts/*/DrawEQSnapshot out.png ribbon
```

## Using it

| | |
|---|---|
| `P` `L` `S` `E` `N` | pencil, line, smooth, erase, node |
| wheel | brush radius, in octaves — so it feels the same at 60 Hz and 6 kHz |
| `Shift` | lock to constant dB |
| `Alt` | smooth, temporarily |
| `Ctrl`/`Cmd` | fine adjust |
| right-drag | erase toward flat |
| double-click | flatten one octave |
| `Ctrl+Z` | undo, one whole stroke at a time |

The **Node** tool is the one to understand. Drawing produces bands; dragging a
band rewrites the curve; drawing again re-fits. Gesture and precision, with no
mode switch and no conversion step.

Slots: click selects the slot you are drawing into, shift-click stores,
alt-click clears, right-click sets the morph target. `morph` blends from what
you drew toward that target, and it is continuous at zero — which is what makes
it the automatable stand-in for a drawing.

## Layout

```
Source/
  Core/     CurveModel, LogGrid, CurveShaping, Params, PresetBank
  DSP/      IRBuilder, Cepstrum, ConvolutionEngine, BiquadCascade,
            CurveFitter, BandMatcher, Analyzer, StateRing, CurveWorker
  UI/       Theme, CurveCanvas, CanvasLayers/, Tools/, Controls/, Editor
```

Three threads, one direction of data flow: the message thread owns the curve,
the worker turns it into a `FilterState`, and the audio thread crossfades to it
and hands the old one back. The audio thread never allocates, never locks and
never destroys — `TestRealtimeSafety` asserts it rather than trusting it.

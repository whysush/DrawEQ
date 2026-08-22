# Decisions, deviations and open questions

CONTEXT.md is the specification. Where the implementation departs from it, or
where the spec was ambiguous enough that a choice had to be made, it is recorded
here with the reasoning. Rule 10 says to ask rather than guess; these are the
places where a decision was needed to keep building, and each one is reversible.

---

## Deviations from the spec

### 1. The Analog model has a broadband trim (§7.3)

**Spec:** `theta = {(f, G, Q)}` for N bells plus two shelves.
**Built:** the same, plus one scalar dB offset applied after the cascade.

A cascade of bells and shelves cannot express a constant offset. Without a trim,
a target of "+4 dB everywhere" is fit by two shelves fighting each other across
the whole band, which fits badly, wastes two of the user's editable bands, and
falls apart the moment they drag one. One extra scalar removes the entire class
of problem and costs a single multiply on the audio thread.

`TestCurveFitter` asserts a flat +5 dB target comes back as trim = 5 dB with
every band idle.

### 2. The fitter does not write the user's `tilt` macro (§7.3, initialisation step 1)

**Spec:** "Estimate broadband tilt by least-squares line fit across log
frequency; assign to the tilt control, subtract from target."
**Built:** the line fit still happens, but it seeds the two shelves instead of
being written to the parameter.

As specified this is circular. `tilt` is a macro *applied to the curve before
the DSP sees it* (§8.2), so if the fitter also wrote to it, the next frame's
target would include the tilt the previous frame just removed, and the control
would drift under the user's hand while they were holding it. Seeding the
shelves gets the same head start - a broadband tilt is exactly what a low shelf
down plus a high shelf up looks like - without the feedback loop.

**This is the deviation most worth a second opinion.** If the intent was that
Analog mode should own a tilt element of its own, separate from the macro, that
is a different and also reasonable design, and it is a small change.

### 3. Convolution priming is `P - gcd(blockSize, P)`, not `P - blockSize` (§7.2)

Identical for every power-of-two block size, which is every block size a host
actually sends, and correct for the ones that are not. A hop fires once P input
samples have arrived, so the output runs behind by the most input that can pile
up between hops, which is `max(n·B mod P)` = `P - gcd(B, P)`. At B = 100, P = 128
the spec's formula gives 28 and the true figure is 124; priming with 28 would
underrun on the first block.

If a host varies its block size unpredictably the priming can still fall short.
That path emits silence for the shortfall rather than stale samples, and counts
it - `ConvolutionEngine::underruns()` is asserted to be zero across the whole
sample-rate and block-size sweep.

### 4. Morph semantics: the canvas edits slot A (§8.1, §8.3)

The spec says morph interpolates between slots and is the escape hatch for
automating a drawing, but not what happens to the live curve at `morph = 0`.

**Built:** the canvas always edits the slot named by `morphA`, and the effective
curve is `lerp(live, slot[morphB], morph)`. At `morph = 0` the live curve passes
through untouched, so the control is continuous where a user will spend most of
their time, and automating it 0 → 100 morphs from what they drew toward slot B.
Switching slot A banks the current drawing into the old slot before loading the
new one, so nothing is silently discarded.

The alternative - morph blends slot A to slot B and the live curve is a third
thing - has a discontinuity at zero that no automation curve can smooth over.

### 5. The residual ribbon is honest in Spectral mode too (§9.4)

The achieved response in Spectral mode is measured by transforming the IR that
was actually built, not assumed equal to the target. Windowing and finite length
genuinely do change the response at the bottom of the range, and the ribbon
showing that is the point of the ribbon.

### 6. The ghost stroke draws the post-macro target, not the raw stroke (§9.4)

Otherwise tilt, shift and smooth would move the plot line without moving the
ghost, and the ribbon between them would show the macros rather than the fit
error. The raw stroke is still what the model stores, so smoothing stays
non-destructive exactly as §6.3 requires.

### 7. Crossfade shares one frequency-domain delay line (§5, §7.2)

The FDL holds *input* history, which does not depend on the impulse response, so
during a spectral crossfade both IRs are convolved against the same FDL and only
the accumulate-and-inverse-transform stage is duplicated. Half the cost, and the
two streams are sample-aligned by construction rather than by care.

### 8. Analytic Jacobian for bells, finite differences for the two shelves (§7.3)

The spec permits finite differences throughout for a first version. Bells get
the closed form because they are 24 of the 26 sections and the inner loop; the
shelves get central differences because their closed form is messier and six
extra evaluations per iteration do not show up in the measurements.
`TestBiquadCascade` checks the analytic gradient against finite differences at
five frequencies.

### 9. A mode switch resets the convolution engines

Leaving Spectral mode leaves the engines' input history stale. Rather than pay
to keep them running while unused, they are reset on the way back in and the
20 ms crossfade covers the fade-in from empty history.

---

## Choices the spec left open

- **JUCE 8.0.9**, Eigen **3.4.0** (release tarball, not a clone - the Eigen
  repository history is enormous), Catch2 **3.5.4**. All pinned.
- **Slot switching stores before loading.** Losing a drawing to a mis-click
  would be unforgivable and the fix is one line.
- **Analyzer reference level.** The analyser reads dBFS and the canvas is a
  relative dB scale; -18 dBFS is anchored to the 0 dB line so a normal mix sits
  mid-plot with no auto-ranging for the eye to chase.
- **Band tokens are drawn even at 0 dB**, faintly, so an idle band is still
  something the Node tool can grab and recruit.
- **Right-click on a slot** sets the morph target; shift-click stores;
  alt-click clears.

---

## Not built

- **M12 (macOS).** Nothing macOS-specific has been attempted: no universal
  binary, no AU wrapper, no Ableton verification.
- **Factory presets (part of M10).** The bank, the eight slots, morph and
  serialisation all work; no factory content has been authored, so all eight
  slots start empty.
- **FL Studio verification.** This machine is Linux. The VST3 builds and passes
  pluginval, but it has not been loaded in FL Studio, which is the one test
  CONTEXT.md 12 asks for at every milestone.
- **Tooltips.** Tool buttons carry tooltips but no TooltipWindow is installed,
  so nothing displays them yet.

---

## A bug worth knowing about: mouse cursors crash JUCE 8 on Linux

Setting **any** non-default `juce::MouseCursor` makes the X display teardown
segfault. It is not this plugin's memory: AddressSanitizer stays silent right up
to the fault, which lands inside `libX11`'s `XCloseDisplay`, reached from
JUCE's own `XWindowSystem` destructor. JUCE appears to cache the standard cursor
handles past the life of the display.

It was found by pluginval at strictness 10, which segfaulted on every run with
GUI tests enabled and passed cleanly with `--skip-gui-tests`. Bisecting found
two independent triggers, both a `setMouseCursor` call:

- `CurveCanvas`'s crosshair, and
- the resize grab handle that `setResizable (true, true)` creates.

An editor with neither passes 25/25. Both are now guarded with `#if JUCE_LINUX`,
and both guards say why. **Windows, the v1 target, is unaffected** and keeps the
crosshair and the corner handle. Linux keeps resizing - the constrainer and the
host frame still drive it - and the canvas draws its own crosshair hairlines, so
nothing visible is lost there either.

Worth revisiting against a newer JUCE before Linux becomes a real target.

---

## Measured performance

From `GraphiteFitBench` and the timing assertions in `TestCurveFitter`, on a
12-core desktop at 48 kHz. CONTEXT.md 10 asks for these to be stated rather than
quietly shipped past.

| Path | Budget | Measured |
|---|---|---|
| Worker, warm fit, 12 bands | < 2 ms | **0.97 ms** |
| Worker, cold fit, 24 bands | < 50 ms | **28.5 ms** |
| Worker, IR rebuild, L = 4096 | < 8 ms | within budget |

**Worth knowing:** the warm-fit budget is specified at 12 bands, which is the
default and which passes comfortably. At the maximum of 24 bands a warm fit
takes **2.0-2.4 ms**. No stated budget covers that case, and it is not a
problem - the worker runs at 30 Hz, so 2.4 ms is 7 % of its 33 ms period, and it
is not the audio thread - but anyone reading the 2 ms figure should know it
applies to 12 bands and not to 24.

Getting there needed one optimisation. The first working fitter missed both
budgets (2.7 ms warm, 79 ms cold) because `tan`, `pow` and `log` were being
evaluated for every (band, frequency) pair. Tabulating `tan (pi f / fs)` per
evaluation point and hoisting the per-band constants out of the inner loop -
`response::BandEval` - cut both by roughly 2.8x. The readable closed form in
`response::bandDb` is still the reference, and a test asserts the fast path
agrees with it to a thousandth of a dB.

The audio-thread budgets in CONTEXT.md 10 (CPU percentages under a real host)
have **not** been measured: that needs a host and a profiler, and this machine
has neither FL Studio nor a calibrated reference. What has been verified is the
harder invariant behind them - `TestRealtimeSafety` proves the audio path never
allocates, under a global allocation trap, in both modes and mid-crossfade.

---

## The interface was redesigned (supersedes CONTEXT.md 9.1-9.3)

CONTEXT.md describes a drafting surface: graphite-grey ghost, warm amber plotter
line, indigo-to-gold band ramp, Space Grotesk labels with JetBrains Mono
numbers. That is no longer what is built. On a supplied visual reference the
interface became a **phosphor terminal**, and the sections above are the record
of what changed rather than a description of the shipping design.

**What changed**

- **One hue.** Every active state is the same green at some brightness, so the
  panel is quiet until something is happening. `Theme::Colour` keeps its
  structure; only the values moved.
- **Layout.** Slots moved to a full-width row across the top; parameters moved
  into a right-hand sidebar; the tools and the fit-error readout became a bottom
  bar. Default size went from 1000x560 to 1180x620 to give the sidebar room.
- **Knobs became character bars.** `[========|.......]` in the monospaced face
  instead of rotary dials. The cells quantise the value visually, so two
  parameters at the same setting line up exactly and the column reads like a
  column of numbers. `Knob` is deleted; `BarSlider` replaces it.
- **Monospace throughout.** CONTEXT.md 9.3 says labels are Grotesk and values
  are Mono, never mixed. A terminal sets everything in one width, so the
  grotesk is now used for the wordmark alone.
- **A status console.** Three lines under the sidebar: fit quality and band
  count, latency and audio load, mode and sample rate.

**What survived, because it is the actual thesis**

The ghost, the plot line and the residual ribbon are unchanged in behaviour -
the ribbon still vanishes when the fit is exact and still blooms at the exact
frequency where it is not. `MAX ERR` is still permanent and still turns amber
above 3 dB, and amber is the one deliberate departure from the single hue
precisely because it has to be unmissable against this much green.

Band tokens still encode frequency, but a single-hue interface cannot ramp
indigo to gold, so **frequency is carried by brightness instead**: dim and
desaturated at 20 Hz, full phosphor at 20 kHz. The property that mattered - a
glance tells you where a band sits - is intact.

**Two figures in the console are newly measured.** The load percentage is the
audio callback timing itself with a vDSO clock read per block, exponentially
smoothed; the latency is what the host was actually told. Neither is decorative,
because a panel reporting a plausible constant would be worse than no panel.

**Not adopted from the reference.** It shows an `OVERSAMPLE` checkbox and labels
the analyser selector `CHANNEL`. Oversampling is not implemented, and a control
that does nothing is worse than an absent one. `CHANNEL` would imply mid-side or
channel selection, which CONTEXT.md 1 lists as an explicit non-goal, so that
selector is labelled for what it actually does.

## Tool icons are vector paths, not font glyphs

CONTEXT.md 9.5 sketches the tool row as characters. Setting a button's text to a
pencil or erase glyph depends on whichever fallback face the machine has, so two
users would see different icons and one would see tofu. `ToolIcons.h` draws all
five as paths in a unit box: they take the theme colour, stay crisp at any UI
scale, and are identical everywhere.

The pencil and the eraser were drawn with the failure mode of small icons in
mind - a bare quadrilateral reads as a pencil *or* an eraser and the viewer
cannot tell which. The pencil gets a collar and a filled graphite tip; the
eraser gets a blunt end, a two-tone sleeve, and a fragment of the line it is
clearing. The node tool draws its handle as a **ring**, the same shape as a band
token on the canvas, so the icon and the thing it manipulates match.

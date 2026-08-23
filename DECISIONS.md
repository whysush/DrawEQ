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

From `DrawEQFitBench` and the timing assertions in `TestCurveFitter`, on a
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

---

## The brush was rewritten to track the cursor

CONTEXT.md 6.2 describes drawing as a raised-cosine dab per incoming point, with
interpolation between mouse positions. Implemented literally, that lags: each
dab pulls its neighbours toward *its own* target, so a bin painted early gets
dragged most of the way to whatever the stroke does next, and a steep gesture
comes out flattened and trailing the cursor.

Measured on a 16 dB ramp drawn over 40 mouse positions, the worst deviation
between where the cursor was and what the curve read back was **0.63 dB**.

The brush is now a single swept pass per segment rather than a run of
overlapping dabs. Each affected bin projects onto the segment, and a bin the
stroke passes directly over is written to the stroke's value at that exact
frequency, once. Two further rules were needed, and each was found by measuring
rather than by reasoning:

- **Ground the gesture has already covered is never re-feathered.** Without
  this the trailing half of every brush drags finished bins toward wherever the
  cursor has since moved. (0.63 -> 0.47 dB.)
- **The skirt past each end aims at the value the stroke actually has on that
  side** - the segment's own endpoint where the stroke is advancing, the value
  recorded at the gesture's extreme where it is not. Getting this wrong in
  either direction moves the error to the other end of the stroke: aiming the
  trailing skirt at the current segment drags the stroke's origin away with
  every mouse move, and aiming the leading skirt at the recorded extreme leaves
  the newest ground one segment stale. (0.47 -> 0.26 -> **0.0077 dB**.)

The residual is interpolation into the feather skirt at the two ends, not lag.
`TestCurveModel` locks it at 0.02 dB.

A visible consequence: fit errors went *up* slightly on the same gesture,
because the fitter is now being asked to match what was actually drawn instead
of a flattened version of it. That is the correct direction.

---

## Drawing is committed, not tracked (supersedes CONTEXT.md 5's cadence)

CONTEXT.md 5 has the worker re-fitting continuously while the user drags, at up
to thirty states a second, dropping intermediate strokes. That is now the
optional behaviour rather than the default.

By default the filter is **left alone until the stroke ends**, and is then built
once with a much larger search. The reason is not to save CPU - it is accuracy.
A fit that has to finish inside a frame gets five Levenberg-Marquardt iterations
from the previous solution. A fit that runs once per stroke can afford several
seeds, a hundred and fifty iterations each, and perturbed restarts. LM only ever
walks downhill, so on a curve with more structure than bands it settles into
whichever basin the seed landed in; shaking the best solution and re-converging
is what finds the better one, and it is only affordable once per gesture.

What that buys, at 12 bands:

| target | live fit | committed |
|---|---|---|
| wide scoop | 4.031 dB | **0.934 dB** |
| brick-wall scoop | 0.548 dB | **0.255 dB** |
| the demo stroke | 0.49 dB | **0.05 dB** |
| narrow notch | 9.387 dB | 8.828 dB (still unfittable, correctly) |

Cost is 25-80 ms on the worker thread, once, while the audio thread keeps
running the previous filter. Nothing is dropped and nothing clicks.

**What the UI had to do differently.** The ghost is now computed on the message
thread every frame straight from the model, through the same macro chain the
worker uses, because the worker's snapshot no longer updates during a stroke and
the user would otherwise be drawing blind. While a commit is outstanding the
ghost brightens and thickens, the residual ribbon shows the gap between what has
been drawn and what is still playing, and the console says `> drawing - release
to fit`. When the stroke lands, the ribbon collapses.

**Commits are counted, not observed.** The worker watches a monotonic count of
finished gestures rather than only the in-progress flag. A flick that begins and
ends between two worker ticks would never be seen open, and would quietly get a
cheap fit instead of the committed one - which is exactly the case that made the
demo render sit at 0.49 dB until it was fixed.

**The same curve always realises the same way.** Undo, redo, a preset recall and
a hand-edited band all route to the committed-quality fit too, so a curve does
not depend on how the user arrived at it.

**On "1:1".** Spectral mode genuinely is: the drawn curve becomes the filter
directly, and the null test holds it to -132 dB. Analog cannot be, for any
finite band count - that is CONTEXT.md 7.5 and it is why Spectral ships
alongside. What the committed fit does is get close enough that the distinction
stops mattering for musical curves, and `MAX ERR` still says so honestly when it
does not.

`liveFit` restores continuous re-fitting for anyone who wants the curve to
follow their hand.

---

## The ghost draws the stroke, not the macro target

A bug report: "the drawing offset is higher than where my cursor is."

It was real, and the cause was a decision recorded further up this file. The
ghost line was being drawn from the *post-macro* target rather than from the raw
stroke, so smoothing sat between the user's hand and the line on screen.
Smoothing pulls extremes toward their neighbourhood, which means it always
lifts a cut - hence "higher", every time, never lower.

Measured, drawing a dip at -14 dB with a half-octave brush:

| smooth | drawn | displayed |
|---|---|---|
| 0 % | -14.00 dB | -14.00 dB |
| 15 % (the old default) | -14.00 dB | **-12.78 dB** |
| 40 % | -14.00 dB | -8.98 dB |

The brush itself was exact the whole time - the raw curve read back -14.00 dB.
Only the line was wrong.

**Three changes.**

- The ghost is now the raw stroke, which is what CONTEXT.md 9.4 says it is and
  what an earlier entry here traded away for a tidier ribbon. That trade was
  wrong: a drawing tool whose line does not land under the cursor is broken,
  however good the reason.
- The post-macro target gets its own faint line, drawn only when the macros
  actually moved something, so the difference is visible and attributable
  instead of silently folded into the ghost. The ribbon still spans target to
  achieved, so `MAX ERR` keeps meaning fit error and nothing else.
- **`smooth` now defaults to 0 %**, against CONTEXT.md 8.2's 15 %. That default
  made sense when the fit had one frame to work in and needed the target
  softened; the committed fit does not need the help, and blurring the stroke by
  a fifth of an octave before the DSP sees it works against every other decision
  in this file. The brush's own raised-cosine kernel already limits how sharp a
  gesture can be, so hand tremor is bounded without it. Smoothing is one drag
  away for anyone who wants it.

On a normal drawn curve the result is now 0.10 dB of fit error with the ghost
invisible beneath the plot, which is what "1:1" should look like.

---

## Starting shapes, and fidelity as a button rather than a slider

Two requests, from the observation that sharp turns in a drawn curve came back
as smooth arcs.

**The arcs were the old smoothing default**, already fixed in the entry above.
Measured on a hard V drawn down to 400 Hz and straight back up, with smoothing
at zero and the committed fit: drawn **-17.89 dB** at the apex, realised
**-17.82 dB**. The corner survives. What was missing was any way to know that
without finding a slider called SMOOTH and understanding what it did.

**Fidelity is now three buttons** - `1:1`, `Soft`, `Smooth` - sitting directly
under the shape menu and setting `smooth` to 0, 15 and 40 %. The numeric bar is
still there for anything in between, and a button lights when the bar happens to
match it. Presets that *set* a real parameter rather than shadowing it: no new
state, nothing to keep in sync, and no way for the button and the value to
disagree.

**Eleven starting shapes** - flat, smiley, warm and bright tilt, de-mud,
presence, air, rumble cut, tape roll-off, telephone, vocal. They are generated,
not stored. A shape built from bells and shelves is one the Analog fitter can
reproduce almost exactly, which a captured 1024-point snapshot would not be, and
generating them means they resample to any future grid size for free. Loading
one is a single undo entry and earns the committed-quality fit, because a shape
is a finished curve.

`TestCurveFitter` holds every shape to a measured bound - a thousandth of a dB
for the band-derived ones, and under a third of a dB for the three built from
slopes steeper than any shelf can go. The bounds are set just above what is
measured rather than at a comfortable round number, because a threshold with an
order of magnitude of slack never catches the regression it exists for.

This also closes part of the "factory presets not authored" gap noted at the top
of this file: the bank still ships empty, but the plugin no longer starts with
nothing to reach for.

---

## Third visual direction: the grey instrument face

On a supplied reference (Ableton's EQ Eight), the interface moved again. The
phosphor terminal recorded above is gone. What replaced it:

- **Warm grey chrome with a recessed dark plate.** The only depth in the panel
  comes from the plot being darker than the face it sits in; nothing is
  bevelled or textured. That is where the "old but new" quality lives - the
  panel reads as a physical device, the controls on it are flat.
- **Two colours doing two jobs, consistently.** Blue is what the filter *does*:
  the response curve, the knob arcs, the value fields. Amber is what the user
  can *grab or has switched on*: band handles, the active slot, engaged
  switches. Nothing is coloured for decoration, so a glance at any control says
  which kind of thing it is.
- **Band handles became solid amber discs.** Against a dark plate a filled
  handle is unambiguously a thing to grab, and at 20 px the number stays
  readable where a ring's outline and its digit start to merge. Frequency still
  rides the colour, now as a warm-to-bright ramp inside the amber rather than
  as a hue - deep ochre at 20 Hz, pale gold at 20 kHz - so the strip does not
  turn into a rainbow.
- **Knobs and tinted value fields replaced the character bars.** `BarSlider` is
  deleted; `Knob` is back for the left column and `ValueField` handles the
  right. A tinted field means "this number is yours to change", which is a
  cheaper affordance than any border or handle.
- **Scales moved inside the plate**, dimmed, which returns the margin they were
  using to the plot.

**A new control came with the layout.** The reference devotes its left column to
the selected band's frequency, gain and Q, and DrawEQ already had the Node
tool for grabbing bands - so clicking a band with it now fills that column, and
typing into it writes back through exactly the path a drag takes. A number typed
and a token dragged cannot disagree, because they are the same code.

Selecting a band must not count as editing it: writing values into the knobs
fires their callbacks, which would push straight back into the curve and re-fit.
A guard around the refresh is what keeps a click from being a change.

**Not adopted.** The reference's per-band filter-type dropdowns have no
counterpart here - band types are the fitter's output, not the user's input,
and offering a control that the next fit would overwrite would be a lie. Its
`Scale` control is likewise absent: the plate shows the full +/-30 dB the curve
model allows, which wastes some vertical space on a gentle curve but never
displays a -30 dB cut as though it were -20.

---

## Dark, pixel-crisp, and tight

Four changes at once, on request: dark mode, a more pixelated look, bars instead
of knobs, and a smaller window with less air in it. The window went from
1040x500 to **880x400** - 32 % less area - without dropping a single control.

**One control type did most of the tightening.** `Knob` and `ValueField` are both
gone, replaced by `BarControl`: a caption, a bar built from discrete cells, and
a number, in one 22 px row. A bar is a fifth the height of a dial, so the panel
needs two short rows along the bottom where it previously needed a column down
each side. The left column is gone entirely and the selected band's frequency,
gain and Q moved into the second bottom row.

**Cells, not fills.** A poured bar has to be measured against its ends to be
read; a counted one can be read directly. The cells are 4 px with a 1 px gap, so
they land on whole pixels by construction, which is most of where the crispness
comes from. A single brighter cell marks the value, so a bipolar bar still says
which side of centre it is on when it is nearly empty.

**What "pixelated" was applied to, and what it was not.** Every corner is square,
every rule is dotted, band handles became squares on whole-pixel centres, and
labels moved to the monospaced face because at 10 px its stems land on pixel
boundaries. The **curve stays antialiased**. It is the measurement, and
stair-stepping it would make it harder to read without making it more honest -
pixelation is a treatment for the chrome, not for the data.

The zero line is the one rule still drawn solid, because it is the reference
every reading on the plate is taken against.

**The plate now shows +/-24 dB** rather than the model's +/-30. Drawing is bounded
by what is on screen, so nothing is hidden by the choice: it only means a shape
loaded from a preset can have its last few dB, at the extreme edges of the
spectrum, drawn flat against the rail. In exchange the curve occupies a useful
fraction of the plate instead of a third of it - which was the largest single
piece of the "lot of open space".

**Bars were briefly unreadable.** The first pass gave the right column 118 px, of
which the caption and number took 82, leaving a seven-cell bar. Widening the
column and narrowing the number to 32 px fixed it. Worth recording because the
failure mode is specific to counted bars: a poured bar degrades gracefully into
a thin line, a counted one stops being a bar at all.

---

## Faders, and where the character came from

Two complaints: the panel looked plain and unlike an EQ, and the controls were
progress bars rather than anything you would find on a desk.

**A progress bar is not a fader, and the difference is not cosmetic.** A filled
bar answers *how much*; a capped fader answers *where*. For a tilt or a shift -
values that live either side of a centre - *where* is the question actually
being asked, and a bar has no way to point at it. `BarControl` became `Fader`: a
recessed slot with ticks along it, a taller cap running in it with a grip line
down its middle, a raised centre tick where a console fader has its detent
moulded in, and the travelled part of the slot lit so it still says how much as
well as where.

One detail worth keeping: the cap's centre can only reach half a cap-width from
each end, so the slot is inset to match. Without that the travel and the drawing
disagree and the cap looks like it stops short of the ends.

**The character came from one pixel.** Not from ornament: `drawPixelBevel` puts
a single light pixel along the top and left of anything raised and a single dark
one along the bottom and right, and reverses it for anything recessed. That is
the entire depth model - no gradients, no blurs, nothing that would blur across
a pixel boundary. It is the look the tracker software this kind of plugin grew
out of had, and it costs two `fillRect` calls.

Applied consistently it does a lot of work for free: a pressed button is
genuinely recessed rather than merely darker, the selected tool sits pressed
into the panel the way a latched hardware button does, the plate is recessed
into the face, and the strips are raised out of it. The combo caret is drawn as
a stack of shortening rows for the same reason - a triangle path would
antialias, and a glyph would depend on a face that may not carry it.

**Everything grew and tightened at once.** 880x400 became 1100x560 with type up
roughly 20 % - and the panel is *fuller*, not emptier, because the layout was
reorganised rather than scaled. The four macros moved out of a second bottom row
into the right column, which had a void under it, and the bottom went from two
rows to one. The plate took the difference.

The column now sizes its switches first and lets the faders divide what is
actually left, so spare room never collects as a gap above the bottom of a
column. The bottom row sizes the band section first and lets the slot strip take
the remainder, so widening the window widens the slots rather than opening a
hole in the middle of the row.

---

## Host simulation, and the two things it found

The unit tests exercise the DSP classes directly. That leaves the VST3 boundary
untested - the factory, bus negotiation, the parameter interface, the state
blob - which is exactly where "our code works" and "the plugin works in a DAW"
come apart. `DrawEQHostSim` loads the built `.vst3` through JUCE's own VST3
host and drives it the way a channel insert is driven: odd buffer lengths, the
sample rate changing underneath, a latency change mid-playback, state across a
save and reload, four instances at once, bypass, and a window opened and closed
five times.

It found two real defects that nothing else had.

**The plugin exposed two bypass controls.** `AudioProcessor::getBypassParameter`
was never overridden, so JUCE's VST3 wrapper synthesised its own alongside the
one in the APVTS. A host's bypass button drove the synthetic one, which cuts
hard - no 20 ms ramp, no latency-compensated dry path. Overriding it hands the
host ours, and there is one control that does the right thing.

**The bypass ramp oscillated forever.** Written as

    ramp += (target > ramp ? +step : -step)

it steps *down* the moment it arrives, because `1.0 > 1.0` is false. Then up,
then down, at the step rate, for as long as the plugin is bypassed. The effect
was a permanent ~1e-3 of the processed signal leaking through a bypassed plugin,
dithered at around a kilohertz. Moving toward the target and stopping on it
takes the bypass null from 9.4e-4 to **3.0e-8**, which is float rounding on the
mix arithmetic and nothing else.

Both are the kind of fault that survives every DSP test - the filter was never
wrong - and would have been found by a user toggling bypass and hearing
something.

`OnePole` also learned to settle. An exponential approach never arrives, so a
smoother that had "finished" still sat a fraction short of its target
indefinitely; it now lands once the remaining distance is inaudible, which makes
a finished gain exactly one and keeps settled smoothers from feeding denormals
into what they drive.

**What this still does not prove.** It is JUCE hosting JUCE. It cannot catch
something specific to FL Studio's wrapper - its plugin window handling, its
delay compensation around a mode change, or how it treats a resizable editor
with a fixed aspect ratio. Loading it in FL remains the outstanding test, and it
is the first thing to do on a Windows machine.

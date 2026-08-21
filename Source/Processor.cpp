#include "Processor.h"
#include "UI/Editor.h"

#include <juce_core/juce_core.h>

namespace graphite
{

namespace
{
    int nextPow2 (int v)
    {
        int p = 1;

        while (p < v)
            p <<= 1;

        return p;
    }

    /** Equal power, so that two uncorrelated signals - which two different
        filter phases certainly are - keep a constant total level across the
        fade instead of dipping in the middle. */
    inline void equalPowerWeights (float t, float& oldW, float& newW) noexcept
    {
        const float angle = 1.5707963268f * juce::jlimit (0.0f, 1.0f, t);
        oldW = std::cos (angle);
        newW = std::sin (angle);
    }
}

GraphiteProcessor::GraphiteProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRAPHITE", params::createLayout())
{
    pBypass   = apvts.getRawParameterValue (params::id::bypass);
    pMode     = apvts.getRawParameterValue (params::id::mode);
    pOutput   = apvts.getRawParameterValue (params::id::outputGain);
    pMix      = apvts.getRawParameterValue (params::id::mix);
    pTilt     = apvts.getRawParameterValue (params::id::tilt);
    pSmooth   = apvts.getRawParameterValue (params::id::smooth);
    pShift    = apvts.getRawParameterValue (params::id::freqShift);
    pMorph    = apvts.getRawParameterValue (params::id::morph);
    pMorphA   = apvts.getRawParameterValue (params::id::morphA);
    pMorphB   = apvts.getRawParameterValue (params::id::morphB);
    pBands    = apvts.getRawParameterValue (params::id::bandCount);
    pAnalyzer = apvts.getRawParameterValue (params::id::analyzer);
    pInvert   = apvts.getRawParameterValue (params::id::phaseInvert);

    curveWorker.setSource (&model);
}

GraphiteProcessor::~GraphiteProcessor()
{
    stopTimer();
    curveWorker.stop();
}

bool GraphiteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

double GraphiteProcessor::getTailLengthSeconds() const
{
    // Spectral mode's tail is the IR; Analog's is the ring-down of its lowest
    // band. Reporting the longer of the two is honest in either mode.
    return double (IRBuilder::irLengthFor (sr)) / sr;
}

void GraphiteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr       = sampleRate;
    maxBlock = juce::jmax (16, samplesPerBlock);

    const int channels = juce::jmax (1, getTotalNumOutputChannels());

    curveWorker.stop();
    curveWorker.prepare (sampleRate, maxBlock);

    cascade.prepare (sampleRate, channels);
    cascade.reset();

    const int partition = ConvolutionEngine::partitionSizeFor (maxBlock);
    const int irLength  = IRBuilder::irLengthFor (sampleRate);

    for (auto& e : engines)
        e.prepare (irLength, partition, maxBlock);

    dryBuffer.setSize (kMaxChannels, maxBlock, false, true, true);
    pathNew.setSize (kMaxChannels, maxBlock, false, true, true);
    pathOld.setSize (kMaxChannels, maxBlock, false, true, true);

    // The dry path has to be delayed by whatever the wet path costs, or `mix`
    // becomes a comb filter rather than a blend.
    const int maxLatency = irLength / 2 + partition + maxBlock;
    const int delayCap   = nextPow2 (maxLatency + 1);
    dryDelayMask = delayCap - 1;

    for (auto& d : dryDelay)
        d.assign (std::size_t (delayCap), 0.0f);

    dryDelayWrite = 0;

    trimSmoother.prepare (sampleRate, 20.0f);
    gainSmoother.prepare (sampleRate, 20.0f);
    mixSmoother.prepare (sampleRate, 20.0f);
    bypassSmoother.prepare (sampleRate, 20.0f);
    trimSmoother.snap (0.0f);
    gainSmoother.snap (1.0f);
    mixSmoother.snap (1.0f);
    bypassSmoother.snap (0.0f);

    active = fading = nullptr;
    fadeSamplesLeft = fadeSamplesTotal = 0;
    activeMode = fadingMode = Mode (int (pMode->load()));

    spectrum.prepare (sampleRate);

    prepared = true;

    pushMacrosToWorker();
    curveWorker.setMorphTarget (presets.curveFor (int (pMorphB->load()) - 1));
    curveWorker.requestColdFit();

    // Build one state synchronously so the very first block already has a
    // filter rather than a bypass.
    curveWorker.buildOnceForTesting();
    curveWorker.start();

    startTimerHz (20);
}

void GraphiteProcessor::releaseResources()
{
    stopTimer();
    curveWorker.stop();
    prepared = false;
}

// ---------------------------------------------------------------------------
// Message thread
// ---------------------------------------------------------------------------

void GraphiteProcessor::pushMacrosToWorker()
{
    curveWorker.setMacros (pTilt->load(), pSmooth->load(), pShift->load(),
                           int (pBands->load()), Mode (int (pMode->load())));
    curveWorker.setMorphAmount (pMorph->load() * 0.01f);
}

void GraphiteProcessor::timerCallback()
{
    pushMacrosToWorker();

    const int a = int (pMorphA->load());
    const int b = int (pMorphB->load());

    if (a != lastMorphA)
    {
        // Switching the edit slot banks the current drawing before loading the
        // new one, so nothing the user drew is ever silently discarded.
        storeCurrentIntoSlot (lastMorphA);
        recallSlot (a);
        lastMorphA = a;
    }

    if (b != lastMorphB)
    {
        curveWorker.setMorphTarget (presets.curveFor (b - 1));
        lastMorphB = b;
    }

    const auto mode = params::AnalyzerMode (int (pAnalyzer->load()));
    spectrum.setEnabled (mode == params::AnalyzerMode::pre || mode == params::AnalyzerMode::both,
                         mode == params::AnalyzerMode::post || mode == params::AnalyzerMode::both);

    // setLatencySamples must not be called from the audio thread, so the change
    // is noticed here instead.
    const int engineLatency = Mode (int (pMode->load())) == Mode::analog
                            ? 0 : engines[0].latencySamples();
    const int wanted = curveWorker.publishedLatency() + engineLatency;

    if (wanted != reportedLatency)
    {
        reportedLatency = wanted;
        setLatencySamples (wanted);
    }
}

void GraphiteProcessor::storeCurrentIntoSlot (int slot)
{
    presets.store (slot - 1, model.getCurve(), pTilt->load(), pSmooth->load(), pShift->load());

    if (slot == int (pMorphB->load()))
        curveWorker.setMorphTarget (presets.curveFor (slot - 1));
}

void GraphiteProcessor::recallSlot (int slot)
{
    CurveArray c {};
    float tilt = 0.0f, smooth = 0.0f, shift = 0.0f;

    if (presets.recall (slot - 1, c, tilt, smooth, shift))
        model.setCurve (c);
    else
        model.reset();

    // A recalled curve has nothing to do with the previous solution, so the
    // warm start would be a hindrance rather than a hint.
    curveWorker.requestColdFit();
}

void GraphiteProcessor::clearSlot (int slot)
{
    presets.clear (slot - 1);

    if (slot == int (pMorphB->load()))
        curveWorker.setMorphTarget (presets.curveFor (slot - 1));
}

// ---------------------------------------------------------------------------
// Audio thread
// ---------------------------------------------------------------------------

void GraphiteProcessor::adoptState (FilterState* incoming) noexcept
{
    if (incoming == nullptr)
        return;

    const bool analogToAnalog = active != nullptr
                             && active->mode == Mode::analog
                             && incoming->mode == Mode::analog;

    if (analogToAnalog)
    {
        // No crossfade: the cascade glides (f, G, Q) instead, which is both
        // cheaper and free of the comb filtering that fading two differently
        // phased responses would produce (CONTEXT.md 5).
        cascade.setTargets (incoming->bands.data(), incoming->numBands);
        trimSmoother.snap (trimSmoother.get());

        FilterState* old = active;
        active = incoming;
        activeMode = Mode::analog;
        curveWorker.ring().retire (old);
        return;
    }

    if (active == nullptr)
    {
        active = incoming;
        activeMode = incoming->mode;

        if (incoming->mode == Mode::analog)
        {
            cascade.setTargets (incoming->bands.data(), incoming->numBands);
            cascade.snapToTargets();
        }

        return;
    }

    // Anything else - a mode switch, or a new IR - crossfades over the outputs.
    if (fading != nullptr)
    {
        // A second change arrived mid-fade. Retiring the older of the two and
        // restarting is a small discontinuity; holding a third state would need
        // a third signal path for no audible benefit.
        curveWorker.ring().retire (fading);
    }

    fading     = active;
    fadingMode = activeMode;
    active     = incoming;
    activeMode = incoming->mode;

    if (activeMode != Mode::analog && fadingMode == Mode::analog)
    {
        // The convolution engines have been idle, so their input history is
        // stale. Starting from silence is covered by the fade-in.
        for (auto& e : engines)
            e.reset();
    }

    if (activeMode == Mode::analog)
        cascade.setTargets (active->bands.data(), active->numBands);

    fadeSamplesTotal = int (kCrossfadeMs * 0.001 * sr);
    fadeSamplesLeft  = fadeSamplesTotal;
}

void GraphiteProcessor::renderAnalog (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    float* channels[kMaxChannels] {};
    const int nch = juce::jmin (buffer.getNumChannels(), kMaxChannels);

    for (int ch = 0; ch < nch; ++ch)
        channels[ch] = buffer.getWritePointer (ch);

    cascade.process (channels, nch, numSamples);

    const float trim = juce::Decibels::decibelsToGain (
        trimSmoother.advance (active != nullptr ? active->trimDb : 0.0f, numSamples));

    for (int ch = 0; ch < nch; ++ch)
        juce::FloatVectorOperations::multiply (channels[ch], trim, numSamples);
}

void GraphiteProcessor::renderSpectral (juce::AudioBuffer<float>& buffer, int numSamples,
                                        const float* irA, const float* irB,
                                        float* const* destA, float* const* destB) noexcept
{
    const int nch = juce::jmin (buffer.getNumChannels(), kMaxChannels);

    for (int ch = 0; ch < nch; ++ch)
        engines[std::size_t (ch)].process (buffer.getReadPointer (ch),
                                           destA[ch], destB[ch], numSamples, irA, irB);
}

void GraphiteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int nch = juce::jmin (buffer.getNumChannels(), kMaxChannels);

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (! prepared || numSamples == 0 || nch == 0)
        return;

    if (auto* incoming = curveWorker.ring().consume())
        adoptState (incoming);

    // Delayed dry copy, for `mix` and for soft bypass.
    for (int ch = 0; ch < nch; ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        float* dst = dryBuffer.getWritePointer (ch);
        auto& line = dryDelay[std::size_t (ch)];
        int w = dryDelayWrite;

        const int delay = juce::jlimit (0, dryDelayMask, reportedLatency);

        for (int n = 0; n < numSamples; ++n)
        {
            line[std::size_t (w)] = src[n];
            dst[n] = line[std::size_t ((w - delay) & dryDelayMask)];
            w = (w + 1) & dryDelayMask;
        }
    }

    dryDelayWrite = (dryDelayWrite + numSamples) & dryDelayMask;

    spectrum.pushPre (buffer.getArrayOfReadPointers(), nch, numSamples);

    const bool fadingNow = fadeSamplesLeft > 0 && fading != nullptr;

    float* newPtr[kMaxChannels] {};
    float* oldPtr[kMaxChannels] {};

    for (int ch = 0; ch < nch; ++ch)
    {
        newPtr[ch] = pathNew.getWritePointer (ch);
        oldPtr[ch] = pathOld.getWritePointer (ch);
    }

    if (! fadingNow)
    {
        if (activeMode == Mode::analog || active == nullptr)
        {
            renderAnalog (buffer, numSamples);
        }
        else
        {
            renderSpectral (buffer, numSamples, active->irSpectra.data(), nullptr, newPtr, oldPtr);

            for (int ch = 0; ch < nch; ++ch)
                juce::FloatVectorOperations::copy (buffer.getWritePointer (ch), newPtr[ch], numSamples);
        }
    }
    else if (activeMode != Mode::analog && fadingMode != Mode::analog)
    {
        // Both spectral: one pass through the shared input history produces
        // both responses, perfectly aligned.
        renderSpectral (buffer, numSamples, fading->irSpectra.data(), active->irSpectra.data(),
                        oldPtr, newPtr);
    }
    else
    {
        // A mode switch: the two paths are different machines, so each gets the
        // input separately.
        for (int ch = 0; ch < nch; ++ch)
        {
            juce::FloatVectorOperations::copy (newPtr[ch], buffer.getReadPointer (ch), numSamples);
            juce::FloatVectorOperations::copy (oldPtr[ch], buffer.getReadPointer (ch), numSamples);
        }

        if (activeMode == Mode::analog)
        {
            juce::AudioBuffer<float> wrapNew (newPtr, nch, numSamples);
            renderAnalog (wrapNew, numSamples);
            renderSpectral (buffer, numSamples, fading->irSpectra.data(), nullptr, oldPtr, oldPtr);
        }
        else
        {
            juce::AudioBuffer<float> wrapOld (oldPtr, nch, numSamples);
            renderAnalog (wrapOld, numSamples);
            renderSpectral (buffer, numSamples, active->irSpectra.data(), nullptr, newPtr, newPtr);
        }
    }

    if (fadingNow)
    {
        const int fadeStart = fadeSamplesTotal - fadeSamplesLeft;

        for (int ch = 0; ch < nch; ++ch)
        {
            float* out = buffer.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
            {
                const int pos = fadeStart + n;
                float wo = 0.0f, wn = 1.0f;

                if (pos < fadeSamplesTotal)
                    equalPowerWeights (float (pos) / float (fadeSamplesTotal), wo, wn);

                out[n] = wo * oldPtr[ch][n] + wn * newPtr[ch][n];
            }
        }

        fadeSamplesLeft = juce::jmax (0, fadeSamplesLeft - numSamples);

        if (fadeSamplesLeft == 0)
        {
            curveWorker.ring().retire (fading);
            fading = nullptr;
        }
    }

    // Mix, output gain, polarity, soft bypass.
    const float mixTarget    = pMix->load() * 0.01f;
    const float gainTarget   = juce::Decibels::decibelsToGain (pOutput->load());
    const float bypassTarget = pBypass->load() > 0.5f ? 1.0f : 0.0f;
    const float polarity     = pInvert->load() > 0.5f ? -1.0f : 1.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const float mix    = mixSmoother.process (mixTarget);
        const float gain   = gainSmoother.process (gainTarget);
        const float bypass = bypassSmoother.process (bypassTarget);

        for (int ch = 0; ch < nch; ++ch)
        {
            float* out = buffer.getWritePointer (ch);
            const float dry = dryBuffer.getReadPointer (ch)[n];
            const float wet = out[n] * polarity;
            const float mixed = (dry + mix * (wet - dry)) * gain;
            out[n] = mixed + bypass * (dry - mixed);
        }
    }

    spectrum.pushPost (buffer.getArrayOfReadPointers(), nch, numSamples);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void GraphiteProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();

    if (xml == nullptr)
        return;

    // The curve is not a parameter and never will be - a host has nowhere to
    // put 1024 automatable floats. It travels here instead, versioned, so a
    // future format can migrate rather than load as noise (CONTEXT.md 8.1).
    storeCurrentIntoSlot (int (pMorphA->load()));

    const auto curveBlob = model.serialise();
    xml->setAttribute ("curve", juce::Base64::toBase64 (curveBlob.data(), curveBlob.size()));
    xml->setAttribute ("curveVersion", int (CurveModel::kCurveVersion));

    const auto bankBlob = presets.serialise();
    xml->setAttribute ("bank", juce::Base64::toBase64 (bankBlob.data(), bankBlob.size()));
    xml->setAttribute ("bankVersion", int (PresetBank::kBankVersion));

    copyXmlToBinary (*xml, destination);
}

void GraphiteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto decode = [] (const juce::String& text, std::vector<std::uint8_t>& out)
    {
        juce::MemoryOutputStream stream;

        if (! juce::Base64::convertFromBase64 (stream, text))
            return false;

        out.assign (static_cast<const std::uint8_t*> (stream.getData()),
                    static_cast<const std::uint8_t*> (stream.getData()) + stream.getDataSize());
        return true;
    };

    std::vector<std::uint8_t> blob;

    if (decode (xml->getStringAttribute ("bank"), blob))
        presets.deserialise (blob.data(), blob.size());

    if (decode (xml->getStringAttribute ("curve"), blob))
        model.deserialise (blob.data(), blob.size());

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    lastMorphA = int (pMorphA->load());
    lastMorphB = int (pMorphB->load());

    if (prepared)
    {
        pushMacrosToWorker();
        curveWorker.setMorphTarget (presets.curveFor (lastMorphB - 1));
        curveWorker.requestColdFit();
    }
}

juce::AudioProcessorEditor* GraphiteProcessor::createEditor()
{
    return new GraphiteEditor (*this);
}

} // namespace graphite

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new graphite::GraphiteProcessor();
}

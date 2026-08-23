/**
    Loads the built VST3 the way a host does, and puts it through what a mixer
    insert actually experiences.

    This is not a second copy of the unit tests. Those exercise the DSP classes
    directly; this goes through the VST3 boundary - the factory, the bus
    negotiation, the parameter interface, the state blob - which is where the
    difference between "our code works" and "the plugin works in a DAW" lives.

    Scenarios are chosen for what a channel insert does to a plugin, not for
    coverage: odd buffer lengths, the sample rate changing under it, a latency
    change mid-playback, state surviving a save and reload, several instances at
    once, and a window being opened and closed repeatedly.

        DrawEQHostSim <path-to.vst3>
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  %s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8());

    if (! ok)
        ++failures;
}

void section (const juce::String& name)
{
    std::printf ("\n%s\n", name.toRawUTF8());
}

bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getReadPointer (ch)[i]))
                return false;

    return true;
}

float peakOf (const juce::AudioBuffer<float>& b)
{
    float peak = 0.0f;

    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        peak = juce::jmax (peak, b.getMagnitude (ch, 0, b.getNumSamples()));

    return peak;
}

/** Fills with a steady tone plus noise: something with energy at every
    frequency the filter might touch, and a predictable level. */
void fillSignal (juce::AudioBuffer<float>& b, int& phase)
{
    juce::Random rng (int64_t (phase) * 7919 + 13);

    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float s = 0.25f * std::sin (float (phase) * 0.031f)
                      + 0.05f * (rng.nextFloat() * 2.0f - 1.0f);

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            b.getWritePointer (ch)[i] = s;

        ++phase;
    }
}

std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& formats,
                                                 const juce::String& path)
{
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, path);

    if (found.isEmpty())
        return {};

    juce::String error;
    return formats.createPluginInstance (*found[0], 48000.0, 512, error);
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::fprintf (stderr, "usage: DrawEQHostSim <path-to.vst3>\n");
        return 2;
    }

    const juce::String path (argv[1]);

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    auto plugin = load (formats, path);

    if (plugin == nullptr)
    {
        std::fprintf (stderr, "could not load %s\n", path.toRawUTF8());
        return 2;
    }

    std::printf ("loaded: %s  (%s)\n",
                 plugin->getName().toRawUTF8(),
                 plugin->getPluginDescription().manufacturerName.toRawUTF8());

    // -----------------------------------------------------------------------
    section ("bus layout - a mixer channel is stereo in, stereo out");
    {
        check (plugin->setBusesLayout (juce::AudioProcessor::BusesLayout {
                   { juce::AudioChannelSet::stereo() }, { juce::AudioChannelSet::stereo() } }),
               "accepts stereo/stereo");

        check (plugin->setBusesLayout (juce::AudioProcessor::BusesLayout {
                   { juce::AudioChannelSet::mono() }, { juce::AudioChannelSet::mono() } }),
               "accepts mono/mono");

        plugin->setBusesLayout (juce::AudioProcessor::BusesLayout {
            { juce::AudioChannelSet::stereo() }, { juce::AudioChannelSet::stereo() } });
    }

    // -----------------------------------------------------------------------
    section ("buffer lengths a host actually sends, including odd ones");
    {
        juce::MidiBuffer midi;
        int phase = 0;
        bool finite = true, underrun = false;

        for (int block : { 512, 480, 441, 300, 128, 100, 64, 32, 1024, 2048 })
        {
            plugin->prepareToPlay (48000.0, block);
            juce::AudioBuffer<float> buffer (2, block);

            for (int n = 0; n < 40; ++n)
            {
                fillSignal (buffer, phase);
                plugin->processBlock (buffer, midi);

                if (! allFinite (buffer))
                    finite = false;

                if (peakOf (buffer) > 8.0f)
                    underrun = true;
            }

            plugin->releaseResources();
        }

        check (finite, "output stays finite at every block length");
        check (! underrun, "no runaway output at any block length");
    }

    // -----------------------------------------------------------------------
    section ("sample rate changing under it, as when the audio device changes");
    {
        juce::MidiBuffer midi;
        int phase = 0;
        bool ok = true;

        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0, 44100.0 })
        {
            plugin->prepareToPlay (sr, 256);
            juce::AudioBuffer<float> buffer (2, 256);

            for (int n = 0; n < 30; ++n)
            {
                fillSignal (buffer, phase);
                plugin->processBlock (buffer, midi);

                if (! allFinite (buffer))
                    ok = false;
            }

            plugin->releaseResources();
        }

        check (ok, "survives repeated prepare at 44.1 through 192 kHz");
    }

    // -----------------------------------------------------------------------
    section ("latency changes mid-playback - the case that breaks delay compensation");
    {
        plugin->prepareToPlay (48000.0, 256);

        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        int phase = 0;

        auto* modeParam = plugin->getParameters()[1];

        for (auto* p : plugin->getParameters())
            if (p->getName (32).equalsIgnoreCase ("Mode"))
                modeParam = p;

        check (modeParam != nullptr, "mode parameter is exposed to the host");

        std::array<int, 3> latencyFor {};
        bool finite = true;

        for (int mode = 0; mode < 3 && modeParam != nullptr; ++mode)
        {
            // A host sees parameters normalised; a three-value choice puts its
            // indices at 0, 0.5 and 1.
            modeParam->setValueNotifyingHost (float (mode) * 0.5f);

            // Give the worker its 1/30 s tick plus the message thread's turn to
            // report the new latency, exactly as it would get in a host.
            for (int n = 0; n < 40; ++n)
            {
                fillSignal (buffer, phase);
                plugin->processBlock (buffer, midi);

                if (! allFinite (buffer))
                    finite = false;

                juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
            }

            latencyFor[std::size_t (mode)] = plugin->getLatencySamples();
        }

        check (finite, "no NaN while switching realisation mode under audio");
        check (latencyFor[2] == 0, "Analog reports zero latency");
        check (latencyFor[0] > 0, "Linear phase reports its latency to the host");

        std::printf ("       reported latency: linear %d, minimum %d, analog %d samples\n",
                     latencyFor[0], latencyFor[1], latencyFor[2]);

        plugin->releaseResources();
    }

    // -----------------------------------------------------------------------
    section ("state survives a project save and reload");
    {
        plugin->prepareToPlay (48000.0, 256);

        // Move something that lives in the state blob rather than in a
        // parameter, which is the half a host does not manage for us.
        for (auto* p : plugin->getParameters())
            if (p->getName (32).equalsIgnoreCase ("Tilt"))
                p->setValueNotifyingHost (0.8f);

        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

        juce::MemoryBlock saved;
        plugin->getStateInformation (saved);
        check (saved.getSize() > 1024, "state blob carries the curve, not just parameters");

        auto second = load (formats, path);
        check (second != nullptr, "a second instance loads");

        if (second != nullptr)
        {
            second->prepareToPlay (48000.0, 256);
            second->setStateInformation (saved.getData(), int (saved.getSize()));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

            juce::MemoryBlock reSaved;
            second->getStateInformation (reSaved);

            check (reSaved.getSize() == saved.getSize(),
                   "restored instance saves a blob of the same size");

            float a = -1.0f, b = -2.0f;

            for (auto* p : plugin->getParameters())
                if (p->getName (32).equalsIgnoreCase ("Tilt"))
                    a = p->getValue();

            for (auto* p : second->getParameters())
                if (p->getName (32).equalsIgnoreCase ("Tilt"))
                    b = p->getValue();

            check (std::abs (a - b) < 1.0e-4f, "parameters come back with the same values");

            second->releaseResources();
        }

        plugin->releaseResources();
    }

    // -----------------------------------------------------------------------
    section ("several instances at once, as on several mixer channels");
    {
        std::vector<std::unique_ptr<juce::AudioPluginInstance>> rack;
        bool ok = true;

        for (int i = 0; i < 4; ++i)
        {
            auto instance = load (formats, path);

            if (instance == nullptr)
            {
                ok = false;
                break;
            }

            instance->prepareToPlay (48000.0, 256);
            rack.push_back (std::move (instance));
        }

        check (ok && rack.size() == 4, "four instances load and prepare");

        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        int phase = 0;
        bool finite = true;

        for (int n = 0; n < 60; ++n)
        {
            fillSignal (buffer, phase);

            for (auto& instance : rack)
            {
                instance->processBlock (buffer, midi);

                if (! allFinite (buffer))
                    finite = false;
            }
        }

        check (finite, "four in series stay finite");

        for (auto& instance : rack)
            instance->releaseResources();
    }

    // -----------------------------------------------------------------------
    section ("automation, at the rate a host writes it");
    {
        plugin->prepareToPlay (48000.0, 128);

        juce::AudioBuffer<float> buffer (2, 128);
        juce::MidiBuffer midi;
        int phase = 0;
        bool finite = true;
        float worst = 0.0f;

        auto params = plugin->getParameters();

        for (int n = 0; n < 400; ++n)
        {
            const float t = float (n) / 400.0f;

            for (int i = 0; i < params.size(); ++i)
                params[i]->setValueNotifyingHost (
                    0.5f + 0.5f * std::sin (t * 30.0f + float (i)));

            fillSignal (buffer, phase);
            plugin->processBlock (buffer, midi);

            if (! allFinite (buffer))
                finite = false;

            worst = juce::jmax (worst, peakOf (buffer));
        }

        check (finite, "every parameter swept at block rate, no NaN");
        std::printf ("       worst peak under full automation: %.2f\n", worst);

        plugin->releaseResources();
    }

    // -----------------------------------------------------------------------
    section ("bypass - the host's own button, not a second one of ours");
    {
        auto* bypass = plugin->getBypassParameter();
        check (bypass != nullptr, "plugin nominates a bypass parameter for the host");

        int named = 0;

        for (auto* p : plugin->getParameters())
            if (p->getName (32).containsIgnoreCase ("byp"))
                ++named;

        check (named == 1, "exactly one bypass control is exposed, not two");

        if (bypass != nullptr)
        {
            plugin->prepareToPlay (48000.0, 256);

            // Analog, so the plugin reports no latency and input and output can
            // be compared within a block. The automation sweep above left the
            // mode wherever its sine ended, and comparing across a 2048 sample
            // delay one 256 sample block at a time would be testing the test.
            for (auto* p : plugin->getParameters())
                if (p->getName (32).equalsIgnoreCase ("Mode"))
                    p->setValueNotifyingHost (1.0f);          // Analog

            // Something the filter would audibly change, so passing through
            // unaltered is a real result rather than a coincidence.
            for (auto* p : plugin->getParameters())
                if (p->getName (32).equalsIgnoreCase ("Tilt"))
                    p->setValueNotifyingHost (1.0f);

            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            check (plugin->getLatencySamples() == 0, "Analog reports no latency to compare across");

            bypass->setValueNotifyingHost (1.0f);

            juce::AudioBuffer<float> buffer (2, 256), dry (2, 256);
            juce::MidiBuffer midi;
            int phase = 0;
            double worst = 0.0;

            // Let the ramp settle first; the first 20 ms are a crossfade by
            // design and comparing against them would be testing the ramp, not
            // the bypass.
            for (int n = 0; n < 60; ++n)
            {
                fillSignal (buffer, phase);
                dry.makeCopyOf (buffer);
                plugin->processBlock (buffer, midi);

                if (n < 20)
                    continue;

                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    worst = juce::jmax (worst,
                                        std::abs (double (buffer.getReadPointer (0)[i])
                                                - double (dry.getReadPointer (0)[i])));
            }

            // A bypassed plugin should be bit-exact, not merely quiet. The
            // bound is float rounding on the mix arithmetic, not a tolerance
            // for something leaking through.
            check (worst < 1.0e-6, "bypassed audio is bit-exact with the input");
            std::printf ("       worst bypass deviation: %.2e\n", worst);

            bypass->setValueNotifyingHost (0.0f);
            plugin->releaseResources();
        }
    }

    // -----------------------------------------------------------------------
    section ("opening and closing the window, repeatedly");
    {
        plugin->prepareToPlay (48000.0, 256);
        bool ok = true;

        for (int i = 0; i < 5; ++i)
        {
            auto* editor = plugin->createEditorIfNeeded();

            if (editor == nullptr)
            {
                ok = false;
                break;
            }

            editor->setSize (900, 470);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            plugin->editorBeingDeleted (editor);
            delete editor;
        }

        check (ok, "editor opens and closes five times without dying");
        plugin->releaseResources();
    }

    plugin.reset();

    std::printf ("\n%s\n", failures == 0 ? "all host checks passed"
                                         : juce::String (failures).toRawUTF8());
    return failures == 0 ? 0 : 1;
}

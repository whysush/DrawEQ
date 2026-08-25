#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdint>

namespace draweq
{

/**
    An audition source, generated in place of the input.

    The standalone has no input by default - it mutes it to avoid a feedback
    loop - so there is nothing to hear the filter working on. This provides it,
    and because it replaces the input at the very top of the block, everything
    downstream is unchanged: the analyser sees it pre and post, the dry path
    delays it, mix and bypass behave exactly as they do on real audio.

    Audio thread only. A phase accumulator, a shift register and three
    one-poles: no allocation, no branching on anything but the mode.
*/
class TestTone
{
public:
    enum class Mode { off = 0, sine, pink, sweep };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        phase = 0.0;
        sweepSeconds = 0.0;
        b0 = b1 = b2 = 0.0f;
    }

    /** Replaces the buffer contents. `frequency` is only read in sine mode. */
    void process (juce::AudioBuffer<float>& buffer, int numSamples,
                  Mode mode, float frequency) noexcept
    {
        if (mode == Mode::off)
            return;

        const int channels = buffer.getNumChannels();

        for (int n = 0; n < numSamples; ++n)
        {
            float sample = 0.0f;

            switch (mode)
            {
                case Mode::sine:
                    sample = float (std::sin (phase));
                    advance (double (frequency));
                    break;

                case Mode::sweep:
                {
                    // Logarithmic, 20 Hz to 20 kHz over eight seconds, looping.
                    // Linear would spend seven of those eight seconds above
                    // 2 kHz and tell you almost nothing about the bottom half
                    // of the curve.
                    constexpr double sweepLength = 8.0;
                    const double t = sweepSeconds / sweepLength;
                    const double hz = 20.0 * std::pow (1000.0, t);

                    sample = float (std::sin (phase));
                    advance (hz);

                    sweepSeconds += 1.0 / sr;

                    if (sweepSeconds >= sweepLength)
                        sweepSeconds = 0.0;

                    break;
                }

                case Mode::pink:
                    sample = pinkNoise();
                    break;

                case Mode::off:
                    break;
            }

            sample *= kLevel;

            for (int ch = 0; ch < channels; ++ch)
                buffer.getWritePointer (ch)[n] = sample;
        }
    }

private:
    void advance (double hz) noexcept
    {
        phase += 6.283185307179586 * juce::jlimit (1.0, sr * 0.49, hz) / sr;

        if (phase >= 6.283185307179586)
            phase -= 6.283185307179586;
    }

    /** Pink rather than white, because pink is what a spectrum analyser reads
        flat and what an ear hears as even across the range - white noise is
        mostly treble and makes the bottom of the curve inaudible. Paul Kellet's
        three-pole approximation: accurate to a fraction of a dB and three
        multiply-adds. */
    float pinkNoise() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;

        const float white = float (int32_t (rng)) * (1.0f / 2147483648.0f);

        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;

        return (b0 + b1 + b2 + white * 0.1848f) * 0.4f;
    }

    /** -18 dBFS: loud enough to judge, quiet enough that a +24 dB draw does
        not clip the output. */
    static constexpr float kLevel = 0.126f;

    double sr = 48000.0;
    double phase = 0.0;
    double sweepSeconds = 0.0;
    std::uint32_t rng = 0x8badf00d;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
};

} // namespace draweq

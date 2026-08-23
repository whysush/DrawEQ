#include "Shapes.h"
#include "../DSP/BandResponse.h"

#include <algorithm>
#include <initializer_list>

namespace draweq::shapes
{

const char* name (Shape s)
{
    switch (s)
    {
        case Shape::flat:       return "Flat";
        case Shape::smiley:     return "Smiley";
        case Shape::warmTilt:   return "Warm tilt";
        case Shape::brightTilt: return "Bright tilt";
        case Shape::deMud:      return "De-mud";
        case Shape::presence:   return "Presence";
        case Shape::air:        return "Air";
        case Shape::rumbleCut:  return "Rumble cut";
        case Shape::telephone:  return "Telephone";
        case Shape::vocal:      return "Vocal";
        case Shape::lowPass:    return "Tape roll-off";
        case Shape::count:      break;
    }

    return "";
}

namespace
{
    /** Sum of bells and shelves, evaluated through the filter's own closed
        form - so anything built this way is a curve the Analog path can
        actually be. */
    void fromBands (std::initializer_list<Band> bands, double sr, CurveArray& out)
    {
        for (int i = 0; i < LogGrid::kSize; ++i)
        {
            const float hz = LogGrid::indexToHz (float (i));
            float acc = 0.0f;

            for (const auto& b : bands)
                acc += response::bandDb (b, hz, sr);

            out[std::size_t (i)] = LogGrid::clampDb (acc);
        }
    }

    /** A slope below (or above) a corner, in dB per octave. Steeper than any
        shelf, which is the point: a rumble cut is not a shelf. */
    void addSlope (CurveArray& c, float cornerHz, float dbPerOctave, bool belowCorner)
    {
        for (int i = 0; i < LogGrid::kSize; ++i)
        {
            const float hz = LogGrid::indexToHz (float (i));
            const float octaves = std::log2 (hz / cornerHz);

            if (belowCorner ? octaves < 0.0f : octaves > 0.0f)
                c[std::size_t (i)] = LogGrid::clampDb (c[std::size_t (i)]
                                                       + dbPerOctave * std::abs (octaves));
        }
    }
}

void build (Shape shape, double sr, CurveArray& out)
{
    out.fill (0.0f);

    switch (shape)
    {
        case Shape::flat:
            break;

        case Shape::smiley:
            fromBands ({ { BandType::lowShelf,    90.0f,  6.0f, 0.707f, 0, true },
                         { BandType::bell,       700.0f, -4.5f, 0.9f,   1, true },
                         { BandType::highShelf, 8000.0f,  5.5f, 0.707f, 2, true } }, sr, out);
            break;

        case Shape::warmTilt:
            fromBands ({ { BandType::lowShelf,   250.0f,  4.0f, 0.5f, 0, true },
                         { BandType::highShelf, 3000.0f, -5.0f, 0.5f, 1, true } }, sr, out);
            break;

        case Shape::brightTilt:
            fromBands ({ { BandType::lowShelf,   250.0f, -4.0f, 0.5f, 0, true },
                         { BandType::highShelf, 3000.0f,  5.0f, 0.5f, 1, true } }, sr, out);
            break;

        case Shape::deMud:
            fromBands ({ { BandType::bell, 300.0f, -6.0f, 1.0f, 0, true } }, sr, out);
            break;

        case Shape::presence:
            fromBands ({ { BandType::bell, 4000.0f, 5.0f, 0.9f, 0, true } }, sr, out);
            break;

        case Shape::air:
            fromBands ({ { BandType::highShelf, 10000.0f, 6.0f, 0.707f, 0, true } }, sr, out);
            break;

        case Shape::rumbleCut:
            // 12 dB per octave below 80 Hz, which is steeper than a shelf can
            // go and is the whole reason this is not one.
            addSlope (out, 80.0f, -12.0f, true);
            break;

        case Shape::lowPass:
            addSlope (out, 7000.0f, -9.0f, false);
            break;

        case Shape::telephone:
            // Band-limited both ends; no bells involved.
            addSlope (out, 400.0f, -14.0f, true);
            addSlope (out, 3000.0f, -14.0f, false);
            break;

        case Shape::vocal:
            fromBands ({ { BandType::bell,       280.0f, -4.0f, 1.1f,   0, true },
                         { BandType::bell,      3500.0f,  3.5f, 0.8f,   1, true },
                         { BandType::highShelf,12000.0f,  4.0f, 0.707f, 2, true } }, sr, out);
            addSlope (out, 90.0f, -12.0f, true);
            break;

        case Shape::count:
            break;
    }
}

} // namespace draweq::shapes

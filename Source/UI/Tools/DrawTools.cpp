#include "DrawTools.h"

namespace graphite
{

const char* toolName (Tool t)
{
    switch (t)
    {
        case Tool::pencil: return "PENCIL";
        case Tool::line:   return "LINE";
        case Tool::smooth: return "SMOOTH";
        case Tool::erase:  return "ERASE";
        case Tool::node:   return "NODE";
    }

    return "";
}

const char* toolGlyph (Tool t)
{
    switch (t)
    {
        case Tool::pencil: return "P";
        case Tool::line:   return "L";
        case Tool::smooth: return "S";
        case Tool::erase:  return "E";
        case Tool::node:   return "N";
    }

    return "";
}

juce::juce_wchar toolKey (Tool t)
{
    switch (t)
    {
        case Tool::pencil: return 'p';
        case Tool::line:   return 'l';
        case Tool::smooth: return 's';
        case Tool::erase:  return 'e';
        case Tool::node:   return 'n';
    }

    return 0;
}

CurveModel::Brush brushFor (Tool t)
{
    switch (t)
    {
        case Tool::smooth: return CurveModel::Brush::smooth;
        case Tool::erase:  return CurveModel::Brush::erase;

        case Tool::pencil:
        case Tool::line:
        case Tool::node:
            break;
    }

    return CurveModel::Brush::draw;
}

void curveFromBands (const Band* bands, int numBands, float trimDb, double sampleRate,
                     CurveArray& out)
{
    for (int i = 0; i < LogGrid::kSize; ++i)
    {
        const float hz = LogGrid::indexToHz (float (i));
        out[std::size_t (i)] = LogGrid::clampDb (
            trimDb + response::cascadeDb (bands, numBands, hz, sampleRate));
    }
}

} // namespace graphite

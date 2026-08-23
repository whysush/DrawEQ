#include "PresetBank.h"
#include "LogGrid.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace draweq
{

PresetBank::PresetBank()
{
    for (auto& s : slots)
        s.curve.fill (0.0f);
}

void PresetBank::store (int slot, const CurveArray& curve, float tilt, float smooth, float shift,
                        const std::string& name)
{
    auto& s = slots[std::size_t (clampSlot (slot))];
    s.curve  = curve;
    s.tilt   = tilt;
    s.smooth = smooth;
    s.shift  = shift;
    s.used   = true;

    if (! name.empty())
        s.name = name;
}

bool PresetBank::recall (int slot, CurveArray& curve, float& tilt, float& smooth, float& shift) const
{
    const auto& s = slots[std::size_t (clampSlot (slot))];

    if (! s.used)
        return false;

    curve  = s.curve;
    tilt   = s.tilt;
    smooth = s.smooth;
    shift  = s.shift;
    return true;
}

void PresetBank::clear (int slot)
{
    auto& s = slots[std::size_t (clampSlot (slot))];
    s.curve.fill (0.0f);
    s.used = false;
    s.name.clear();
}

// Layout: 'G','B','N','K' | uint16 version | uint16 slots | uint16 gridSize
//         then per slot: uint8 used | 3 float32 macros | gridSize float32
std::vector<std::uint8_t> PresetBank::serialise() const
{
    const std::size_t perSlot = 1 + 3 * sizeof (float) + sizeof (float) * LogGrid::kSize;
    std::vector<std::uint8_t> out (10 + kSlots * perSlot);

    out[0] = 'G'; out[1] = 'B'; out[2] = 'N'; out[3] = 'K';

    const std::uint16_t v = kBankVersion;
    const std::uint16_t n = kSlots;
    const std::uint16_t g = std::uint16_t (LogGrid::kSize);
    std::memcpy (out.data() + 4, &v, 2);
    std::memcpy (out.data() + 6, &n, 2);
    std::memcpy (out.data() + 8, &g, 2);

    std::size_t at = 10;

    for (const auto& s : slots)
    {
        out[at++] = s.used ? 1 : 0;
        const float macros[3] = { s.tilt, s.smooth, s.shift };
        std::memcpy (out.data() + at, macros, sizeof (macros));
        at += sizeof (macros);
        std::memcpy (out.data() + at, s.curve.data(), sizeof (float) * LogGrid::kSize);
        at += sizeof (float) * LogGrid::kSize;
    }

    return out;
}

bool PresetBank::deserialise (const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < 10)
        return false;

    if (data[0] != 'G' || data[1] != 'B' || data[2] != 'N' || data[3] != 'K')
        return false;

    std::uint16_t v = 0, n = 0, g = 0;
    std::memcpy (&v, data + 4, 2);
    std::memcpy (&n, data + 6, 2);
    std::memcpy (&g, data + 8, 2);

    if (v > kBankVersion || g == 0)
        return false;

    const std::size_t perSlot = 1 + 3 * sizeof (float) + sizeof (float) * std::size_t (g);

    if (size < 10 + std::size_t (n) * perSlot)
        return false;

    std::size_t at = 10;

    for (int i = 0; i < std::min (int (n), kSlots); ++i)
    {
        Slot s;
        s.used = data[at++] != 0;

        float macros[3] {};
        std::memcpy (macros, data + at, sizeof (macros));
        at += sizeof (macros);
        s.tilt = macros[0]; s.smooth = macros[1]; s.shift = macros[2];

        std::vector<float> stored (g);
        std::memcpy (stored.data(), data + at, sizeof (float) * std::size_t (g));
        at += sizeof (float) * std::size_t (g);

        for (int k = 0; k < LogGrid::kSize; ++k)
        {
            const float pos = float (k) * float (g - 1) / float (LogGrid::kSize - 1);
            const int   j0  = std::clamp (int (pos), 0, int (g) - 1);
            const int   j1  = std::min (j0 + 1, int (g) - 1);
            const float val = stored[std::size_t (j0)]
                            + (pos - float (j0)) * (stored[std::size_t (j1)] - stored[std::size_t (j0)]);
            s.curve[std::size_t (k)] = std::isfinite (val) ? LogGrid::clampDb (val) : 0.0f;
        }

        slots[std::size_t (i)] = s;
    }

    return true;
}

} // namespace draweq

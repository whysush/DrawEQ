#pragma once

#include "CurveSnapshot.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace draweq
{

/**
    Eight slots, saved with the project.

    A slot holds a *curve*, not a band list. That distinction matters: Analog
    mode re-fits on recall, so a preset made today stays valid - and gets
    better - if the fitter improves (CONTEXT.md 8.3).
*/
class PresetBank
{
public:
    static constexpr int kSlots = 8;
    static constexpr std::uint16_t kBankVersion = 1;

    struct Slot
    {
        CurveArray curve {};
        float tilt = 0.0f, smooth = 15.0f, shift = 0.0f;
        bool  used = false;
        std::string name;
    };

    PresetBank();

    void store (int slot, const CurveArray& curve, float tilt, float smooth, float shift,
                const std::string& name = {});

    bool recall (int slot, CurveArray& curve, float& tilt, float& smooth, float& shift) const;

    void clear (int slot);

    const Slot& operator[] (int slot) const { return slots[std::size_t (clampSlot (slot))]; }

    bool isUsed (int slot) const { return slots[std::size_t (clampSlot (slot))].used; }

    /** Curve for the morph target end. An empty slot is flat, which makes
        morphing toward an unused slot a fade to no EQ rather than a silent
        no-op the user cannot explain. */
    const CurveArray& curveFor (int slot) const { return slots[std::size_t (clampSlot (slot))].curve; }

    std::vector<std::uint8_t> serialise() const;
    bool deserialise (const std::uint8_t* data, std::size_t size);

private:
    static int clampSlot (int s) { return s < 0 ? 0 : (s >= kSlots ? kSlots - 1 : s); }

    std::array<Slot, kSlots> slots {};
};

} // namespace draweq

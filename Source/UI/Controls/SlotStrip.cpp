#include "SlotStrip.h"
#include "../../Processor.h"

namespace graphite
{

SlotStrip::SlotStrip (GraphiteProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (true);
    startTimerHz (10);
}

SlotStrip::~SlotStrip()
{
    stopTimer();
}

void SlotStrip::timerCallback()
{
    repaint();
}

juce::Rectangle<float> SlotStrip::boundsForSlot (int index) const
{
    // The leading cell is the "SLOT" caption, so the eight buttons divide what
    // is left rather than the whole width.
    const float labelWidth = 34.0f;
    const float w = (float (getWidth()) - labelWidth) / float (PresetBank::kSlots);
    return juce::Rectangle<float> (labelWidth + float (index) * w, 0.0f, w, float (getHeight()))
               .reduced (1.0f, 1.0f);
}

int SlotStrip::slotAt (juce::Point<float> p) const
{
    for (int i = 0; i < PresetBank::kSlots; ++i)
        if (boundsForSlot (i).contains (p))
            return i;

    return -1;
}

void SlotStrip::paint (juce::Graphics& g)
{
    auto& state = processor.apvts;
    const int slotA = int (state.getRawParameterValue (params::id::morphA)->load());
    const int slotB = int (state.getRawParameterValue (params::id::morphB)->load());

    Theme::drawTrackedLabel (g, "Slot", getLocalBounds().withWidth (32),
                             Theme::Colour::of (Theme::Colour::textMid),
                             juce::Justification::centredLeft);

    for (int i = 0; i < PresetBank::kSlots; ++i)
    {
        const auto area = boundsForSlot (i);
        const bool isA = (i + 1) == slotA;
        const bool isB = (i + 1) == slotB;
        const bool used = processor.bank().isUsed (i);

        // A is the slot you draw into; B is where morph is heading. A is
        // filled amber, B is outlined in it - same colour, different weight, so
        // the pair reads as one relationship rather than two unrelated states.
        g.setColour (Theme::Colour::of (isA ? Theme::Colour::accent : Theme::Colour::raised)
                         .brighter (i == hovered ? 0.10f : 0.0f));
        g.fillRect (area);

        g.setColour (isB ? Theme::Colour::of (Theme::Colour::accent)
                         : Theme::Colour::of (Theme::Colour::recessed).withAlpha (0.7f));
        g.drawRect (area, isB ? 1.0f : 1.0f);

        g.setFont (Theme::monoFont (Theme::Metrics::smallSize));
        g.setColour (Theme::Colour::of (isA || used || isB ? Theme::Colour::textHi
                                                           : Theme::Colour::textLo));
        g.drawText (juce::String (i + 1), area.toNearestInt(), juce::Justification::centred);

        if (i == focused && hasKeyboardFocus (false))
        {
            g.setColour (Theme::Colour::of (Theme::Colour::focus));
            g.drawRect (area.expanded (1.0f), Theme::Metrics::focusRing);
        }
    }
}

void SlotStrip::mouseDown (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.position);

    if (slot < 0)
        return;

    grabKeyboardFocus();
    focused = slot;

    auto set = [this] (const char* id, int value)
    {
        if (auto* p = processor.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (float (value)));
            p->endChangeGesture();
        }
    };

    if (e.mods.isAltDown())
        processor.clearSlot (slot + 1);
    else if (e.mods.isShiftDown())
        processor.storeCurrentIntoSlot (slot + 1);
    else if (e.mods.isRightButtonDown())
        set (params::id::morphB, slot + 1);
    else
        set (params::id::morphA, slot + 1);

    repaint();
}

void SlotStrip::mouseMove (const juce::MouseEvent& e)
{
    hovered = slotAt (e.position);
    repaint();
}

void SlotStrip::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

bool SlotStrip::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::leftKey))  { focused = juce::jmax (0, focused - 1); repaint(); return true; }
    if (key.isKeyCode (juce::KeyPress::rightKey)) { focused = juce::jmin (PresetBank::kSlots - 1, focused + 1); repaint(); return true; }

    if (key.isKeyCode (juce::KeyPress::returnKey) || key.isKeyCode (juce::KeyPress::spaceKey))
    {
        if (auto* p = processor.apvts.getParameter (params::id::morphA))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (float (focused + 1)));
            p->endChangeGesture();
        }

        return true;
    }

    return false;
}

} // namespace graphite

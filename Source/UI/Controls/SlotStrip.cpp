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
    const float w = float (getWidth()) / float (PresetBank::kSlots);
    return juce::Rectangle<float> (float (index) * w, 0.0f, w, float (getHeight())).reduced (2.0f);
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

    for (int i = 0; i < PresetBank::kSlots; ++i)
    {
        const auto area = boundsForSlot (i);
        const bool isA = (i + 1) == slotA;
        const bool isB = (i + 1) == slotB;
        const bool used = processor.bank().isUsed (i);

        g.setColour (Theme::Colour::of (isA ? Theme::Colour::raised : Theme::Colour::panel));
        g.fillRoundedRectangle (area, 3.0f);

        // A is the slot you are drawing into, B is where morph is heading.
        // Different colours because they are different kinds of thing.
        g.setColour (isA ? Theme::Colour::of (Theme::Colour::plot)
                   : isB ? Theme::Colour::of (Theme::Colour::focus)
                         : Theme::Colour::of (Theme::Colour::hairline));
        g.drawRoundedRectangle (area, 3.0f, isA || isB ? 1.5f : 1.0f);

        g.setFont (Theme::monoFont (Theme::Metrics::smallSize));
        g.setColour (Theme::Colour::of (used || isA ? Theme::Colour::textHi : Theme::Colour::textLo));
        g.drawText (juce::String (i + 1), area, juce::Justification::centred);

        if (i == hovered)
        {
            g.setColour (Theme::Colour::of (Theme::Colour::focus).withAlpha (0.15f));
            g.fillRoundedRectangle (area, 3.0f);
        }

        if (i == focused && hasKeyboardFocus (false))
        {
            g.setColour (Theme::Colour::of (Theme::Colour::focus));
            g.drawRoundedRectangle (area.expanded (1.0f), 3.0f, Theme::Metrics::focusRing);
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

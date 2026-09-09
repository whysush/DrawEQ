/**
    Renders an animated sequence of editor frames to a folder of PNGs, which
    Tools/make_gifs.py then assembles into a GIF.

    This is RenderUI's sibling and exists for the same reason - to show the
    interface without a host, a window or an audio device - but a still cannot
    show the thing DrawEQ actually does, which is respond while you draw.

        DrawEQFrames <output-dir> [sketch|notch]

    Two decisions are worth knowing about.

    The gesture is delivered as real mouse events through CurveCanvas rather
    than by poking CurveModel directly. That costs a little setup and buys a
    lot: the brush, the tool state, the ghost line and the crosshair readout
    all behave exactly as they do under a hand, so the recording cannot drift
    away from the product.

    And the waypoints are in canvas pixels, not hertz and decibels. The canvas
    converts pixels to frequency itself, so driving it in pixels and drawing
    the cursor at those same pixels keeps the two registered by construction -
    there is no second copy of the mapping to disagree with the first.

    Pink noise runs throughout, because a spectrum drawn for the picture would
    be a lie and the analyser is calibrated to read pink as flat.
*/

#include "../Source/Processor.h"
#include "../Source/UI/Controls/ToolIcons.h"
#include "../Source/UI/CurveCanvas.h"
#include "../Source/UI/Editor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace
{
/** A gesture waypoint in canvas-relative coordinates, both 0..1. y = 0.5 is
    the zero-decibel line, so 0.25 is a lift and 0.75 a cut. */
struct Way { float x, y; };

/** A broad tonal shape: low lift, boxy-midrange scoop, air shelf. The curve
    somebody actually reaches for, rather than one chosen to look impressive. */
const std::vector<Way> kSketch
{
    { 0.02f, 0.50f }, { 0.08f, 0.36f }, { 0.14f, 0.31f }, { 0.21f, 0.34f },
    { 0.28f, 0.42f }, { 0.35f, 0.53f }, { 0.42f, 0.63f }, { 0.49f, 0.69f },
    { 0.55f, 0.71f }, { 0.62f, 0.65f }, { 0.69f, 0.56f }, { 0.75f, 0.47f },
    { 0.82f, 0.40f }, { 0.89f, 0.35f }, { 0.95f, 0.32f }, { 0.99f, 0.33f },
};

/** One narrow, deliberate cut. The same pencil, used surgically. */
const std::vector<Way> kNotch
{
    { 0.44f, 0.50f }, { 0.48f, 0.56f }, { 0.51f, 0.70f }, { 0.525f, 0.84f },
    { 0.535f, 0.90f }, { 0.545f, 0.84f }, { 0.56f, 0.70f }, { 0.60f, 0.56f },
    { 0.64f, 0.50f },
};

juce::MouseEvent eventAt (juce::Component& canvas, juce::Point<float> p, bool dragged)
{
    return { juce::Desktop::getInstance().getMainMouseSource(),
             p,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
             &canvas, &canvas,
             juce::Time::getCurrentTime(),
             p, juce::Time::getCurrentTime(),
             1, dragged };
}

draweq::CurveCanvas* findCanvas (juce::Component& c)
{
    if (auto* found = dynamic_cast<draweq::CurveCanvas*> (&c))
        return found;

    for (auto* child : c.getChildren())
        if (auto* found = findCanvas (*child))
            return found;

    return nullptr;
}

/** The pencil from the toolbar, drawn over the snapshot at the point the
    gesture has reached. There is no operating-system cursor in a headless
    render, and the tool the recording is demonstrating ought to be visible. */
void drawCursor (juce::Image& image, juce::Point<float> pos, float size)
{
    using namespace draweq::icons;

    juce::Graphics g (image);

    // The unit-box pencil has its tip at (0.150, 0.850); line that up with the
    // point being drawn so the graphite sits on the stroke, not beside it.
    const auto place = juce::AffineTransform::scale (size)
                           .translated (pos.x - 0.150f * size,
                                        pos.y - 0.850f * size);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (pencil(), place.translated (2.0f, 3.0f));

    g.setColour (juce::Colour (0xffe8b23f));
    g.fillPath (pencil(), place);

    g.setColour (juce::Colour (0xffd6d9de));
    g.fillPath (pencilFerrule(), place);

    g.setColour (juce::Colour (0xff1b1d21));
    g.fillPath (pencilTip(), place);

    g.setColour (juce::Colour (0xff15171a));
    g.strokePath (pencil(), juce::PathStrokeType (1.6f), place);
    g.strokePath (pencilShoulder(), juce::PathStrokeType (1.2f), place);
}

/** Runs audio and pumps the message loop together. The worker fits on its own
    thread and the canvas repaints on a timer, so a frame taken without letting
    both run shows a plot line lagging the stroke by however long we failed to
    wait. */
void settle (draweq::DrawEQProcessor& processor, int milliseconds)
{
    constexpr int blockSize = 128;      // exactly what prepareToPlay was given
    juce::AudioBuffer<float> block (2, blockSize);
    juce::MidiBuffer midi;

    const int blocks = juce::jmax (1, (milliseconds * 48000) / (1000 * blockSize));

    for (int i = 0; i < blocks; ++i)
    {
        block.clear();
        processor.processBlock (block, midi);
    }

    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

bool writeFrame (juce::AudioProcessorEditor& editor, const juce::File& dir, int index,
                 const juce::Point<float>* cursorInEditor)
{
    auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true);

    if (cursorInEditor != nullptr)
        drawCursor (image, *cursorInEditor, 34.0f);

    auto file = dir.getChildFile (juce::String::formatted ("frame-%03d.png", index));
    file.deleteFile();

    if (auto stream = file.createOutputStream())
    {
        juce::PNGImageFormat png;
        return png.writeImageToStream (image, *stream);
    }

    return false;
}
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String outputDir = argc > 1 ? argv[1] : "frames";
    const juce::String which     = argc > 2 ? argv[2] : "sketch";

    juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (outputDir));
    dir.createDirectory();

    draweq::DrawEQProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    if (auto* tone = processor.apvts.getParameter (draweq::params::id::testTone))
        tone->setValueNotifyingHost (2.0f / 3.0f);               // Pink

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    editor->setSize (draweq::Theme::Metrics::defaultWidth,
                     draweq::Theme::Metrics::defaultHeight);

    auto* canvas = findCanvas (*editor);

    if (canvas == nullptr)
    {
        std::fprintf (stderr, "no curve canvas in the editor\n");
        return 1;
    }

    const auto& path = which == "notch" ? kNotch : kSketch;
    const auto bounds = canvas->getLocalBounds().toFloat();

    auto pixelFor = [&bounds] (Way w)
    {
        return juce::Point<float> (bounds.getX() + w.x * bounds.getWidth(),
                                   bounds.getY() + w.y * bounds.getHeight());
    };

    auto inEditor = [&editor, canvas] (juce::Point<float> p)
    {
        return editor->getLocalPoint (canvas, p);
    };

    // Let the analyser fill before the first frame, so the recording does not
    // open on an empty spectrum climbing into view.
    settle (processor, 700);

    int frame = 0;

    // Approach: the pencil travels to the start before anything is drawn, so
    // the first thing a viewer sees is the tool arriving, not a line appearing
    // from nowhere.
    {
        const auto start = pixelFor (path.front());
        const juce::Point<float> from (start.x - 90.0f, start.y - 70.0f);

        for (int i = 0; i <= 5; ++i)
        {
            const auto p = from + (start - from) * (float (i) / 5.0f);
            canvas->mouseMove (eventAt (*canvas, p, false));
            settle (processor, 60);
            const auto cursor = inEditor (p);
            writeFrame (*editor, dir, frame++, &cursor);
        }
    }

    // The stroke. Waypoints are interpolated so the pencil moves smoothly
    // rather than teleporting between control points, and the gesture stays
    // open throughout exactly as it would under a held button.
    canvas->mouseDown (eventAt (*canvas, pixelFor (path.front()), false));

    constexpr int stepsPerLeg = 3;

    for (size_t i = 1; i < path.size(); ++i)
    {
        const auto a = pixelFor (path[i - 1]);
        const auto b = pixelFor (path[i]);

        for (int s = 1; s <= stepsPerLeg; ++s)
        {
            const auto p = a + (b - a) * (float (s) / float (stepsPerLeg));
            canvas->mouseDrag (eventAt (*canvas, p, true));
            settle (processor, 55);
            const auto cursor = inEditor (p);
            writeFrame (*editor, dir, frame++, &cursor);
        }
    }

    // Release. This is the moment the fit happens, so it gets held on.
    const auto end = pixelFor (path.back());
    canvas->mouseUp (eventAt (*canvas, end, true));

    for (int i = 0; i < 5; ++i)
    {
        settle (processor, 110);
        const auto cursor = inEditor (end);
        writeFrame (*editor, dir, frame++, &cursor);
    }

    // Take the pointer off the canvas rather than just ceasing to draw it.
    // The crosshair readout belongs to the mouse being there, so leaving it
    // behind with no cursor under it looks like a rendering bug.
    canvas->mouseExit (eventAt (*canvas, end, false));

    for (int i = 0; i < 8; ++i)
    {
        settle (processor, 110);
        writeFrame (*editor, dir, frame++, nullptr);
    }

    if (auto* tone = processor.apvts.getParameter (draweq::params::id::testTone))
        tone->setValueNotifyingHost (0.0f);

    processor.releaseResources();
    std::printf ("wrote %d frames to %s\n", frame, dir.getFullPathName().toRawUTF8());
    return 0;
}

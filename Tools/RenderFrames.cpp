/**
    Renders an animated sequence of editor frames to a folder of PNGs, which
    Tools/make_gifs.py then assembles into a GIF.

    This is RenderUI's sibling, and it exists for the same reason: to show the
    interface without a host, a window or an audio device. The difference is
    that a still cannot show the thing DrawEQ actually does, which is respond
    while you draw. So this drives the model a step at a time and snapshots
    between steps.

        DrawEQFrames <output-dir> [draw|morph]

    Pink noise runs throughout, because a spectrum drawn for the picture would
    be a lie and the analyser is calibrated to read pink as flat.
*/

#include "../Source/Processor.h"
#include "../Source/UI/Editor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace
{
struct Point { float hz, db; };

/** The curve the "draw" animation traces: a low lift, a boxy-midrange scoop
    and an air shelf - the shape someone actually reaches for, rather than one
    chosen to look impressive. */
const std::vector<Point> kStroke
{
    {   20.0f,  0.0f }, {   32.0f,  4.5f }, {   48.0f,  7.5f }, {   72.0f,  6.5f },
    {  110.0f,  4.0f }, {  170.0f,  1.0f }, {  260.0f, -2.0f }, {  380.0f, -5.5f },
    {  520.0f, -8.0f }, {  700.0f, -9.5f }, {  900.0f, -9.0f }, { 1200.0f, -6.5f },
    { 1800.0f, -3.0f }, { 2600.0f,  0.0f }, { 3600.0f,  2.5f }, { 5000.0f,  4.5f },
    { 7000.0f,  6.0f }, { 9500.0f,  7.0f }, { 13000.0f, 8.0f }, { 20000.0f, 7.5f },
};

/** Runs audio and pumps the message loop together. The worker fits on its own
    thread and the canvas picks the result up on a timer, so a frame taken
    without letting both run shows a plot line lagging the stroke by however
    long we failed to wait. */
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

bool writeFrame (juce::AudioProcessorEditor& editor, const juce::File& dir, int index)
{
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true);
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
    const juce::String which     = argc > 2 ? argv[2] : "draw";

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

    // Let the analyser fill before the first frame, so the GIF does not open
    // on an empty spectrum climbing into view.
    settle (processor, 700);

    int frame = 0;
    auto& model = processor.curve();

    if (which == "morph")
    {
        // Draw the curve up front, then sweep morph. Morph is continuous at
        // zero, which is what makes a drawing automatable, and a sweep is the
        // only honest way to show that.
        model.beginGesture();
        model.startStroke (kStroke.front().hz, kStroke.front().db);

        for (size_t i = 1; i < kStroke.size(); ++i)
            model.strokeTo (kStroke[i].hz, kStroke[i].db, 0.5f, 1.0f,
                            draweq::CurveModel::Brush::draw);

        model.endGesture();
        settle (processor, 400);

        auto* morph = processor.apvts.getParameter (draweq::params::id::morph);

        if (morph == nullptr)
        {
            std::fprintf (stderr, "no morph parameter\n");
            return 1;
        }

        constexpr int steps = 26;

        for (int i = 0; i <= steps; ++i)          // flat -> drawn
        {
            morph->setValueNotifyingHost (float (i) / float (steps));
            settle (processor, 90);
            writeFrame (*editor, dir, frame++);
        }

        for (int i = steps - 1; i >= 0; --i)      // and back
        {
            morph->setValueNotifyingHost (float (i) / float (steps));
            settle (processor, 90);
            writeFrame (*editor, dir, frame++);
        }
    }
    else
    {
        // One stroke, one frame at a time. The gesture stays open across the
        // whole animation exactly as it would under a held mouse button, so
        // the ghost line and the fit behave as they do in use.
        settle (processor, 200);
        writeFrame (*editor, dir, frame++);       // a beat on the flat curve

        model.beginGesture();
        model.startStroke (kStroke.front().hz, kStroke.front().db);

        for (size_t i = 1; i < kStroke.size(); ++i)
        {
            model.strokeTo (kStroke[i].hz, kStroke[i].db, 0.5f, 1.0f,
                            draweq::CurveModel::Brush::draw);
            settle (processor, 110);
            writeFrame (*editor, dir, frame++);
        }

        model.endGesture();

        // Hold on the finished curve. The fit continues to converge after the
        // stroke ends, and that is worth seeing rather than cutting away from.
        for (int i = 0; i < 10; ++i)
        {
            settle (processor, 120);
            writeFrame (*editor, dir, frame++);
        }
    }

    if (auto* tone = processor.apvts.getParameter (draweq::params::id::testTone))
        tone->setValueNotifyingHost (0.0f);

    processor.releaseResources();
    std::printf ("wrote %d frames to %s\n", frame, dir.getFullPathName().toRawUTF8());
    return 0;
}

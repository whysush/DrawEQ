/**
    Renders the editor to a PNG without opening a window or an audio device.

    Component::createComponentSnapshot paints into a software image, so this
    runs from a terminal, in CI, and on a machine whose speakers you would
    rather not have a plugin connect itself to. It exists so that a change to
    the interface can be looked at, and diffed, without loading a host.

        DrawEQUISnapshot <output.png> [demo|flat]
*/

#include "../Source/Processor.h"
#include "../Source/UI/Editor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
/** A stroke with something to say: a low bump, a midrange scoop, an air lift.
    `withNotch` adds one gesture too narrow for any biquad cascade, which is
    what gives the residual ribbon something to show. */
void drawDemoCurve (draweq::CurveModel& model, bool withNotch)
{
    model.beginGesture();
    model.startStroke (20.0f, 0.0f);
    model.strokeTo (55.0f,   7.5f, 0.55f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (110.0f,  4.0f, 0.55f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (260.0f, -2.0f, 0.55f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (520.0f, -8.0f, 0.45f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (900.0f, -9.5f, 0.45f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (1800.0f, -3.0f, 0.5f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (3600.0f,  2.5f, 0.5f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (7000.0f,  6.0f, 0.5f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (14000.0f, 8.0f, 0.6f, 1.0f, draweq::CurveModel::Brush::draw);
    model.strokeTo (20000.0f, 7.0f, 0.6f, 1.0f, draweq::CurveModel::Brush::draw);
    model.endGesture();

    if (! withNotch)
        return;

    model.beginGesture();
    model.startStroke (2600.0f, 0.0f);

    for (int i = 0; i < 12; ++i)
        model.strokeTo (2600.0f, -22.0f, 0.06f, 1.0f, draweq::CurveModel::Brush::draw);

    model.endGesture();
}
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String outputPath = argc > 1 ? argv[1] : "draweq-ui.png";
    const juce::String which = argc > 2 ? argv[2] : "demo";
    const bool demo   = which != "flat";
    const bool ribbon = which == "ribbon";

    draweq::DrawEQProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    if (demo)
        drawDemoCurve (processor.curve(), ribbon);



    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    const bool small = which == "small";

    editor->setSize (small ? int (draweq::Theme::Metrics::defaultWidth * 0.75)
                           : draweq::Theme::Metrics::defaultWidth,
                     small ? int (draweq::Theme::Metrics::defaultHeight * 0.75)
                           : draweq::Theme::Metrics::defaultHeight);

    // Let the worker fit the curve and the canvas timer pick the result up;
    // without this the plot line has nothing to draw yet.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);

    // Then actually run audio through it, so the spectrum in the picture is
    // one the plugin measured rather than one drawn for the picture. Pink,
    // because that is what the analyser is calibrated to read flat.
    if (which != "silent")
    {
        if (auto* toneParam = processor.apvts.getParameter (draweq::params::id::testTone))
            toneParam->setValueNotifyingHost (2.0f / 3.0f);      // Pink

        // Exactly the block size prepareToPlay was given. A host never sends
        // more than it promised and the processor sizes its scratch buffers to
        // that promise, so feeding it a larger block corrupts the heap - which
        // is precisely what this tool did on the first attempt.
        constexpr int blockSize = 128;

        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;

        for (int i = 0; i < 340; ++i)
        {
            block.clear();
            processor.processBlock (block, midi);

            if (i % 8 == 0)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
        }

        if (auto* toneParam = processor.apvts.getParameter (draweq::params::id::testTone))
            toneParam->setValueNotifyingHost (0.0f);
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true);

    juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (outputPath));
    output.deleteFile();

    if (auto stream = output.createOutputStream())
    {
        juce::PNGImageFormat png;

        if (! png.writeImageToStream (image, *stream))
        {
            std::fprintf (stderr, "failed to encode %s\n", output.getFullPathName().toRawUTF8());
            return 1;
        }
    }

    processor.releaseResources();
    std::printf ("wrote %s (%d x %d)\n", output.getFullPathName().toRawUTF8(),
                 image.getWidth(), image.getHeight());
    return 0;
}

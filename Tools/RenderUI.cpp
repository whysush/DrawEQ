/**
    Renders the editor to a PNG without opening a window or an audio device.

    Component::createComponentSnapshot paints into a software image, so this
    runs from a terminal, in CI, and on a machine whose speakers you would
    rather not have a plugin connect itself to. It exists so that a change to
    the interface can be looked at, and diffed, without loading a host.

        GraphiteUISnapshot <output.png> [demo|flat]
*/

#include "../Source/Processor.h"
#include "../Source/UI/Editor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
/** A stroke with something to say: a low bump, a midrange scoop, an air lift,
    and one notch too narrow to fit - so the residual ribbon has a reason to
    show itself. */
void drawDemoCurve (graphite::CurveModel& model)
{
    model.beginGesture();
    model.startStroke (20.0f, 0.0f);
    model.strokeTo (55.0f,   7.5f, 0.55f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (110.0f,  4.0f, 0.55f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (260.0f, -2.0f, 0.55f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (520.0f, -8.0f, 0.45f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (900.0f, -9.5f, 0.45f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (1800.0f, -3.0f, 0.5f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (3600.0f,  2.5f, 0.5f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (7000.0f,  6.0f, 0.5f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (14000.0f, 8.0f, 0.6f, 1.0f, graphite::CurveModel::Brush::draw);
    model.strokeTo (20000.0f, 7.0f, 0.6f, 1.0f, graphite::CurveModel::Brush::draw);
    model.endGesture();

    model.beginGesture();
    model.startStroke (2600.0f, 0.0f);

    for (int i = 0; i < 12; ++i)
        model.strokeTo (2600.0f, -22.0f, 0.06f, 1.0f, graphite::CurveModel::Brush::draw);

    model.endGesture();
}
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String outputPath = argc > 1 ? argv[1] : "graphite-ui.png";
    const juce::String which = argc > 2 ? argv[2] : "demo";
    const bool demo   = which != "flat";
    const bool ribbon = which == "ribbon";

    graphite::GraphiteProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    if (demo)
        drawDemoCurve (processor.curve());

    if (ribbon)
    {
        // Smoothing off, so the notch in the demo curve stays as sharp as it
        // was drawn - which no biquad cascade can follow. This is the mode that
        // shows the residual ribbon doing its job.
        if (auto* smooth = processor.apvts.getParameter (graphite::params::id::smooth))
            smooth->setValueNotifyingHost (0.0f);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    editor->setSize (graphite::Theme::Metrics::defaultWidth,
                     graphite::Theme::Metrics::defaultHeight);

    // Let the worker fit the curve and the canvas timer pick the result up;
    // without this the plot line has nothing to draw yet.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);

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

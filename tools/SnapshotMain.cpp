#include <juce_gui_extra/juce_gui_extra.h>
#include "../source/PluginProcessor.h"
#include "../source/PluginEditor.h"

namespace
{
    void renderToPNG (const juce::File& output, int w, int h)
    {
        deanamp::DeanAmpProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (w, h);
        // Force layout
        ed->setBounds (0, 0, w, h);

        juce::Image img (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (img);
            ed->paintEntireComponent (g, false);
        }

        juce::PNGImageFormat png;
        if (auto stream = output.createOutputStream())
        {
            png.writeImageToStream (img, *stream);
            std::cout << "Wrote " << output.getFullPathName() << " (" << w << "x" << h << ")\n";
        }
        else
        {
            std::cerr << "Could not open " << output.getFullPathName() << " for writing\n";
        }
    }
}

class SnapshotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Dean Amp Snapshot"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise (const juce::String& cli) override
    {
        auto args = juce::JUCEApplication::getCommandLineParameterArray();
        juce::String path = "/tmp/deanamp/ui.png";
        int w = 920, h = 460;
        for (int i = 0; i < args.size(); ++i)
        {
            if (args[i] == "--out" && i + 1 < args.size()) path = args[++i];
            else if (args[i] == "--size" && i + 1 < args.size())
            {
                auto s = args[++i];
                w = s.upToFirstOccurrenceOf ("x", false, false).getIntValue();
                h = s.fromFirstOccurrenceOf ("x", false, false).getIntValue();
            }
        }
        juce::ignoreUnused (cli);

        juce::File out (path);
        out.getParentDirectory().createDirectory();
        renderToPNG (out, w, h);
        quit();
    }

    void shutdown() override {}
};

START_JUCE_APPLICATION (SnapshotApp)

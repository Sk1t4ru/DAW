#include <JuceHeader.h>
#include "MainComponent.h"

class DAWApplication  : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Simple DAW"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow ("Simple DAW", new MainComponent(), *this));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, juce::Component* c, JUCEApplication& app)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                        .findColour (ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons),
              owner (app)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (c, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            owner.systemRequestedQuit();
        }

    private:
        JUCEApplication& owner;
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (DAWApplication)
//123
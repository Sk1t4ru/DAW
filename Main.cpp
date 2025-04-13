#include <JuceHeader.h>
#include "MainComponent.h"

class DAWApplication : public juce::JUCEApplication
{
public:
    DAWApplication() = default;

    const juce::String getApplicationName() override { return "Simple DAW"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow(getApplicationName(), *this));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, JUCEApplication& app)
            : DocumentWindow(name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                      .findColour(juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons),
              owner(app)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            owner.systemRequestedQuit();
        }

    private:
        JUCEApplication& owner;
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(DAWApplication)
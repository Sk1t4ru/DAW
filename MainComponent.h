#pragma once

#include <JuceHeader.h>

class MainComponent  : public juce::AudioAppComponent,
                       public juce::Button::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked(juce::Button*) override;

private:
    juce::TextButton recordButton;
    juce::TextButton saveButton;
    juce::ToggleButton distortionButton;
    juce::ToggleButton delayButton;

    juce::AudioBuffer<float> recordedBuffer;
    int recordedSamples = 0;
    bool isRecording = false;

    juce::AudioBuffer<float> delayBuffer;
    int delayBufferPos = 0;

    bool useDistortion = false;
    bool useDelay = false;

    void applyEffects(juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

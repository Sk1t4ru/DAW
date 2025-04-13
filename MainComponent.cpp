#include "MainComponent.h"

MainComponent::MainComponent()
{
    addAndMakeVisible(recordButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(distortionButton);
    addAndMakeVisible(delayButton);

    recordButton.setButtonText("Record");
    saveButton.setButtonText("Save");
    distortionButton.setButtonText("Distortion");
    delayButton.setButtonText("Delay");

    recordButton.addListener(this);
    saveButton.addListener(this);
    distortionButton.addListener(this);
    delayButton.addListener(this);

    setSize(600, 400);
    setAudioChannels(2, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    recordedBuffer.setSize(2, static_cast<int>(sampleRate * 60.0));
    recordedBuffer.clear();
    recordedSamples = 0;

    delayBuffer.setSize(2, static_cast<int>(sampleRate));
    delayBuffer.clear();
    delayBufferPos = 0;
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;
    
    if (isRecording)
    {
        int remaining = recordedBuffer.getNumSamples() - recordedSamples;
        int samplesToCopy = juce::jmin(bufferToFill.numSamples, remaining);

        if (samplesToCopy > 0)
        {
            for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
            {
                recordedBuffer.copyFrom(ch, recordedSamples,
                                     *buffer, ch, bufferToFill.startSample,
                                     samplesToCopy);
            }
            recordedSamples += samplesToCopy;
        }

        if (samplesToCopy < bufferToFill.numSamples)
        {
            isRecording = false;
            recordButton.setButtonText("Record");
        }
    }

    applyEffects(*buffer);
}

void MainComponent::releaseResources()
{
    // Cleanup if needed
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey);
    drawWaveform(g);
}

void MainComponent::drawWaveform(juce::Graphics& g)
{
    g.setColour(juce::Colours::lightgrey);
    auto bounds = getLocalBounds().removeFromBottom(200).reduced(10);
    g.drawRect(bounds, 1);

    if (recordedSamples > 0)
    {
        g.setColour(juce::Colours::cyan);
        
        auto* channelData = recordedBuffer.getReadPointer(0);
        auto scale = bounds.getHeight() / 2.0f;
        auto center = bounds.getCentreY();
        auto step = static_cast<float>(recordedSamples) / bounds.getWidth();
        
        juce::Path waveformPath;
        waveformPath.startNewSubPath(bounds.getX(), center);
        
        for (int x = bounds.getX(); x < bounds.getRight(); ++x)
        {
            auto sampleIndex = juce::jmin(static_cast<int>((x - bounds.getX()) * step), recordedSamples - 1);
            auto sample = channelData[sampleIndex];
            auto y = center - sample * scale;
            waveformPath.lineTo(static_cast<float>(x), y);
        }
        
        g.strokePath(waveformPath, juce::PathStrokeType(1.0f));
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto buttonArea = area.removeFromTop(150);
    
    recordButton.setBounds(buttonArea.removeFromTop(30).reduced(5));
    saveButton.setBounds(buttonArea.removeFromTop(30).reduced(5));
    distortionButton.setBounds(buttonArea.removeFromTop(30).reduced(5));
    delayButton.setBounds(buttonArea.removeFromTop(30).reduced(5));
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &recordButton)
    {
        isRecording = !isRecording;
        recordButton.setButtonText(isRecording ? "Stop" : "Record");
        
        if (isRecording)
        {
            recordedSamples = 0;
            recordedBuffer.clear();
        }
        repaint();
    }
    else if (button == &saveButton && recordedSamples > 0)
    {
        auto chooser = std::make_unique<juce::FileChooser>("Save Audio File",
                                                         juce::File(),
                                                         "*.wav");

        auto chooserFlags = juce::FileBrowserComponent::saveMode |
                          juce::FileBrowserComponent::canSelectFiles |
                          juce::FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File())
            {
                juce::WavAudioFormat wavFormat;
                if (auto outputStream = file.createOutputStream())
                {
                    if (auto writer = wavFormat.createWriterFor(outputStream.get(),
                                                              deviceManager.getCurrentAudioDevice()->getCurrentSampleRate(),
                                                              recordedBuffer.getNumChannels(),
                                                              16, {}, 0))
                    {
                        outputStream.release(); // writer now owns the stream
                        writer->writeFromAudioSampleBuffer(recordedBuffer, 0, recordedSamples);
                        writer->flush();
                    }
                }
            }
        });
    }
    else if (button == &distortionButton)
    {
        useDistortion = distortionButton.getToggleState();
    }
    else if (button == &delayButton)
    {
        useDelay = delayButton.getToggleState();
    }
}

void MainComponent::applyEffects(juce::AudioBuffer<float>& buffer)
{
    if (useDistortion)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                data[i] = std::tanh(data[i] * 5.0f);
            }
        }
    }

    if (useDelay)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* in = buffer.getReadPointer(ch);
            auto* out = buffer.getWritePointer(ch);
            auto* delayData = delayBuffer.getWritePointer(ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float dry = in[i];
                float delayed = delayData[delayBufferPos];
                out[i] = dry + 0.5f * delayed;
                delayData[delayBufferPos] = dry + delayed * 0.5f;
                delayBufferPos = (delayBufferPos + 1) % delayBuffer.getNumSamples();
            }
        }
    }
}
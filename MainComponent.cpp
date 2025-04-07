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

    setSize(400, 200);
    setAudioChannels(2, 2); // 2 входа, 2 выхода
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    DBG("prepareToPlay: sampleRate = " << sampleRate);

    if (sampleRate <= 0)
    {
        jassertfalse;
        return;
    }

    int maxLength = (int)(sampleRate * 60.0); // максимум 60 сек записи
    recordedBuffer.setSize(2, maxLength);
    recordedBuffer.clear();
    recordedSamples = 0;

    delayBuffer.setSize(2, (int)sampleRate); // 1 секунда задержки
    delayBuffer.clear();
    delayBufferPos = 0;
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* buffer = bufferToFill.buffer;

    // Запись
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
            DBG("Recording buffer full, stopping.");
            isRecording = false;
            juce::MessageManager::callAsync([this]() {
                recordButton.setButtonText("Record");
            });
        }
    }

    // Эффекты
    applyEffects(*buffer);
}

void MainComponent::releaseResources()
{
    DBG("releaseResources");
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10).removeFromTop(150);
    recordButton.setBounds(area.removeFromTop(30).reduced(5));
    saveButton.setBounds(area.removeFromTop(30).reduced(5));
    distortionButton.setBounds(area.removeFromTop(30).reduced(5));
    delayButton.setBounds(area.removeFromTop(30).reduced(5));
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &recordButton)
    {
        isRecording = !isRecording;

        if (isRecording)
        {
            recordedSamples = 0;
            recordedBuffer.clear();
            recordButton.setButtonText("Stop");
        }
        else
        {
            recordButton.setButtonText("Record");
        }
    }
    else if (button == &saveButton)
    {
        juce::FileChooser chooser("Save WAV File", {}, "*.wav");
        chooser.launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::FileOutputStream> outputStream(file.createOutputStream());

                if (outputStream != nullptr)
                {
                    std::unique_ptr<juce::AudioFormatWriter> writer(
                        wavFormat.createWriterFor(outputStream.get(), 44100, 2, 16, {}, 0));

                    if (writer != nullptr)
                    {
                        writer->writeFromAudioSampleBuffer(recordedBuffer, 0, recordedSamples);
                        DBG("File saved: " + file.getFullPathName());
                    }

                    outputStream.release(); // передано во writer
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
                data[i] = std::tanh(data[i] * 5.0f); // простая перегрузка
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

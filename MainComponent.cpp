// MainComponent.cpp
#include "MainComponent.h"

MainComponent::MainComponent() : 
    thumbnail(512, formatManager, thumbnailCache)
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

    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);

    setSize(600, 400);
    
    // Запрашиваем разрешения на запись
    juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
                                     [this](bool granted)
                                     {
                                         int numInputChannels = granted ? 2 : 0;
                                         setAudioChannels(numInputChannels, 2);
                                     });
}

MainComponent::~MainComponent()
{
    thumbnail.removeChangeListener(this);
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    recordedBuffer.setSize(2, static_cast<int>(sampleRate * 300.0)); // 5 минут записи
    recordedBuffer.clear();
    recordedSamples = 0;

    delayBuffer.setSize(2, static_cast<int>(sampleRate * 2.0)); // 2 секунды задержки
    delayBuffer.clear();
    delayBufferPos = 0;

    thumbnail.reset(2, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* outputBuffer = bufferToFill.buffer;
    const int numSamples = bufferToFill.numSamples;
    const int startSample = bufferToFill.startSample;

    // Получаем входные данные
    auto* inputDevice = deviceManager.getCurrentAudioDevice();
    if (!inputDevice) {
        outputBuffer->clear();
        return;
    }

    const auto& inputChannels = inputDevice->getActiveInputChannels();
    const int numInputChannels = inputChannels.countNumberOfSetBits();
    // Применяем эффекты
    applyEffects(*outputBuffer);
    if (numInputChannels > 0)
    {
        // Создаем временный буфер для входных данных
        juce::AudioBuffer<float> inputBuffer(numInputChannels, numSamples);
        for (int ch = 0; ch < numInputChannels; ++ch)
        {
            if (inputChannels[ch])
            {
                inputBuffer.copyFrom(ch, 0, 
                                   *outputBuffer, 
                                   ch, 
                                   startSample, 
                                   numSamples);
            }
        }

        // Запись в буфер
        if (isRecording)
        {
            int remaining = recordedBuffer.getNumSamples() - recordedSamples;
            int samplesToCopy = juce::jmin(numSamples, remaining);

            if (samplesToCopy > 0)
            {
                for (int ch = 0; ch < recordedBuffer.getNumChannels(); ++ch)
                {
                    recordedBuffer.copyFrom(ch, recordedSamples,
                                          inputBuffer,
                                          ch % numInputChannels,
                                          0,
                                          samplesToCopy);
                }
                recordedSamples += samplesToCopy;
                
                // Обновляем thumbnail
                thumbnail.addBlock(recordedSamples - samplesToCopy, 
                                 inputBuffer, 
                                 0, 
                                 samplesToCopy);
            }

            if (samplesToCopy < numSamples)
            {
                isRecording = false;
                recordButton.setButtonText("Record");
            }
        }

        // Мониторинг - передаем входной сигнал на выход
        for (int ch = 0; ch < outputBuffer->getNumChannels(); ++ch)
        {
            outputBuffer->copyFrom(ch, startSample,
                                 inputBuffer,
                                 ch % numInputChannels,
                                 0,
                                 numSamples);
        }
    }
    else
    {
        outputBuffer->clear();
    }

    
}

void MainComponent::releaseResources()
{
    // Cleanup if needed
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey);
    
    // Рисуем waveform
    g.setColour(juce::Colours::lightgrey);
    auto bounds = getLocalBounds().removeFromBottom(200).reduced(10);
    g.drawRect(bounds, 1);
    
    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour(juce::Colours::cyan);
        thumbnail.drawChannels(g, bounds.reduced(2), 
                             0.0, thumbnail.getTotalLength(), 1.0f);
    }
    else
    {
        g.setFont(14.0f);
        g.drawFittedText("(No audio recorded)", bounds, juce::Justification::centred, 2);
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
        if (isRecording)
            stopRecording();
        else
            startRecording();
    }
    else if (button == &saveButton && recordedSamples > 0)
    {
        auto chooserFlags = juce::FileBrowserComponent::saveMode |
                          juce::FileBrowserComponent::canSelectFiles |
                          juce::FileBrowserComponent::warnAboutOverwriting;

        fileChooser = std::make_unique<juce::FileChooser>("Save Audio File",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav");

        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file != juce::File{})
            {
                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::AudioFormatWriter> writer;
                
                if (auto outputStream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
                {
                    writer.reset(wavFormat.createWriterFor(outputStream.get(),
                        deviceManager.getCurrentAudioDevice()->getCurrentSampleRate(),
                        recordedBuffer.getNumChannels(),
                        16, {}, 0));
                    
                    if (writer != nullptr)
                    {
                        outputStream.release(); // writer теперь владеет потоком
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

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
        repaint();
}

void MainComponent::startRecording()
{
    if (!juce::RuntimePermissions::isGranted(juce::RuntimePermissions::writeExternalStorage))
    {
        juce::RuntimePermissions::request(juce::RuntimePermissions::writeExternalStorage,
                                         [this](bool granted) { if (granted) startRecording(); });
        return;
    }

    lastRecording = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                      .getNonexistentChildFile("Recording", ".wav");

    recordedSamples = 0;
    recordedBuffer.clear();
    thumbnail.reset(2, deviceManager.getCurrentAudioDevice()->getCurrentSampleRate());

    isRecording = true;
    recordButton.setButtonText("Stop");
    repaint();
}

void MainComponent::stopRecording()
{
    isRecording = false;
    recordButton.setButtonText("Record");
    repaint();
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
                out[i] = dry + 0.2f * delayed;
                delayData[delayBufferPos] = dry + delayed * 0.2f;
                delayBufferPos = (delayBufferPos + 1) % delayBuffer.getNumSamples();
            }
        }
    }
}
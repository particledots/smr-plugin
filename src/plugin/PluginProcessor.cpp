#include "PluginProcessor.h"
#include "PluginEditor.h"

SmrAudioProcessor::SmrAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void SmrAudioProcessor::prepareToPlay (double, int) {}

bool SmrAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    // Milestone 0: mono->mono or stereo->stereo only.
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void SmrAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // In place: the host gives one buffer for input and output. The core copies only if pointers differ.
    const int numChannels = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const float* const* readPtrs = buffer.getArrayOfReadPointers();
    float* const* writePtrs = buffer.getArrayOfWritePointers();

    core.process (readPtrs, writePtrs, numChannels, buffer.getNumSamples());

    for (int ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SmrAudioProcessor::createEditor()
{
    return new SmrAudioProcessorEditor (*this);
}

// Entry point JUCE looks for in every plugin format.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmrAudioProcessor();
}

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr float maxDelaySeconds = 4.0f;
constexpr float maxReverbSeconds = 6.0f;

float softClip (float x)
{
    return std::tanh (x);
}

float equalPowerDry (float mix)
{
    return std::cos (juce::MathConstants<float>::halfPi * mix);
}

float equalPowerWet (float mix)
{
    return std::sin (juce::MathConstants<float>::halfPi * mix);
}
}

ResamplingDelayAudioProcessor::ResamplingDelayAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    mode = state.getRawParameterValue ("mode");
    delayMs = state.getRawParameterValue ("delayMs");
    feedback = state.getRawParameterValue ("feedback");
    reverbTime = state.getRawParameterValue ("reverbTime");
    mix = state.getRawParameterValue ("mix");
    lofi = state.getRawParameterValue ("lofi");
    tone = state.getRawParameterValue ("tone");
}

ResamplingDelayAudioProcessor::APVTS::ParameterLayout ResamplingDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "mode", "Mode", juce::StringArray { "Delay", "Reverb" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayMs", "Echo Time", juce::NormalisableRange<float> (35.0f, 1800.0f, 0.01f, 0.42f), 320.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "feedback", "Repeats", juce::NormalisableRange<float> (0.0f, 0.96f, 0.001f), 0.48f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverbTime", "Reverb Decay", juce::NormalisableRange<float> (0.35f, maxReverbSeconds, 0.001f, 0.55f), 2.4f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Dry/Wet", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.42f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lofi", "LoFi Wear", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "tone", "Tone", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.38f));

    return { params.begin(), params.end() };
}

void ResamplingDelayAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    delayLine.prepare (sampleRate, 2, maxDelaySeconds);
    crusher.prepare (2);

    const std::array<float, 6> baseTimes { 0.0437f, 0.0611f, 0.0833f, 0.1179f, 0.1731f, 0.2297f };

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto channelIndex = static_cast<size_t> (channel);

        for (size_t i = 0; i < reverbLines[channelIndex].size(); ++i)
        {
            auto& line = reverbLines[channelIndex][i];
            line.baseDelaySeconds = baseTimes[i] * (channel == 0 ? 1.0f : 1.071f);
            line.delay.prepare (sampleRate, 1, 1.2);
        }
    }

    toneStates = {};
    wowPhase = 0.0f;
    smoothedDelayMs = delayMs->load();
    smoothedReverbTime = reverbTime->load();
}

void ResamplingDelayAudioProcessor::releaseResources()
{
    delayLine.clear();
    crusher.reset();
}

bool ResamplingDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void ResamplingDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    updateSmoothedValues();

    if (mode->load() < 0.5f)
        processDelay (buffer);
    else
        processReverb (buffer);
}

void ResamplingDelayAudioProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto mixValue = mix->load();
    const auto wetGain = equalPowerWet (mixValue);
    const auto dryGain = equalPowerDry (mixValue);
    const auto fb = feedback->load();
    const auto lofiAmount = lofi->load();
    const auto wowDepthSamples = juce::jmap (lofiAmount, 1.0f, 48.0f);
    const auto wowRate = juce::jmap (lofiAmount, 0.08f, 0.75f);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto wow = std::sin (wowPhase) * wowDepthSamples
                       + std::sin (wowPhase * 2.73f) * wowDepthSamples * 0.21f;
        wowPhase += juce::MathConstants<float>::twoPi * wowRate / static_cast<float> (currentSampleRate);
        if (wowPhase > juce::MathConstants<float>::twoPi)
            wowPhase -= juce::MathConstants<float>::twoPi;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto dry = data[sample];
            const auto channelOffset = channel == 0 ? 0.0f : 11.0f;
            const auto delaySamples = juce::jlimit (1.0f,
                                                    maxDelaySeconds * static_cast<float> (currentSampleRate) - 4.0f,
                                                    smoothedDelayMs * 0.001f * static_cast<float> (currentSampleRate)
                                                        + wow + channelOffset);

            auto wet = delayLine.read (channel, delaySamples);
            wet = toneFilter (channel, wet, tone->load());
            wet = crush (channel, wet);

            const auto intoDelay = softClip (dry + wet * fb * juce::jmap (lofiAmount, 0.9f, 1.35f));
            delayLine.write (channel, intoDelay);
            data[sample] = softClip (dry * dryGain + wet * wetGain);
        }

        delayLine.advance();
    }
}

void ResamplingDelayAudioProcessor::processReverb (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto mixValue = mix->load();
    const auto wetGain = equalPowerWet (mixValue);
    const auto dryGain = equalPowerDry (mixValue);
    const auto lofiAmount = lofi->load();
    const auto sizeScale = juce::jmap (smoothedReverbTime, 0.35f, maxReverbSeconds, 0.52f, 2.75f);
    const auto regen = juce::jlimit (0.2f, 0.94f, 0.38f + smoothedReverbTime * 0.082f + lofiAmount * 0.08f);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto leftDry = buffer.getReadPointer (0)[sample];
        const auto rightDry = numChannels > 1 ? buffer.getReadPointer (1)[sample] : leftDry;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto channelIndex = static_cast<size_t> (channel);
            auto* data = buffer.getWritePointer (channel);
            const auto dry = channel == 0 ? leftDry : rightDry;
            const auto opposite = channel == 0 ? rightDry : leftDry;
            const auto input = (dry * 0.72f + opposite * 0.28f) * 0.42f;
            float tank = 0.0f;

            for (size_t i = 0; i < reverbLines[channelIndex].size(); ++i)
            {
                auto& line = reverbLines[channelIndex][i];
                const auto smear = std::sin (wowPhase * (0.31f + 0.07f * static_cast<float> (i)) + phaseOffsets[channelIndex])
                                 * juce::jmap (lofiAmount, 3.0f, 85.0f);
                const auto delaySamples = juce::jlimit (8.0f,
                                                        1.15f * static_cast<float> (currentSampleRate),
                                                        line.baseDelaySeconds * sizeScale * static_cast<float> (currentSampleRate) + smear);

                const auto tap = line.delay.read (0, delaySamples);
                const auto diffused = softClip (input + tap * regen - tank * 0.045f);
                const auto degraded = crush (channel, toneFilter (channel, diffused, tone->load()));
                line.delay.write (0, degraded);
                line.delay.advance();
                tank += tap * (i % 2 == 0 ? 1.0f : -0.72f);
            }

            tank *= 0.23f;
            data[sample] = softClip (dry * dryGain + tank * wetGain * 1.65f);
        }

        wowPhase += juce::MathConstants<float>::twoPi * juce::jmap (lofiAmount, 0.05f, 0.62f)
                    / static_cast<float> (currentSampleRate);
        if (wowPhase > juce::MathConstants<float>::twoPi)
            wowPhase -= juce::MathConstants<float>::twoPi;
    }
}

float ResamplingDelayAudioProcessor::toneFilter (int channel, float input, float toneValue)
{
    const auto cutoff = juce::jmap (toneValue, 550.0f, 9000.0f);
    const auto coefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / static_cast<float> (currentSampleRate));
    auto& stateValue = toneStates[static_cast<size_t> (juce::jlimit (0, 1, channel))];
    stateValue += coefficient * (input - stateValue);
    return stateValue;
}

void ResamplingDelayAudioProcessor::updateSmoothedValues()
{
    const auto delayCoefficient = 1.0f - std::exp (-1.0f / (0.075f * static_cast<float> (currentSampleRate)));
    const auto reverbCoefficient = 1.0f - std::exp (-1.0f / (0.18f * static_cast<float> (currentSampleRate)));
    smoothedDelayMs += delayCoefficient * (delayMs->load() - smoothedDelayMs);
    smoothedReverbTime += reverbCoefficient * (reverbTime->load() - smoothedReverbTime);
}

void ResamplingDelayAudioProcessor::VariableDelay::prepare (double newSampleRate, int channels, double maxDelaySecondsToUse)
{
    const auto length = static_cast<int> (std::ceil (newSampleRate * maxDelaySecondsToUse)) + 8;
    buffer.setSize (channels, length);
    clear();
}

void ResamplingDelayAudioProcessor::VariableDelay::clear()
{
    buffer.clear();
    writePosition = 0;
}

float ResamplingDelayAudioProcessor::VariableDelay::read (int channel, float delaySamples) const
{
    const auto size = buffer.getNumSamples();
    auto readPosition = static_cast<float> (writePosition) - delaySamples;

    while (readPosition < 0.0f)
        readPosition += static_cast<float> (size);

    const auto index0 = static_cast<int> (readPosition) % size;
    const auto index1 = (index0 + 1) % size;
    const auto fraction = readPosition - static_cast<float> (index0);
    return juce::jmap (fraction, buffer.getSample (channel, index0), buffer.getSample (channel, index1));
}

void ResamplingDelayAudioProcessor::VariableDelay::write (int channel, float sample)
{
    buffer.setSample (channel, writePosition, sample);
}

void ResamplingDelayAudioProcessor::VariableDelay::advance()
{
    writePosition = (writePosition + 1) % buffer.getNumSamples();
}

void ResamplingDelayAudioProcessor::SampleCrusher::prepare (int channels)
{
    counters.assign (static_cast<size_t> (channels), 0);
    heldSamples.assign (static_cast<size_t> (channels), 0.0f);
}

void ResamplingDelayAudioProcessor::SampleCrusher::reset()
{
    std::fill (counters.begin(), counters.end(), 0);
    std::fill (heldSamples.begin(), heldSamples.end(), 0.0f);
}

float ResamplingDelayAudioProcessor::SampleCrusher::process (int channel, float input, float amount)
{
    const auto index = static_cast<size_t> (juce::jlimit (0, static_cast<int> (heldSamples.size()) - 1, channel));
    const auto holdLength = juce::jlimit (1, 34, static_cast<int> (1.0f + amount * amount * 33.0f));

    if (counters[index] <= 0)
    {
        heldSamples[index] = input;
        counters[index] = holdLength;
    }

    --counters[index];

    const auto bits = juce::jmap (amount, 14.0f, 5.0f);
    const auto steps = std::pow (2.0f, std::round (bits));
    const auto crushed = std::round (heldSamples[index] * steps) / steps;
    return input + (crushed - input) * amount;
}

void ResamplingDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ResamplingDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ResamplingDelayAudioProcessor::createEditor()
{
    return new ResamplingDelayAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResamplingDelayAudioProcessor();
}

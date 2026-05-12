#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr float maxDelaySeconds = 4.0f;
constexpr float maxReverbSeconds = 6.0f;
constexpr std::array<const char*, 6> parameterIds {
    "delayMs", "feedback", "reverbTime", "mix", "lofi", "tone"
};

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

float parseTimeToSeconds (const juce::String& token)
{
    const auto text = token.trim().toLowerCase();
    if (text.endsWith ("ms"))
        return text.dropLastCharacters (2).getFloatValue() * 0.001f;
    if (text.endsWithChar ('s'))
        return text.dropLastCharacters (1).getFloatValue();

    return text.getFloatValue();
}

float parseLevel (const juce::String& token)
{
    const auto text = token.trim();
    if (text.endsWithChar ('%'))
        return juce::jlimit (0.0f, 1.0f, text.dropLastCharacters (1).getFloatValue() * 0.01f);

    return juce::jlimit (0.0f, 1.0f, text.getFloatValue());
}

bool looksLikeNumberOrPercent (const juce::String& token)
{
    const auto text = token.trim();
    if (text.isEmpty())
        return false;

    const auto numeric = text.endsWithChar ('%') ? text.dropLastCharacters (1) : text;
    return numeric.containsOnly ("0123456789.+-");
}
}

juce::String ResamplingDelayAudioProcessor::ParameterModulation::setScript (const juce::String& newScript, double sampleRate)
{
    script = newScript;
    stages.clear();
    loop = true;
    active = false;
    stage = 0;
    sample = 0;
    initialised = false;

    juce::StringArray lines;
    lines.addLines (newScript);

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        auto line = lines[lineIndex].upToFirstOccurrenceOf ("#", false, false).trim();
        if (line.isEmpty())
            continue;

        juce::StringArray tokens;
        tokens.addTokens (line, " \t", "\"'");
        tokens.removeEmptyStrings();

        if (tokens.size() == 0)
            continue;

        const auto lineError = [&] (const juce::String& message)
        {
            return "Line " + juce::String (lineIndex + 1) + ": " + message;
        };

        if (tokens[0].equalsIgnoreCase ("modulator") || tokens[0].equalsIgnoreCase ("end"))
            continue;

        if (tokens[0].equalsIgnoreCase ("mode") && tokens.size() > 1)
        {
            loop = ! tokens[1].equalsIgnoreCase ("one_shot");
            continue;
        }

        const auto isStageLine = tokens[0].equalsIgnoreCase ("stage");
        const auto commandIndex = isStageLine ? 2 : 0;
        if (isStageLine && tokens.size() < 3)
            return lineError ("stage needs a command, for example 'stage 1 to 80% for 1s'.");

        if (commandIndex >= tokens.size())
            return lineError ("missing command.");

        const auto command = tokens[commandIndex].toLowerCase();
        if (! (command == "to" || command == "hold" || command == "random" || command == "sine" || command == "wander"))
            return lineError ("unknown command '" + tokens[commandIndex] + "'. Use to, hold, random, sine, or wander.");

        ModStage next;
        next.target = stages.empty() ? 1.0f : stages.back().target;
        next.minimum = 0.0f;
        next.maximum = 1.0f;
        next.samples = (int) std::round (0.25 * sampleRate);
        next.smooth = true;
        int optionStart = commandIndex + 1;

        if (command == "to")
        {
            if (optionStart >= tokens.size())
                return lineError ("'to' needs a target value.");

            if (tokens[optionStart].equalsIgnoreCase ("random"))
            {
                if (optionStart + 2 >= tokens.size())
                    return lineError ("'to random' needs minimum and maximum values.");

                next.type = ModStage::Type::random;
                next.minimum = parseLevel (tokens[optionStart + 1]);
                next.maximum = parseLevel (tokens[optionStart + 2]);
                optionStart += 3;
            }
            else
            {
                if (! looksLikeNumberOrPercent (tokens[optionStart]))
                    return lineError ("target value must be 0..1 or a percentage.");

                next.type = ModStage::Type::ramp;
                next.target = parseLevel (tokens[optionStart]);
                optionStart += 1;
            }
        }
        else if (command == "hold")
        {
            next.type = ModStage::Type::hold;
        }
        else
        {
            if (optionStart + 1 >= tokens.size())
                return lineError ("'" + command + "' needs minimum and maximum values.");

            if (! looksLikeNumberOrPercent (tokens[optionStart]) || ! looksLikeNumberOrPercent (tokens[optionStart + 1]))
                return lineError ("'" + command + "' values must be 0..1 or percentages.");

            next.type = command == "random" ? ModStage::Type::random
                      : command == "sine"   ? ModStage::Type::sine
                                            : ModStage::Type::wander;
            next.minimum = parseLevel (tokens[optionStart]);
            next.maximum = parseLevel (tokens[optionStart + 1]);
            optionStart += 2;
        }

        if (next.minimum > next.maximum)
            std::swap (next.minimum, next.maximum);

        for (int i = optionStart; i < tokens.size(); ++i)
        {
            if ((tokens[i].equalsIgnoreCase ("for") || tokens[i].equalsIgnoreCase ("in")) && i + 1 < tokens.size())
                next.samples = juce::jmax (1, (int) std::round (parseTimeToSeconds (tokens[++i]) * sampleRate));
            else if (tokens[i].equalsIgnoreCase ("curve") && i + 1 < tokens.size())
                next.smooth = ! tokens[++i].equalsIgnoreCase ("linear");
            else
                return lineError ("unexpected token '" + tokens[i] + "'.");
        }

        stages.push_back (next);
    }

    active = ! stages.empty();
    return {};
}

float ResamplingDelayAudioProcessor::ParameterModulation::nextRandom()
{
    randomState = randomState * 1664525u + 1013904223u;
    return (float) ((randomState >> 8) & 0x00ffffffu) / (float) 0x00ffffffu;
}

float ResamplingDelayAudioProcessor::ParameterModulation::process (float baseNormalised)
{
    if (stages.empty() || ! active)
        return baseNormalised;

    if (! initialised)
    {
        current = baseNormalised;
        start = baseNormalised;
        initialised = true;
    }

    auto& currentStage = stages[(size_t) stage];
    if (sample == 0)
    {
        if (currentStage.type == ModStage::Type::random || currentStage.type == ModStage::Type::wander)
            currentStage.target = juce::jmap (nextRandom(), currentStage.minimum, currentStage.maximum);
        else if (currentStage.type == ModStage::Type::hold)
            currentStage.target = start;
    }

    const auto progress = juce::jlimit (0.0f, 1.0f, (float) sample / (float) juce::jmax (1, currentStage.samples));
    const auto shaped = currentStage.smooth ? progress * progress * (3.0f - 2.0f * progress) : progress;

    if (currentStage.type == ModStage::Type::sine)
    {
        const auto phase = progress * juce::MathConstants<float>::twoPi;
        const auto wave = 0.5f + 0.5f * std::sin (phase - juce::MathConstants<float>::halfPi);
        current = juce::jmap (wave, currentStage.minimum, currentStage.maximum);
    }
    else
    {
        current = start + (currentStage.target - start) * shaped;
    }

    if (++sample >= currentStage.samples)
    {
        current = currentStage.target;
        sample = 0;
        start = current;

        if (++stage >= (int) stages.size())
            stage = loop ? 0 : (int) stages.size() - 1;
    }

    const auto modulated = juce::jlimit (0.0f, 1.0f, current);
    return juce::jlimit (0.0f, 1.0f, baseNormalised + (modulated - baseNormalised) * depth);
}

ResamplingDelayAudioProcessor::ResamplingDelayAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (auto& value : currentModulatedValues)
        value.store (0.0f);

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
    currentDelayMs = smoothedDelayMs;
    currentFeedback = feedback->load();
    currentReverbTime = smoothedReverbTime;
    currentMix = mix->load();
    currentLofi = lofi->load();
    currentTone = tone->load();
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

    processModulations (buffer.getNumSamples());
    updateSmoothedValues();

    if (mode->load() < 0.5f)
        processDelay (buffer);
    else
        processReverb (buffer);
}

void ResamplingDelayAudioProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto mixValue = currentMix;
    const auto wetGain = equalPowerWet (mixValue);
    const auto dryGain = equalPowerDry (mixValue);
    const auto fb = currentFeedback;
    const auto lofiAmount = currentLofi;
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
            wet = toneFilter (channel, wet, currentTone);
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
    const auto mixValue = currentMix;
    const auto wetGain = equalPowerWet (mixValue);
    const auto dryGain = equalPowerDry (mixValue);
    const auto lofiAmount = currentLofi;
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
                const auto degraded = crush (channel, toneFilter (channel, diffused, currentTone));
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
    smoothedDelayMs += delayCoefficient * (currentDelayMs - smoothedDelayMs);
    smoothedReverbTime += reverbCoefficient * (currentReverbTime - smoothedReverbTime);
}

void ResamplingDelayAudioProcessor::processModulations (int numSamples)
{
    const auto samples = juce::jmax (1, numSamples);

    for (int i = 0; i < samples; ++i)
    {
        juce::ignoreUnused (i);
        currentDelayMs = processModulatedParameter (0, delayMs->load());
        currentFeedback = processModulatedParameter (1, feedback->load());
        currentReverbTime = processModulatedParameter (2, reverbTime->load());
        currentMix = processModulatedParameter (3, mix->load());
        currentLofi = processModulatedParameter (4, lofi->load());
        currentTone = processModulatedParameter (5, tone->load());
    }
}

int ResamplingDelayAudioProcessor::parameterIndexForId (const juce::String& parameterId)
{
    for (int i = 0; i < (int) parameterIds.size(); ++i)
        if (parameterId == parameterIds[(size_t) i])
            return i;

    return -1;
}

float ResamplingDelayAudioProcessor::normaliseParameterValue (int parameterIndex, float value)
{
    switch (parameterIndex)
    {
        case 0: return juce::jmap (value, 35.0f, 1800.0f, 0.0f, 1.0f);
        case 1: return juce::jmap (value, 0.0f, 0.96f, 0.0f, 1.0f);
        case 2: return juce::jmap (value, 0.35f, maxReverbSeconds, 0.0f, 1.0f);
        default: return juce::jlimit (0.0f, 1.0f, value);
    }
}

float ResamplingDelayAudioProcessor::denormaliseParameterValue (int parameterIndex, float normalised)
{
    const auto value = juce::jlimit (0.0f, 1.0f, normalised);

    switch (parameterIndex)
    {
        case 0: return juce::jmap (value, 35.0f, 1800.0f);
        case 1: return juce::jmap (value, 0.0f, 0.96f);
        case 2: return juce::jmap (value, 0.35f, maxReverbSeconds);
        default: return value;
    }
}

float ResamplingDelayAudioProcessor::processModulatedParameter (int parameterIndex, float baseValue)
{
    const juce::ScopedLock lock (modulationLock);
    if (! juce::isPositiveAndBelow (parameterIndex, (int) parameterModulations.size()))
        return baseValue;

    const auto baseNormalised = normaliseParameterValue (parameterIndex, baseValue);
    const auto modulatedValue = denormaliseParameterValue (parameterIndex, parameterModulations[(size_t) parameterIndex].process (baseNormalised));
    currentModulatedValues[(size_t) parameterIndex].store (modulatedValue, std::memory_order_relaxed);
    return modulatedValue;
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
    auto savedState = state.copyState();
    {
        const juce::ScopedLock lock (modulationLock);
        for (int i = 0; i < (int) parameterIds.size(); ++i)
        {
            savedState.setProperty ("fabricScript_" + juce::String (parameterIds[(size_t) i]),
                                    parameterModulations[(size_t) i].script,
                                    nullptr);
            savedState.setProperty ("fabricScriptActive_" + juce::String (parameterIds[(size_t) i]),
                                    parameterModulations[(size_t) i].active,
                                    nullptr);
            savedState.setProperty ("fabricScriptDepth_" + juce::String (parameterIds[(size_t) i]),
                                    parameterModulations[(size_t) i].depth,
                                    nullptr);
        }
    }

    if (auto xml = savedState.createXml())
        copyXmlToBinary (*xml, destData);
}

void ResamplingDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            state.replaceState (juce::ValueTree::fromXml (*xml));
            for (int i = 0; i < (int) parameterIds.size(); ++i)
            {
                setFabricScriptForParameter (parameterIds[(size_t) i],
                    state.state.getProperty ("fabricScript_" + juce::String (parameterIds[(size_t) i])).toString());
                setFabricScriptActive (parameterIds[(size_t) i],
                    (bool) state.state.getProperty ("fabricScriptActive_" + juce::String (parameterIds[(size_t) i]), true));
                setFabricScriptDepth (parameterIds[(size_t) i],
                    (float) state.state.getProperty ("fabricScriptDepth_" + juce::String (parameterIds[(size_t) i]), 1.0f));
            }
        }
}

void ResamplingDelayAudioProcessor::setFabricScriptForParameter (const juce::String& parameterId, const juce::String& script)
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) parameterModulations.size()))
        return;

    ParameterModulation parsed;
    if (parsed.setScript (script, currentSampleRate).isNotEmpty())
        return;

    const juce::ScopedLock lock (modulationLock);
    parsed.depth = parameterModulations[(size_t) index].depth;
    parameterModulations[(size_t) index] = std::move (parsed);
}

juce::String ResamplingDelayAudioProcessor::validateFabricScript (const juce::String& script) const
{
    ParameterModulation parsed;
    return parsed.setScript (script, currentSampleRate);
}

juce::String ResamplingDelayAudioProcessor::getFabricScriptForParameter (const juce::String& parameterId) const
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) parameterModulations.size()))
        return {};

    const juce::ScopedLock lock (modulationLock);
    return parameterModulations[(size_t) index].script;
}

void ResamplingDelayAudioProcessor::setFabricScriptDepth (const juce::String& parameterId, float depth)
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) parameterModulations.size()))
        return;

    const juce::ScopedLock lock (modulationLock);
    parameterModulations[(size_t) index].depth = juce::jlimit (0.0f, 1.0f, depth);
}

float ResamplingDelayAudioProcessor::getFabricScriptDepth (const juce::String& parameterId) const
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) parameterModulations.size()))
        return 1.0f;

    const juce::ScopedLock lock (modulationLock);
    return parameterModulations[(size_t) index].depth;
}

void ResamplingDelayAudioProcessor::setFabricScriptActive (const juce::String& parameterId, bool active)
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) parameterModulations.size()))
        return;

    const juce::ScopedLock lock (modulationLock);
    auto& modulation = parameterModulations[(size_t) index];
    modulation.active = active && ! modulation.stages.empty();
    if (modulation.active)
    {
        modulation.stage = 0;
        modulation.sample = 0;
        modulation.initialised = false;
    }
}

bool ResamplingDelayAudioProcessor::isFabricScriptActive (const juce::String& parameterId) const
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) parameterModulations.size()))
        return false;

    const juce::ScopedLock lock (modulationLock);
    return parameterModulations[(size_t) index].active && ! parameterModulations[(size_t) index].stages.empty();
}

float ResamplingDelayAudioProcessor::getCurrentModulatedParameterValue (const juce::String& parameterId) const
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) currentModulatedValues.size()))
        return 0.0f;

    return currentModulatedValues[(size_t) index].load (std::memory_order_relaxed);
}

bool ResamplingDelayAudioProcessor::parameterHasFabricScript (const juce::String& parameterId) const
{
    const auto index = parameterIndexForId (parameterId);
    if (! juce::isPositiveAndBelow (index, (int) parameterModulations.size()))
        return false;

    const juce::ScopedLock lock (modulationLock);
    return ! parameterModulations[(size_t) index].stages.empty();
}

juce::AudioProcessorEditor* ResamplingDelayAudioProcessor::createEditor()
{
    return new ResamplingDelayAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResamplingDelayAudioProcessor();
}

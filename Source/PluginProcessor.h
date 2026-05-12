#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <vector>

class ResamplingDelayAudioProcessor final : public juce::AudioProcessor
{
public:
    ResamplingDelayAudioProcessor();
    ~ResamplingDelayAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS& getState() { return state; }
    static APVTS::ParameterLayout createParameterLayout();

private:
    struct VariableDelay
    {
        void prepare (double newSampleRate, int channels, double maxDelaySeconds);
        void clear();
        float read (int channel, float delaySamples) const;
        void write (int channel, float sample);
        void advance();

        juce::AudioBuffer<float> buffer;
        int writePosition = 0;
    };

    struct SampleCrusher
    {
        void prepare (int channels);
        void reset();
        float process (int channel, float input, float amount);

        std::vector<int> counters;
        std::vector<float> heldSamples;
    };

    struct ReverbLine
    {
        VariableDelay delay;
        float baseDelaySeconds = 0.1f;
    };

    void processDelay (juce::AudioBuffer<float>& buffer);
    void processReverb (juce::AudioBuffer<float>& buffer);
    float toneFilter (int channel, float input, float toneValue);
    float crush (int channel, float input) { return crusher.process (channel, input, lofi->load()); }
    void updateSmoothedValues();

    APVTS state;

    VariableDelay delayLine;
    SampleCrusher crusher;
    std::array<std::array<ReverbLine, 6>, 2> reverbLines;

    std::array<float, 2> toneStates {};
    std::array<float, 2> phaseOffsets { 0.0f, juce::MathConstants<float>::pi };
    double currentSampleRate = 44100.0;
    float wowPhase = 0.0f;
    float smoothedDelayMs = 320.0f;
    float smoothedReverbTime = 2.4f;

    std::atomic<float>* mode = nullptr;
    std::atomic<float>* delayMs = nullptr;
    std::atomic<float>* feedback = nullptr;
    std::atomic<float>* reverbTime = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* lofi = nullptr;
    std::atomic<float>* tone = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResamplingDelayAudioProcessor)
};

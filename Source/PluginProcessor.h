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

    void setFabricScriptForParameter (const juce::String& parameterId, const juce::String& script);
    juce::String validateFabricScript (const juce::String& script) const;
    juce::String getFabricScriptForParameter (const juce::String& parameterId) const;
    void setFabricScriptDepth (const juce::String& parameterId, float depth);
    float getFabricScriptDepth (const juce::String& parameterId) const;
    void setFabricScriptActive (const juce::String& parameterId, bool active);
    bool isFabricScriptActive (const juce::String& parameterId) const;
    float getCurrentModulatedParameterValue (const juce::String& parameterId) const;
    bool parameterHasFabricScript (const juce::String& parameterId) const;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS& getState() { return state; }
    static APVTS::ParameterLayout createParameterLayout();

private:
    static constexpr int numScriptableParameters = 6;

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
        SampleCrusher crusher;
        float baseDelaySeconds = 0.1f;
        float toneState = 0.0f;
        float deClickState = 0.0f;
        float tapState = 0.0f;
        float writeState = 0.0f;
    };

    struct ModStage
    {
        enum class Type
        {
            ramp,
            hold,
            sine,
            random,
            wander
        };

        Type type = Type::ramp;
        float target = 0.0f;
        float minimum = 0.0f;
        float maximum = 1.0f;
        int samples = 1;
        bool smooth = true;
    };

    struct ParameterModulation
    {
        juce::String script;
        std::vector<ModStage> stages;
        bool loop = true;
        bool active = false;
        float depth = 1.0f;
        int stage = 0;
        int sample = 0;
        float start = 0.0f;
        float current = 0.0f;
        bool initialised = false;
        uint32_t randomState = 0x12345678u;

        juce::String setScript (const juce::String& newScript, double sampleRate);
        float process (float baseNormalised);

    private:
        float nextRandom();
    };

    void processDelay (juce::AudioBuffer<float>& buffer);
    void processReverb (juce::AudioBuffer<float>& buffer);
    float toneFilter (int channel, float input, float toneValue);
    float reverbToneFilter (ReverbLine& line, float input, float toneValue);
    float reverbDeClickFilter (ReverbLine& line, float input, float lofiAmount);
    float reverbSpikeGuard (float& guardState, float input, float lofiAmount);
    float crush (int channel, float input) { return crusher.process (channel, input, currentLofi); }
    void updateSmoothedValues();
    void processModulations (int numSamples);
    static int parameterIndexForId (const juce::String& parameterId);
    static float normaliseParameterValue (int parameterIndex, float value);
    static float denormaliseParameterValue (int parameterIndex, float normalised);
    float processModulatedParameter (int parameterIndex, float baseValue);

    APVTS state;

    VariableDelay delayLine;
    SampleCrusher crusher;
    std::array<std::array<ReverbLine, 6>, 2> reverbLines;

    std::array<float, 2> toneStates {};
    std::array<float, 2> reverbWetStates {};
    std::array<float, 2> phaseOffsets { 0.0f, juce::MathConstants<float>::pi };
    double currentSampleRate = 44100.0;
    float wowPhase = 0.0f;
    float smoothedDelayMs = 320.0f;
    float smoothedReverbTime = 2.4f;
    float currentDelayMs = 320.0f;
    float currentFeedback = 0.48f;
    float currentReverbTime = 2.4f;
    float currentMix = 0.42f;
    float currentLofi = 0.55f;
    float currentTone = 0.38f;

    std::array<ParameterModulation, numScriptableParameters> parameterModulations;
    std::array<std::atomic<float>, numScriptableParameters> currentModulatedValues;
    mutable juce::CriticalSection modulationLock;

    std::atomic<float>* mode = nullptr;
    std::atomic<float>* delayMs = nullptr;
    std::atomic<float>* feedback = nullptr;
    std::atomic<float>* reverbTime = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* lofi = nullptr;
    std::atomic<float>* tone = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResamplingDelayAudioProcessor)
};

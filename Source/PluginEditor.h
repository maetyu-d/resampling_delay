#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

class ResamplingDelayAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer
{
public:
    explicit ResamplingDelayAudioProcessorEditor (ResamplingDelayAudioProcessor&);
    ~ResamplingDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct KnobLookAndFeel final : public juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&,
                               int x,
                               int y,
                               int width,
                               int height,
                               float sliderPosProportional,
                               float rotaryStartAngle,
                               float rotaryEndAngle,
                               juce::Slider&) override;
    };

    struct ModSlider final : public juce::Slider
    {
        bool hasScript = false;
        bool scriptActive = false;
        float pulsePhase = 0.0f;
        std::function<void()> onDotClicked;
        std::function<void()> onDotDoubleClicked;
        std::function<void()> onManualDragStarted;

        void mouseDown (const juce::MouseEvent&) override;
    };

    void configureSlider (ModSlider& slider, const juce::String& parameterId, const juce::String& parameterName, const juce::String& suffix = {});
    void configureLabel (juce::Label& label, const juce::String& text);
    void setSliderAccent (ModSlider& slider, juce::Colour accent);
    void openFabricScriptEditor (const juce::String& parameterId, const juce::String& parameterName);
    void timerCallback() override;
    void updateScriptIndicator (ModSlider& slider, const juce::String& parameterId);
    void updateModulatedSlider (ModSlider& slider, const juce::String& parameterId);

    ResamplingDelayAudioProcessor& audioProcessor;
    KnobLookAndFeel knobLookAndFeel;

    juce::ComboBox modeBox;
    ModSlider delaySlider;
    ModSlider feedbackSlider;
    ModSlider reverbSlider;
    ModSlider mixSlider;
    ModSlider lofiSlider;
    ModSlider toneSlider;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label modeLabel;
    juce::Label delayLabel;
    juce::Label feedbackLabel;
    juce::Label reverbLabel;
    juce::Label mixLabel;
    juce::Label lofiLabel;
    juce::Label toneLabel;

    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<Attachment> delayAttachment;
    std::unique_ptr<Attachment> feedbackAttachment;
    std::unique_ptr<Attachment> reverbAttachment;
    std::unique_ptr<Attachment> mixAttachment;
    std::unique_ptr<Attachment> lofiAttachment;
    std::unique_ptr<Attachment> toneAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResamplingDelayAudioProcessorEditor)
};

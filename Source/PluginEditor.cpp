#include "PluginEditor.h"

void ResamplingDelayAudioProcessorEditor::KnobLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                                                            int x,
                                                                            int y,
                                                                            int width,
                                                                            int height,
                                                                            float sliderPosProportional,
                                                                            float rotaryStartAngle,
                                                                            float rotaryEndAngle,
                                                                            juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.46f;
    const auto centre = bounds.getCentre();
    const auto lineWidth = juce::jmax (4.0f, radius * 0.16f);
    const auto arcRadius = radius - lineWidth * 0.5f;
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::ColourGradient shadow (juce::Colour (0x62000000), centre.x, centre.y + radius * 0.35f,
                                 juce::Colours::transparentBlack, centre.x, centre.y + radius * 1.20f, true);
    g.setGradientFill (shadow);
    g.fillEllipse (centre.x - radius * 1.08f, centre.y - radius * 0.82f, radius * 2.16f, radius * 2.16f);

    juce::ColourGradient body (juce::Colour (0xff313a42), centre.x, centre.y - radius * 0.15f,
                               juce::Colour (0xff07090b), centre.x, centre.y + radius, true);
    body.addColour (0.32, juce::Colour (0xff1a2229));
    body.addColour (0.78, juce::Colour (0xff0d1115));
    g.setGradientFill (body);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (juce::Colour (0x55e9f8ff));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.1f);

    g.setColour (juce::Colour (0x2cffffff));
    g.drawEllipse (centre.x - radius * 0.72f, centre.y - radius * 0.72f, radius * 1.44f, radius * 1.44f, 1.2f);
    g.setColour (juce::Colour (0x24000000));
    g.drawEllipse (centre.x - radius * 0.86f, centre.y - radius * 0.86f, radius * 1.72f, radius * 1.72f, 2.0f);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (valueArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto thumbRadius = lineWidth * 0.64f;
    const auto thumbPoint = centre.getPointOnCircumference (arcRadius, angle);
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (thumbPoint.x - thumbRadius, thumbPoint.y - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);

    const auto dotRadius = juce::jmax (4.0f, lineWidth * 0.48f);
    const auto* modSlider = dynamic_cast<ResamplingDelayAudioProcessorEditor::ModSlider*> (&slider);
    const auto hasScript = modSlider != nullptr && modSlider->hasScript;
    const auto scriptActive = modSlider != nullptr && modSlider->scriptActive;
    const auto pulse = scriptActive && modSlider != nullptr
        ? 0.5f + 0.5f * std::sin (modSlider->pulsePhase)
        : 0.0f;
    const auto glowScale = scriptActive ? 1.0f + pulse * 0.42f : 1.0f;
    const auto coreScale = scriptActive ? 1.0f + pulse * 0.12f : 1.0f;
    const auto dotColour = scriptActive ? juce::Colour (0xff42c7ff)
                                        : (hasScript ? juce::Colour (0xff276ea7) : juce::Colour (0xff193449));
    const auto glowAlpha = scriptActive ? (juce::uint8) (0x42 + (int) (pulse * 0x34))
                                        : (hasScript ? (juce::uint8) 0x30 : (juce::uint8) 0x14);

    g.setColour (dotColour.withAlpha ((juce::uint8) glowAlpha));
    g.fillEllipse (centre.x - dotRadius * 3.1f * glowScale,
                   centre.y - dotRadius * 3.1f * glowScale,
                   dotRadius * 6.2f * glowScale,
                   dotRadius * 6.2f * glowScale);
    g.setColour (dotColour.brighter (scriptActive ? 0.18f : 0.02f));
    g.fillEllipse (centre.x - dotRadius * coreScale,
                   centre.y - dotRadius * coreScale,
                   dotRadius * 2.0f * coreScale,
                   dotRadius * 2.0f * coreScale);
    g.setColour (scriptActive ? juce::Colour (0xccffffff) : juce::Colour (0x55ffffff));
    g.fillEllipse (centre.x - dotRadius * 0.40f, centre.y - dotRadius * 0.48f, dotRadius * 0.58f, dotRadius * 0.42f);
    g.setColour (juce::Colour (0x66000000));
    g.drawEllipse (centre.x - dotRadius, centre.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f, 1.0f);
}

void ResamplingDelayAudioProcessorEditor::ModSlider::mouseDown (const juce::MouseEvent& event)
{
    const auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const auto dotRadius = juce::jmax (10.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.055f);

    if (event.position.getDistanceFrom (centre) <= dotRadius * 1.6f && onDotClicked != nullptr)
    {
        if (event.getNumberOfClicks() >= 2 && onDotDoubleClicked != nullptr)
            onDotDoubleClicked();
        else
            onDotClicked();

        return;
    }

    if (onManualDragStarted != nullptr)
        onManualDragStarted();

    juce::Slider::mouseDown (event);
}

namespace
{
class FabricScriptComponent final : public juce::Component
{
public:
    FabricScriptComponent (ResamplingDelayAudioProcessor& processorIn,
                           juce::String parameterIdIn,
                           juce::String parameterNameIn)
        : processor (processorIn),
          parameterId (std::move (parameterIdIn)),
          parameterName (std::move (parameterNameIn))
    {
        title.setText ("Fabric envelope: " + parameterName, juce::dontSendNotification);
        title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, juce::Colour (0xfff4f7fb));
        addAndMakeVisible (title);

        script.setMultiLine (true);
        script.setReturnKeyStartsNewLine (true);
        script.setScrollbarsShown (true);
        script.setFont (juce::FontOptions (14.0f));
        script.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0c1116));
        script.setColour (juce::TextEditor::textColourId, juce::Colour (0xffeef5ff));
        script.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff26323d));
        script.setText (processor.getFabricScriptForParameter (parameterId).isEmpty()
            ? defaultScript()
            : processor.getFabricScriptForParameter (parameterId));
        addAndMakeVisible (script);

        help.setText ("Commands: to 80%, random 20% 80%, hold, sine 10% 90%, wander 20% 80%.",
                      juce::dontSendNotification);
        help.setFont (juce::FontOptions (13.0f));
        help.setColour (juce::Label::textColourId, juce::Colour (0xff8f9aa5));
        help.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (help);

        depthLabel.setText ("Depth", juce::dontSendNotification);
        depthLabel.setFont (juce::FontOptions (13.0f));
        depthLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb8c4ce));
        depthLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (depthLabel);

        depth.setSliderStyle (juce::Slider::LinearHorizontal);
        depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 20);
        depth.setRange (0.0, 1.0, 0.001);
        depth.setValue (processor.getFabricScriptDepth (parameterId), juce::dontSendNotification);
        depth.setColour (juce::Slider::trackColourId, juce::Colour (0xff42c7ff));
        depth.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff24313b));
        depth.setColour (juce::Slider::thumbColourId, juce::Colour (0xffeef5ff));
        depth.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffdce7ef));
        depth.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        depth.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (depth);

        error.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        error.setColour (juce::Label::textColourId, juce::Colour (0xffff6f61));
        error.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (error);

        apply.setButtonText ("Apply");
        apply.onClick = [this]
        {
            const auto validationError = processor.validateFabricScript (script.getText());
            if (validationError.isNotEmpty())
            {
                error.setText (validationError, juce::dontSendNotification);
                return;
            }

            processor.setFabricScriptForParameter (parameterId, script.getText());
            processor.setFabricScriptDepth (parameterId, (float) depth.getValue());
            if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState (0);
        };
        addAndMakeVisible (apply);

        clear.setButtonText ("Clear");
        clear.onClick = [this]
        {
            processor.setFabricScriptForParameter (parameterId, {});
            if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState (0);
        };
        addAndMakeVisible (clear);

        cancel.setButtonText ("Cancel");
        cancel.onClick = [this]
        {
            if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState (0);
        };
        addAndMakeVisible (cancel);

        setSize (620, 470);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (18);
        title.setBounds (area.removeFromTop (30));
        help.setBounds (area.removeFromBottom (30));
        error.setBounds (area.removeFromBottom (28));
        auto buttons = area.removeFromBottom (42);
        apply.setBounds (buttons.removeFromRight (94).reduced (4));
        clear.setBounds (buttons.removeFromRight (94).reduced (4));
        cancel.setBounds (buttons.removeFromRight (94).reduced (4));
        auto depthArea = area.removeFromBottom (34);
        depthLabel.setBounds (depthArea.removeFromLeft (58));
        depth.setBounds (depthArea.reduced (0, 4));
        script.setBounds (area.reduced (0, 8));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0e1419));
    }

private:
    juce::String defaultScript() const
    {
        if (parameterId == "delayMs")
            return "modulator delayMs\n"
                   "  mode loop\n"
                   "  stage 1 to 28% for 6s curve smooth\n"
                   "  stage 2 to 72% for 9s curve smooth\n"
                   "  stage 3 hold for 1200ms\n"
                   "  stage 4 to 42% for 5s curve smooth\n"
                   "end\n";

        if (parameterId == "feedback")
            return "modulator feedback\n"
                   "  mode loop\n"
                   "  stage 1 sine 38% 68% for 8s\n"
                   "end\n";

        if (parameterId == "reverbTime")
            return "modulator reverbTime\n"
                   "  mode loop\n"
                   "  stage 1 to 35% for 7s curve smooth\n"
                   "  stage 2 to 82% for 12s curve smooth\n"
                   "  stage 3 to 56% for 5s curve smooth\n"
                   "end\n";

        if (parameterId == "mix")
            return "modulator mix\n"
                   "  mode loop\n"
                   "  stage 1 sine 32% 58% for 10s\n"
                   "end\n";

        if (parameterId == "lofi")
            return "modulator lofi\n"
                   "  mode loop\n"
                   "  stage 1 random 38% 76% for 220ms curve smooth\n"
                   "  stage 2 random 18% 62% for 480ms curve smooth\n"
                   "  stage 3 hold for 160ms\n"
                   "end\n";

        if (parameterId == "tone")
            return "modulator tone\n"
                   "  mode loop\n"
                   "  stage 1 wander 22% 68% for 1300ms curve smooth\n"
                   "  stage 2 sine 32% 82% for 11s\n"
                   "end\n";

        return "modulator " + parameterId + "\n"
               "  mode loop\n"
               "  stage 1 random 20% 80% for 700ms curve smooth\n"
               "  stage 2 hold for 300ms\n"
               "  stage 3 sine 10% 90% for 2s\n"
               "  stage 4 wander 25% 75% for 900ms curve smooth\n"
               "end\n";
    }

    ResamplingDelayAudioProcessor& processor;
    juce::String parameterId;
    juce::String parameterName;
    juce::Label title;
    juce::Label help;
    juce::Label depthLabel;
    juce::Label error;
    juce::TextEditor script;
    juce::Slider depth;
    juce::TextButton apply;
    juce::TextButton clear;
    juce::TextButton cancel;
};
}

ResamplingDelayAudioProcessorEditor::ResamplingDelayAudioProcessorEditor (ResamplingDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    configureLabel (titleLabel, "RESAMPLING DELAY");
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (26.0f, juce::Font::plain));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff5f8fb));
    addAndMakeVisible (titleLabel);

    configureLabel (subtitleLabel, "matd.space");
    subtitleLabel.setJustificationType (juce::Justification::centredRight);
    subtitleLabel.setFont (juce::FontOptions (13.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d9ba7));
    addAndMakeVisible (subtitleLabel);

    modeBox.addItem ("Delay", 1);
    modeBox.addItem ("Reverb", 2);
    modeBox.setJustificationType (juce::Justification::centred);
    modeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0c1116));
    modeBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xffeef5ff));
    modeBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff42c7ff));
    modeBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff42c7ff));
    addAndMakeVisible (modeBox);

    configureSlider (delaySlider, "delayMs", "Echo Time", " ms");
    configureSlider (feedbackSlider, "feedback", "Repeats");
    configureSlider (reverbSlider, "reverbTime", "Decay", " s");
    configureSlider (mixSlider, "mix", "Dry/Wet");
    configureSlider (lofiSlider, "lofi", "Wear");
    configureSlider (toneSlider, "tone", "Tone");

    configureLabel (modeLabel, "Mode");
    configureLabel (delayLabel, "Echo Time");
    configureLabel (feedbackLabel, "Repeats");
    configureLabel (reverbLabel, "Decay");
    configureLabel (mixLabel, "Dry/Wet");
    configureLabel (lofiLabel, "Wear");
    configureLabel (toneLabel, "Tone");

    setSliderAccent (delaySlider, juce::Colour (0xff6ce0c7));
    setSliderAccent (feedbackSlider, juce::Colour (0xffffbd61));
    setSliderAccent (reverbSlider, juce::Colour (0xffa9b5ff));
    setSliderAccent (mixSlider, juce::Colour (0xfff0f3f4));
    setSliderAccent (lofiSlider, juce::Colour (0xffff6f59));
    setSliderAccent (toneSlider, juce::Colour (0xff77d7ff));

    modeAttachment = std::make_unique<ComboAttachment> (audioProcessor.getState(), "mode", modeBox);
    delayAttachment = std::make_unique<Attachment> (audioProcessor.getState(), "delayMs", delaySlider);
    feedbackAttachment = std::make_unique<Attachment> (audioProcessor.getState(), "feedback", feedbackSlider);
    reverbAttachment = std::make_unique<Attachment> (audioProcessor.getState(), "reverbTime", reverbSlider);
    mixAttachment = std::make_unique<Attachment> (audioProcessor.getState(), "mix", mixSlider);
    lofiAttachment = std::make_unique<Attachment> (audioProcessor.getState(), "lofi", lofiSlider);
    toneAttachment = std::make_unique<Attachment> (audioProcessor.getState(), "tone", toneSlider);

    setSize (960, 500);
    startTimerHz (30);
}

ResamplingDelayAudioProcessorEditor::~ResamplingDelayAudioProcessorEditor()
{
    for (auto* slider : { &delaySlider, &feedbackSlider, &reverbSlider, &mixSlider, &lofiSlider, &toneSlider })
        slider->setLookAndFeel (nullptr);
}

void ResamplingDelayAudioProcessorEditor::configureSlider (ModSlider& slider,
                                                           const juce::String& parameterId,
                                                           const juce::String& parameterName,
                                                           const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setLookAndFeel (&knobLookAndFeel);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 18);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff6ce0c7));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff252a2d));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xfff7f7f2));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff0f3f4));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.onDotClicked = [this, parameterId]
    {
        audioProcessor.setFabricScriptActive (parameterId, ! audioProcessor.isFabricScriptActive (parameterId));
    };
    slider.onDotDoubleClicked = [this, parameterId, parameterName] { openFabricScriptEditor (parameterId, parameterName); };
    slider.onManualDragStarted = [this, parameterId] { audioProcessor.setFabricScriptActive (parameterId, false); };
    addAndMakeVisible (slider);
}

void ResamplingDelayAudioProcessorEditor::configureLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffdbe4e4));
    label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void ResamplingDelayAudioProcessorEditor::setSliderAccent (ModSlider& slider, juce::Colour accent)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, accent.withAlpha (0.18f));
}

void ResamplingDelayAudioProcessorEditor::openFabricScriptEditor (const juce::String& parameterId, const juce::String& parameterName)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Fabric Script";
    options.dialogBackgroundColour = juce::Colour (0xff20262b);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setOwned (new FabricScriptComponent (audioProcessor, parameterId, parameterName));
    options.launchAsync();
}

void ResamplingDelayAudioProcessorEditor::timerCallback()
{
    auto animatePulse = [] (ModSlider& slider)
    {
        if (! slider.scriptActive)
        {
            slider.pulsePhase = 0.0f;
            return;
        }

        slider.pulsePhase += 0.18f;
        if (slider.pulsePhase > juce::MathConstants<float>::twoPi)
            slider.pulsePhase -= juce::MathConstants<float>::twoPi;

        slider.repaint();
    };

    updateScriptIndicator (delaySlider, "delayMs");
    updateScriptIndicator (feedbackSlider, "feedback");
    updateScriptIndicator (reverbSlider, "reverbTime");
    updateScriptIndicator (mixSlider, "mix");
    updateScriptIndicator (lofiSlider, "lofi");
    updateScriptIndicator (toneSlider, "tone");

    updateModulatedSlider (delaySlider, "delayMs");
    updateModulatedSlider (feedbackSlider, "feedback");
    updateModulatedSlider (reverbSlider, "reverbTime");
    updateModulatedSlider (mixSlider, "mix");
    updateModulatedSlider (lofiSlider, "lofi");
    updateModulatedSlider (toneSlider, "tone");

    animatePulse (delaySlider);
    animatePulse (feedbackSlider);
    animatePulse (reverbSlider);
    animatePulse (mixSlider);
    animatePulse (lofiSlider);
    animatePulse (toneSlider);
}

void ResamplingDelayAudioProcessorEditor::updateScriptIndicator (ModSlider& slider, const juce::String& parameterId)
{
    const auto hasScript = audioProcessor.parameterHasFabricScript (parameterId);
    const auto scriptActive = audioProcessor.isFabricScriptActive (parameterId);

    if (slider.hasScript != hasScript || slider.scriptActive != scriptActive)
    {
        slider.hasScript = hasScript;
        slider.scriptActive = scriptActive;
        if (! scriptActive)
            slider.pulsePhase = 0.0f;
        slider.repaint();
    }
}

void ResamplingDelayAudioProcessorEditor::updateModulatedSlider (ModSlider& slider, const juce::String& parameterId)
{
    if (! audioProcessor.isFabricScriptActive (parameterId) || slider.isMouseButtonDown())
        return;

    slider.setValue (audioProcessor.getCurrentModulatedParameterValue (parameterId), juce::dontSendNotification);
}

void ResamplingDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff070a0d));

    auto outer = getLocalBounds().toFloat();
    juce::ColourGradient background (juce::Colour (0xff111b24), outer.getTopLeft(),
                                     juce::Colour (0xff030405), outer.getBottomRight(), false);
    background.addColour (0.40, juce::Colour (0xff0b1218));
    background.addColour (0.78, juce::Colour (0xff08090b));
    g.setGradientFill (background);
    g.fillRect (outer);

    auto halo = outer.reduced (80.0f, 30.0f);
    juce::ColourGradient sheen (juce::Colour (0x223ed0ff), halo.getTopLeft(),
                                juce::Colours::transparentBlack, halo.getBottomRight(), true);
    g.setGradientFill (sheen);
    g.fillEllipse (halo.withY (halo.getY() - 80.0f));

    auto panel = getLocalBounds().toFloat().reduced (18.0f);
    juce::ColourGradient rainbow (juce::Colour (0xd838d9ff), panel.getTopLeft(),
                                  juce::Colour (0xd8ff4f8a), panel.getBottomRight(), false);
    rainbow.addColour (0.16, juce::Colour (0xd86a7cff));
    rainbow.addColour (0.32, juce::Colour (0xd8c95dff));
    rainbow.addColour (0.49, juce::Colour (0xd8ff7a5c));
    rainbow.addColour (0.66, juce::Colour (0xd8ffd35f));
    rainbow.addColour (0.82, juce::Colour (0xd86dffb5));
    g.setGradientFill (rainbow);
    g.fillRoundedRectangle (panel, 8.0f);

    juce::ColourGradient depth (juce::Colour (0x8a050a0e), panel.getTopLeft(),
                                juce::Colour (0xc6030507), panel.getBottomRight(), false);
    depth.addColour (0.45, juce::Colour (0x71101924));
    g.setGradientFill (depth);
    g.fillRoundedRectangle (panel, 8.0f);

    auto gloss = panel.reduced (1.0f).withTrimmedBottom (panel.getHeight() * 0.58f);
    juce::ColourGradient glossGradient (juce::Colour (0x26ffffff), gloss.getTopLeft(),
                                        juce::Colours::transparentWhite, gloss.getBottomLeft(), false);
    g.setGradientFill (glossGradient);
    g.fillRoundedRectangle (gloss, 8.0f);

    auto content = getLocalBounds().reduced (42, 32);
    content.removeFromTop (48);
    content.removeFromTop (18);
    auto upperWell = content.removeFromTop (158).toFloat().reduced (4.0f, 2.0f);
    content.removeFromTop (16);
    auto lowerWell = content.removeFromTop (158).toFloat().reduced (4.0f, 2.0f);
    auto combinedWell = upperWell.getUnion (lowerWell).expanded (2.0f, 5.0f);

    juce::ColourGradient wellGradient (juce::Colour (0x70070b10), combinedWell.getTopLeft(),
                                       juce::Colour (0x96030406), combinedWell.getBottomRight(), false);
    wellGradient.addColour (0.50, juce::Colour (0x68111820));
    g.setGradientFill (wellGradient);
    g.fillRoundedRectangle (combinedWell, 9.0f);

    g.setColour (juce::Colour (0x18ffffff));
    g.drawRoundedRectangle (combinedWell, 9.0f, 0.8f);

    juce::ColourGradient rowSheen (juce::Colour (0x1affffff), upperWell.getTopLeft(),
                                   juce::Colours::transparentWhite, upperWell.getBottomLeft(), false);
    g.setGradientFill (rowSheen);
    g.fillRoundedRectangle (upperWell.withHeight (upperWell.getHeight() * 0.36f), 8.0f);

    g.setGradientFill (rowSheen);
    g.fillRoundedRectangle (lowerWell.withHeight (lowerWell.getHeight() * 0.30f), 8.0f);

    auto rowGap = juce::Rectangle<float> (combinedWell.getX() + 12.0f,
                                          upperWell.getBottom() + 4.0f,
                                          combinedWell.getWidth() - 24.0f,
                                          lowerWell.getY() - upperWell.getBottom() - 8.0f);
    g.setColour (juce::Colour (0x24000000));
    g.fillRoundedRectangle (rowGap, 3.0f);
    g.setColour (juce::Colour (0x13ffffff));
    g.drawLine (rowGap.getX() + 8.0f, rowGap.getCentreY(), rowGap.getRight() - 8.0f, rowGap.getCentreY(), 1.0f);

    juce::ColourGradient bottomShade (juce::Colours::transparentBlack, panel.getCentreX(), panel.getY() + panel.getHeight() * 0.35f,
                                      juce::Colour (0x74000000), panel.getCentreX(), panel.getBottom(), false);
    g.setGradientFill (bottomShade);
    g.fillRoundedRectangle (panel, 8.0f);

    g.setColour (juce::Colour (0x68ffffff));
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);

    g.setColour (juce::Colour (0x38000000));
    g.drawRoundedRectangle (panel.reduced (1.5f), 7.0f, 1.2f);

    auto display = juce::Rectangle<float> (panel.getCentreX() - 112.0f, panel.getY() + 25.0f, 224.0f, 28.0f);
    juce::ColourGradient displayGlass (juce::Colour (0x22000000), display.getTopLeft(),
                                       juce::Colour (0x46000000), display.getBottomRight(), false);
    g.setGradientFill (displayGlass);
    g.fillRoundedRectangle (display, 10.0f);
    g.setColour (juce::Colour (0x18ffffff));
    g.drawRoundedRectangle (display, 10.0f, 0.8f);

    g.setColour (juce::Colour (0x1fd7ecff));
    g.drawLine (display.getX() + 28.0f, display.getCentreY(), display.getRight() - 28.0f, display.getCentreY(), 1.0f);
}

void ResamplingDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (42, 32);
    auto header = area.removeFromTop (48);
    titleLabel.setBounds (header.removeFromLeft (332));
    subtitleLabel.setBounds (header.removeFromRight (210));

    area.removeFromTop (18);

    auto upper = area.removeFromTop (158);
    area.removeFromTop (16);
    auto lower = area.removeFromTop (158);

    const auto layoutSlider = [] (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
    {
        label.setBounds (bounds.removeFromTop (22));
        slider.setBounds (bounds.reduced (0, 1));
    };

    const auto layoutMode = [this] (juce::Rectangle<int> bounds)
    {
        modeLabel.setBounds (bounds.removeFromTop (22));
        modeBox.setBounds (bounds.withSizeKeepingCentre (142, 32));
    };

    const auto upperWidth = upper.getWidth() / 4;
    layoutMode (upper.removeFromLeft (upperWidth).reduced (12, 5));
    layoutSlider (delaySlider, delayLabel, upper.removeFromLeft (upperWidth).reduced (12, 5));
    layoutSlider (feedbackSlider, feedbackLabel, upper.removeFromLeft (upperWidth).reduced (12, 5));
    layoutSlider (mixSlider, mixLabel, upper.reduced (12, 5));

    const auto lowerCellWidth = upperWidth;
    const auto lowerControlsWidth = lowerCellWidth * 3;
    auto lowerControls = lower.withSizeKeepingCentre (lowerControlsWidth, lower.getHeight());
    layoutSlider (reverbSlider, reverbLabel, lowerControls.removeFromLeft (lowerCellWidth).reduced (12, 5));
    layoutSlider (lofiSlider, lofiLabel, lowerControls.removeFromLeft (lowerCellWidth).reduced (12, 5));
    layoutSlider (toneSlider, toneLabel, lowerControls.reduced (12, 5));
}

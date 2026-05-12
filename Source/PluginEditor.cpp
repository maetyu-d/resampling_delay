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
    const auto dotColour = slider.findColour (juce::Slider::rotarySliderFillColourId);
    g.setColour (dotColour.withAlpha ((juce::uint8) 0x44));
    g.fillEllipse (centre.x - dotRadius * 3.1f, centre.y - dotRadius * 3.1f, dotRadius * 6.2f, dotRadius * 6.2f);
    g.setColour (dotColour.brighter (0.08f));
    g.fillEllipse (centre.x - dotRadius, centre.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    g.setColour (juce::Colour (0xaaffffff));
    g.fillEllipse (centre.x - dotRadius * 0.40f, centre.y - dotRadius * 0.48f, dotRadius * 0.58f, dotRadius * 0.42f);
    g.setColour (juce::Colour (0x66000000));
    g.drawEllipse (centre.x - dotRadius, centre.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f, 1.0f);
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

    configureSlider (delaySlider, " ms");
    configureSlider (feedbackSlider);
    configureSlider (reverbSlider, " s");
    configureSlider (mixSlider);
    configureSlider (lofiSlider);
    configureSlider (toneSlider);

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
}

ResamplingDelayAudioProcessorEditor::~ResamplingDelayAudioProcessorEditor()
{
    for (auto* slider : { &delaySlider, &feedbackSlider, &reverbSlider, &mixSlider, &lofiSlider, &toneSlider })
        slider->setLookAndFeel (nullptr);
}

void ResamplingDelayAudioProcessorEditor::configureSlider (juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setLookAndFeel (&knobLookAndFeel);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 20);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff6ce0c7));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff252a2d));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xfff7f7f2));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff0f3f4));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
}

void ResamplingDelayAudioProcessorEditor::configureLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffdbe4e4));
    label.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    addAndMakeVisible (label);
}

void ResamplingDelayAudioProcessorEditor::setSliderAccent (juce::Slider& slider, juce::Colour accent)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, accent.withAlpha (0.18f));
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

    auto content = getLocalBounds().reduced (38, 30);
    content.removeFromTop (48);
    content.removeFromTop (12);
    auto upperWell = content.removeFromTop (168).toFloat().reduced (6.0f, 3.0f);
    auto lowerWell = content.removeFromTop (168).toFloat().reduced (6.0f, 3.0f);
    auto combinedWell = upperWell.getUnion (lowerWell).expanded (2.0f, 4.0f);

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

    juce::ColourGradient bottomShade (juce::Colours::transparentBlack, panel.getCentreX(), panel.getY() + panel.getHeight() * 0.35f,
                                      juce::Colour (0x74000000), panel.getCentreX(), panel.getBottom(), false);
    g.setGradientFill (bottomShade);
    g.fillRoundedRectangle (panel, 8.0f);

    g.setColour (juce::Colour (0x68ffffff));
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);

    g.setColour (juce::Colour (0x38000000));
    g.drawRoundedRectangle (panel.reduced (1.5f), 7.0f, 1.2f);

    auto display = juce::Rectangle<float> (panel.getCentreX() - 124.0f, panel.getY() + 25.0f, 248.0f, 28.0f);
    juce::ColourGradient displayGlass (juce::Colour (0x22000000), display.getTopLeft(),
                                       juce::Colour (0x46000000), display.getBottomRight(), false);
    g.setGradientFill (displayGlass);
    g.fillRoundedRectangle (display, 10.0f);
    g.setColour (juce::Colour (0x18ffffff));
    g.drawRoundedRectangle (display, 10.0f, 0.8f);

    auto meter = juce::Rectangle<float> (display.getCentreX() - 104.0f, display.getCentreY() - 4.5f, 208.0f, 9.0f);
    g.setColour (juce::Colour (0x34000000));
    g.fillRoundedRectangle (meter, 4.0f);
    g.setColour (juce::Colour (0x24ffffff));
    g.drawRoundedRectangle (meter, 4.0f, 0.7f);

    auto meterInner = meter.reduced (4.0f, 3.0f);
    auto leftMeter = meterInner.removeFromLeft ((meterInner.getWidth() - 4.0f) * 0.5f);
    meterInner.removeFromLeft (4.0f);
    auto rightMeter = meterInner;
    auto centreLine = meter.withWidth (1.0f).withCentre (meter.getCentre());
    g.setColour (juce::Colour (0x36d7ecff));
    g.fillRect (centreLine);

    g.setColour (juce::Colour (0xff42c7ff).withAlpha (0.56f));
    g.fillRoundedRectangle (leftMeter.withTrimmedLeft (leftMeter.getWidth() * 0.26f), 3.0f);
    g.fillRoundedRectangle (rightMeter.withTrimmedRight (rightMeter.getWidth() * 0.18f), 3.0f);
    g.setColour (juce::Colour (0xffeef5ff).withAlpha (0.62f));
    g.fillEllipse (meter.getCentreX() + 28.0f, meter.getCentreY() - 2.4f, 4.8f, 4.8f);
}

void ResamplingDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (38, 30);
    auto header = area.removeFromTop (48);
    titleLabel.setBounds (header.removeFromLeft (360));
    subtitleLabel.setBounds (header);

    area.removeFromTop (12);

    auto upper = area.removeFromTop (168);
    auto lower = area.removeFromTop (168);

    const auto layoutSlider = [] (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
    {
        label.setBounds (bounds.removeFromTop (24));
        slider.setBounds (bounds);
    };

    const auto layoutMode = [this] (juce::Rectangle<int> bounds)
    {
        modeLabel.setBounds (bounds.removeFromTop (24));
        modeBox.setBounds (bounds.reduced (16, 50));
    };

    const auto upperWidth = upper.getWidth() / 4;
    layoutMode (upper.removeFromLeft (upperWidth).reduced (8));
    layoutSlider (delaySlider, delayLabel, upper.removeFromLeft (upperWidth).reduced (8));
    layoutSlider (feedbackSlider, feedbackLabel, upper.removeFromLeft (upperWidth).reduced (8));
    layoutSlider (mixSlider, mixLabel, upper.reduced (8));

    const auto lowerWidth = lower.getWidth() / 5;
    lower.removeFromLeft (lowerWidth);
    layoutSlider (reverbSlider, reverbLabel, lower.removeFromLeft (lowerWidth).reduced (8));
    layoutSlider (lofiSlider, lofiLabel, lower.removeFromLeft (lowerWidth).reduced (8));
    layoutSlider (toneSlider, toneLabel, lower.removeFromLeft (lowerWidth).reduced (8));
}

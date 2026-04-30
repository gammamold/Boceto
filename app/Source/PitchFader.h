#pragma once
#include "JuceHeader.h"

/*  Horizontal pitch fader, SL-1200 style.
    Range cycles ±8% / ±16% / ±50%. Value displayed as percentage.
    Returns rate multiplier (1.0 + percent/100) via getRateMultiplier(). */
class PitchFader : public juce::Component
{
public:
    PitchFader()
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue (true, 0.0);
        applyRange();
        slider.onValueChange = [this]
        {
            updateLabel();
            if (onChange) onChange (getRateMultiplier());
        };
        addAndMakeVisible (slider);

        valueLabel.setJustificationType (juce::Justification::centredRight);
        valueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (valueLabel);

        rangeLabel.setJustificationType (juce::Justification::centredLeft);
        rangeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (rangeLabel);

        updateLabel();
    }

    /** rangeIndex: 0 → ±8, 1 → ±16, 2 → ±50 */
    void cycleRange()
    {
        rangeIndex = (rangeIndex + 1) % 3;
        applyRange();
    }

    int getRangePercent() const
    {
        static const int r[] = { 8, 16, 50 };
        return r[rangeIndex];
    }

    /** Playback rate multiplier — e.g. 1.024 for +2.4% */
    double getRateMultiplier() const
    {
        return 1.0 + slider.getValue() / 100.0;
    }

    void resetToZero()
    {
        slider.setValue (0.0, juce::sendNotificationSync);
    }

    /** Move the fader to match a rate multiplier without triggering onChange.
        Auto-expands the range if the value falls outside it. */
    void setRateMultiplier (double mult)
    {
        const double percent = (mult - 1.0) * 100.0;
        while (std::abs (percent) > (double) getRangePercent() && rangeIndex < 2)
            cycleRange();
        slider.setValue (percent, juce::dontSendNotification);
        updateLabel();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        rangeLabel.setBounds (r.removeFromLeft (70));
        valueLabel.setBounds (r.removeFromRight (80));
        slider    .setBounds (r.reduced (4, 0));
    }

    std::function<void (double rateMultiplier)> onChange;

private:
    void applyRange()
    {
        const auto current = slider.getValue();
        const auto r = (double) getRangePercent();
        slider.setRange (-r, r, 0.01);
        slider.setValue (juce::jlimit (-r, r, current), juce::sendNotificationSync);
        rangeLabel.setText ("±" + juce::String (getRangePercent()) + "%",
                            juce::dontSendNotification);
    }

    void updateLabel()
    {
        const auto v = slider.getValue();
        const auto sign = v >= 0.0 ? "+" : "";
        valueLabel.setText (sign + juce::String (v, 2) + "%",
                            juce::dontSendNotification);
    }

    juce::Slider slider;
    juce::Label  valueLabel;
    juce::Label  rangeLabel;
    int          rangeIndex = 0;
};

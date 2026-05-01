#pragma once
#include "JuceHeader.h"
#include "EffectsChain.h"

/*  Full-screen "room" for effects controls. Owns its own dark background.
    Toggle visibility from MainComponent. v1 has a single FILTER slider
    (DJ-style: center bypass, sweep LP left / HP right) plus BACK and a
    label showing the current cutoff. */
class EfxView : public juce::Component
{
public:
    explicit EfxView (EffectsChain& fxChain) : effects (fxChain)
    {
        backBtn.setButtonText ("BACK");
        backBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (backBtn);

        title.setText ("EFFECTS", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (title);

        filterLabel.setText ("FILTER", juce::dontSendNotification);
        filterLabel.setJustificationType (juce::Justification::centredLeft);
        filterLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (filterLabel);

        filterValue.setJustificationType (juce::Justification::centredRight);
        filterValue.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (filterValue);

        filterSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        filterSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        filterSlider.setRange (-1.0, 1.0, 0.001);
        filterSlider.setValue (0.0, juce::dontSendNotification);
        filterSlider.setDoubleClickReturnValue (true, 0.0);
        filterSlider.onValueChange = [this]
        {
            effects.setFilterPosition (filterSlider.getValue());
            updateFilterLabel();
        };
        addAndMakeVisible (filterSlider);

        resetBtn.setButtonText ("BYPASS");
        resetBtn.onClick = [this]
        {
            filterSlider.setValue (0.0, juce::sendNotificationSync);
        };
        addAndMakeVisible (resetBtn);

        updateFilterLabel();
    }

    /** Called when BACK is tapped. */
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff141414));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);

        auto topRow = r.removeFromTop (44);
        backBtn.setBounds (topRow.removeFromLeft (90));
        topRow.removeFromLeft (8);
        title.setBounds (topRow);
        r.removeFromTop (24);

        // FILTER block
        auto labelRow = r.removeFromTop (28);
        filterLabel.setBounds (labelRow.removeFromLeft (110));
        filterValue.setBounds (labelRow.removeFromRight (160));

        r.removeFromTop (4);
        filterSlider.setBounds (r.removeFromTop (52).reduced (4, 0));

        r.removeFromTop (8);
        resetBtn.setBounds (r.removeFromTop (36).reduced (40, 0));
    }

    /** Pull current state from the chain (e.g. when re-entering view). */
    void syncFromChain()
    {
        filterSlider.setValue (effects.getFilterPosition(), juce::dontSendNotification);
        updateFilterLabel();
    }

private:
    void updateFilterLabel()
    {
        filterValue.setText (effects.getFilterDescription(), juce::dontSendNotification);
    }

    EffectsChain&     effects;
    juce::TextButton  backBtn;
    juce::TextButton  resetBtn;
    juce::Label       title;
    juce::Label       filterLabel;
    juce::Label       filterValue;
    juce::Slider      filterSlider;

    JUCE_DECLARE_NON_COPYABLE (EfxView)
};

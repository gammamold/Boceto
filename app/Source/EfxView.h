#pragma once
#include "JuceHeader.h"
#include "EffectsChain.h"

/*  Full-screen "room" for effects controls. Three sections — FILTER,
    DELAY, CHORUS — each section header + the relevant sliders. */
class EfxView : public juce::Component
{
public:
    explicit EfxView (EffectsChain& fxChain) : effects (fxChain)
    {
        addAndMakeVisible (backBtn);
        backBtn.setButtonText ("BACK");
        backBtn.onClick = [this] { if (onClose) onClose(); };

        addAndMakeVisible (title);
        title.setText ("EFFECTS", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        title.setColour (juce::Label::textColourId, juce::Colours::white);

        // ── Filter ───────────────────────────────────────────────
        setupSectionHeader (filterHeader, "FILTER");
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
            filterValue.setText (effects.getFilterDescription(), juce::dontSendNotification);
        };
        addAndMakeVisible (filterSlider);

        // ── Delay ────────────────────────────────────────────────
        setupSectionHeader (delayHeader, "DELAY");
        setupParamSlider (delayTime,    "TIME",    "ms",   10.0,  1500.0, 1.0,  effects.getDelayTimeMs(),
            [this] (double v) { effects.setDelayTimeMs (v); });
        setupParamSlider (delayFeedback,"FB",      "",     0.0,   0.92,   0.01, effects.getDelayFeedback(),
            [this] (double v) { effects.setDelayFeedback (v); });
        setupParamSlider (delayMix,     "MIX",     "",     0.0,   1.0,    0.01, effects.getDelayMix(),
            [this] (double v) { effects.setDelayMix (v); });

        // ── Chorus ───────────────────────────────────────────────
        setupSectionHeader (chorusHeader, "CHORUS");
        setupParamSlider (chorusRate,   "RATE",    "Hz",   0.1,   5.0,    0.01, effects.getChorusRate(),
            [this] (double v) { effects.setChorusRate (v); });
        setupParamSlider (chorusDepth,  "DEPTH",   "",     0.0,   1.0,    0.01, effects.getChorusDepth(),
            [this] (double v) { effects.setChorusDepth (v); });
        setupParamSlider (chorusMix,    "MIX",     "",     0.0,   1.0,    0.01, effects.getChorusMix(),
            [this] (double v) { effects.setChorusMix (v); });

        syncFromChain();
    }

    std::function<void()> onClose;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff141414));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);

        auto top = r.removeFromTop (44);
        backBtn.setBounds (top.removeFromLeft (90));
        top.removeFromLeft (8);
        title.setBounds (top);
        r.removeFromTop (16);

        // Filter section
        filterHeader.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        auto fRow = r.removeFromTop (44);
        filterValue.setBounds (fRow.removeFromRight (140));
        filterSlider.setBounds (fRow.reduced (4, 0));
        r.removeFromTop (12);

        // Delay section
        delayHeader.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        delayTime    .layoutInto (r.removeFromTop (36));
        r.removeFromTop (4);
        delayFeedback.layoutInto (r.removeFromTop (36));
        r.removeFromTop (4);
        delayMix     .layoutInto (r.removeFromTop (36));
        r.removeFromTop (12);

        // Chorus section
        chorusHeader.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        chorusRate .layoutInto (r.removeFromTop (36));
        r.removeFromTop (4);
        chorusDepth.layoutInto (r.removeFromTop (36));
        r.removeFromTop (4);
        chorusMix  .layoutInto (r.removeFromTop (36));
    }

    void syncFromChain()
    {
        filterSlider.setValue (effects.getFilterPosition(), juce::dontSendNotification);
        filterValue.setText (effects.getFilterDescription(), juce::dontSendNotification);
        delayTime    .setValueQuiet (effects.getDelayTimeMs());
        delayFeedback.setValueQuiet (effects.getDelayFeedback());
        delayMix     .setValueQuiet (effects.getDelayMix());
        chorusRate   .setValueQuiet (effects.getChorusRate());
        chorusDepth  .setValueQuiet (effects.getChorusDepth());
        chorusMix    .setValueQuiet (effects.getChorusMix());
    }

private:
    /*  A small labelled horizontal slider component so each row in the EFX
        panel is consistent: [name] [slider] [value].  */
    struct ParamRow
    {
        juce::Label  name;
        juce::Slider slider;
        juce::Label  value;
        juce::String unit;
        std::function<void(double)> onChange;

        void layoutInto (juce::Rectangle<int> r)
        {
            name .setBounds (r.removeFromLeft  (70));
            value.setBounds (r.removeFromRight (90));
            slider.setBounds (r.reduced (4, 0));
        }

        void setValueQuiet (double v)
        {
            slider.setValue (v, juce::dontSendNotification);
            updateValueText();
        }

        void updateValueText()
        {
            const auto v = slider.getValue();
            value.setText (juce::String (v, slider.getInterval() < 1.0 ? 2 : 0)
                            + (unit.isEmpty() ? juce::String() : " " + unit),
                           juce::dontSendNotification);
        }
    };

    void setupSectionHeader (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (juce::Font (juce::FontOptions (16.0f).withStyle ("Bold")));
        l.setColour (juce::Label::textColourId, juce::Colours::orange);
        addAndMakeVisible (l);
    }

    void setupParamSlider (ParamRow& row, const juce::String& name, const juce::String& unit,
                           double minV, double maxV, double step, double initial,
                           std::function<void(double)> onChange)
    {
        row.name.setText (name, juce::dontSendNotification);
        row.name.setJustificationType (juce::Justification::centredLeft);
        row.name.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (row.name);

        row.value.setJustificationType (juce::Justification::centredRight);
        row.value.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (row.value);

        row.unit = unit;
        row.onChange = std::move (onChange);
        row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        row.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row.slider.setRange (minV, maxV, step);
        row.slider.setValue (initial, juce::dontSendNotification);
        row.slider.onValueChange = [this, &row]
        {
            if (row.onChange) row.onChange (row.slider.getValue());
            row.updateValueText();
        };
        addAndMakeVisible (row.slider);
        row.updateValueText();
    }

    EffectsChain&    effects;
    juce::TextButton backBtn;
    juce::Label      title;

    juce::Label  filterHeader;
    juce::Label  filterValue;
    juce::Slider filterSlider;

    juce::Label  delayHeader;
    ParamRow     delayTime;
    ParamRow     delayFeedback;
    ParamRow     delayMix;

    juce::Label  chorusHeader;
    ParamRow     chorusRate;
    ParamRow     chorusDepth;
    ParamRow     chorusMix;

    JUCE_DECLARE_NON_COPYABLE (EfxView)
};

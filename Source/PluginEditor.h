#pragma once
#include "PluginProcessor.h"
#include <functional>

#define RU(s) juce::String::fromUTF8 (s)
using Fmt = std::function<juce::String (double)>;

// Банка-крутилка: уровень "газировки" = значение параметра
class CanKnob : public juce::Slider
{
public:
    CanKnob (juce::String flavor, juce::Colour body, juce::Colour accent, juce::Colour liquid, Fmt f);
    void paint (juce::Graphics&) override;
private:
    juce::String flavorName; juce::Colour bodyC, accentC, liquidC; Fmt fmt;
};

// Крышка банки с кольцом — главная ручка DRIVE
class LidKnob : public juce::Slider
{
public:
    explicit LidKnob (Fmt f);
    void paint (juce::Graphics&) override;
private:
    Fmt fmt;
};

// Малая ручка-крышечка
class CapKnob : public juce::Slider
{
public:
    CapKnob (juce::String name, juce::Colour cap, Fmt f);
    void paint (juce::Graphics&) override;
private:
    juce::String label; juce::Colour capC; Fmt fmt;
};

class TubeMeter : public juce::Component
{
public:
    void setLevel (float v) { level = v; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float level = 0.f;
};

class GazyavaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GazyavaEditor (GazyavaProcessor&);
    ~GazyavaEditor() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;

    GazyavaProcessor& proc;
    juce::Image catL, catR;
    LidKnob lid;
    CanKnob canBass, canCrunch, canIce;
    CapKnob kIn, kFreq, kOvl, kWidth, kMix, kOut;
    TubeMeter tubeL, tubeR;
    juce::TextButton modeBtn[4], limBtn, monoBtn;
    float lvIn = 0.f, lvOut = 0.f;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    std::unique_ptr<SA> aLid, aBass, aCrunch, aIce, aIn, aFreq, aOvl, aWidth, aMix, aOut;
    std::unique_ptr<BA> aLim, aMono;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GazyavaEditor)
};

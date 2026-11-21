#pragma once
#include <JuceHeader.h>

class GazyavaProcessor : public juce::AudioProcessor
{
public:
    GazyavaProcessor();
    ~GazyavaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Gazyava"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static float shape (float x, float k, int mode) noexcept;
    void updateTargets (bool snap);
    float getP (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::dsp::LinkwitzRileyFilter<float> xover;
    juce::dsp::StateVariableTPTFilter<float> tone;
    juce::dsp::Limiter<float> limiter;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::AudioBuffer<float> dryBuf;
    juce::SmoothedValue<float> sIn, sDrive, sOvl, sBass, sCrunch, sWidth, sMix, sOut;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GazyavaProcessor)
};

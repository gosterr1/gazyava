#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout GazyavaProcessor::createLayout()
{
    using NR = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&p] (const char* id, const char* name, NR r, float def)
    { p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def)); };

    add ("in",       "Input",     NR (-12.f, 12.f, 0.01f), 0.f);
    add ("drive",    "Drive",     NR (0.f, 1.f, 0.001f),   0.4f);
    add ("overload", "Overload",  NR (0.f, 1.f, 0.001f),   0.f);
    add ("bass",     "Bass",      NR (0.f, 1.f, 0.001f),   0.4f);
    NR fr (40.f, 300.f, 0.1f); fr.setSkewForCentre (100.f);
    add ("freq",     "Crossover", fr, 100.f);
    add ("crunch",   "Crunch",    NR (0.f, 1.f, 0.001f),   0.2f);
    add ("ice",      "Ice Tone",  NR (0.f, 1.f, 0.001f),   0.f);
    add ("width",    "Width",     NR (0.f, 1.f, 0.001f),   0.5f);
    add ("mix",      "Mix",       NR (0.f, 1.f, 0.001f),   1.f);
    add ("out",      "Output",    NR (-18.f, 6.f, 0.01f),  0.f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode",
                 juce::StringArray { "Tube", "Tape", "Fuzz", "Fold" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "limiter", 1 }, "Limiter", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "monobass", 1 }, "Mono Bass", false));
    return { p.begin(), p.end() };
}

GazyavaProcessor::GazyavaProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GAZYAVA", createLayout())
{
}

bool GazyavaProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float GazyavaProcessor::shape (float x, float k, int mode) noexcept
{
    switch (mode)
    {
        case 0:  return (std::tanh (k * x + 0.2f) - 0.19738f) / std::tanh (k * 0.9f);              // tube (asymmetric)
        case 1:  return std::atan (k * x * 1.2f) / std::atan (k * 1.2f);                           // tape
        case 2:  { const float a = 1.f - std::exp (-std::abs (x) * k * 1.4f);                      // fuzz
                   return (x < 0 ? -a : a) / (1.f - std::exp (-k * 1.4f)); }
        default: return std::sin (x * (1.f + k * 0.35f) * 1.2f);                                   // wavefold
    }
}

void GazyavaProcessor::updateTargets (bool snap)
{
    auto set = [snap] (juce::SmoothedValue<float>& s, float v)
    { if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v); };
    set (sIn,     juce::Decibels::decibelsToGain (getP ("in")));
    set (sDrive,  getP ("drive"));
    set (sOvl,    getP ("overload"));
    set (sBass,   juce::Decibels::decibelsToGain (getP ("bass") * 18.f));
    set (sCrunch, getP ("crunch"));
    set (sWidth,  getP ("width") * 2.f);
    set (sMix,    getP ("mix"));
    set (sOut,    juce::Decibels::decibelsToGain (getP ("out")));
}

void GazyavaProcessor::prepareToPlay (double sr, int spb)
{
    sampleRate = sr;
    const double osr = sr * 4.0;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) spb, 2 };
    juce::dsp::ProcessSpec ospec { osr, (juce::uint32) (spb * 4), 2 };

    oversampling.initProcessing ((size_t) spb);
    oversampling.reset();
    xover.prepare (ospec);
    tone.prepare (spec);
    tone.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    limiter.prepare (spec);
    limiter.setThreshold (-0.5f);
    limiter.setRelease (60.f);

    const int lat = (int) std::round (oversampling.getLatencyInSamples());
    setLatencySamples (lat);
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) lat);
    dryBuf.setSize (2, spb);

    for (auto* s : { &sIn, &sDrive, &sOvl, &sBass, &sCrunch, &sWidth }) s->reset (osr, 0.02);
    for (auto* s : { &sMix, &sOut }) s->reset (sr, 0.03);
    updateTargets (true);
}

void GazyavaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0) return;

    updateTargets (false);
    const int  mode     = (int) getP ("mode");
    const bool monoBass = getP ("monobass") > 0.5f;
    const bool limOn    = getP ("limiter") > 0.5f;
    const float ice     = getP ("ice");
    xover.setCutoffFrequency (getP ("freq"));

    const float inPk = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n));
    inLevel.store (juce::jmax (inLevel.load(), inPk));

    // dry copy delayed by oversampler latency (keeps MIX phase-aligned)
    dryBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < 2; ++c)
    {
        auto* src = buffer.getReadPointer (c);
        auto* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i) { dryDelay.pushSample (c, src[i]); d[i] = dryDelay.popSample (c); }
    }

    juce::dsp::AudioBlock<float> blk (buffer);
    auto sub = blk.getSubsetChannelBlock (0, 2).getSubBlock (0, (size_t) n);
    auto up = oversampling.processSamplesUp (sub);
    float* ch[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
    const int nu = (int) up.getNumSamples();

    for (int i = 0; i < nu; ++i)
    {
        const float inG = sIn.getNextValue(), dr = sDrive.getNextValue(), ov = sOvl.getNextValue(),
                    bg = sBass.getNextValue(), cr = sCrunch.getNextValue(), wd = sWidth.getNextValue();
        const float k = 1.f + dr * 14.f;
        float lo[2], hi[2];
        for (int c = 0; c < 2; ++c) xover.processSample (c, ch[c][i] * inG, lo[c], hi[c]);
        if (monoBass) lo[0] = lo[1] = 0.5f * (lo[0] + lo[1]);
        { const float m = 0.5f * (hi[0] + hi[1]), s = 0.5f * (hi[0] - hi[1]) * wd; hi[0] = m + s; hi[1] = m - s; }

        for (int c = 0; c < 2; ++c)
        {
            float s = shape (lo[c], k, mode) * bg;                       // saturated + boosted bass
            float h = hi[c];
            if (cr > 0.001f)                                              // crunch: harmonics on the highs
            { const float hs = shape (h * (1.f + cr * 3.f), 1.f + cr * 10.f, mode); h += (hs - h) * cr; }
            s += h;
            if (ov > 0.001f)                                              // overload: hard-clip stage
            {
                const float y = juce::jlimit (-1.f, 1.f, s * (1.f + ov * 18.f));
                s += (y * 0.85f - s) * juce::jmin (1.f, ov * 3.f);
            }
            ch[c][i] = s;
        }
    }
    oversampling.processSamplesDown (sub);

    if (ice > 0.01f)                                                      // ice: dark/cool tone
    {
        tone.setCutoffFrequency (juce::jmin (20000.f * std::pow (0.075f, ice), 0.45f * (float) sampleRate));
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        tone.process (ctx);
    }

    for (int i = 0; i < n; ++i)
    {
        const float m = sMix.getNextValue(), og = sOut.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            auto* o = buffer.getWritePointer (c);
            o[i] = (dryBuf.getSample (c, i) * (1.f - m) + o[i] * m) * og;
        }
    }
    if (limOn) { juce::dsp::ProcessContextReplacing<float> ctx (sub); limiter.process (ctx); }

    const float outPk = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n));
    outLevel.store (juce::jmax (outLevel.load(), outPk));
}

juce::AudioProcessorEditor* GazyavaProcessor::createEditor() { return new GazyavaEditor (*this); }

void GazyavaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void GazyavaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GazyavaProcessor(); }

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

MeteoraTapeProcessor::MeteoraTapeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MeteoraTapeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // INPUT GAIN: fino a +24 dB per spingere il nastro in saturazione (0 dB a meta' corsa, sul 5)
    juce::NormalisableRange<float> inR (-12.0f, 24.0f); inR.setSkewForCentre (0.0f);
    // OUTPUT: make-up, fino a -24 dB per compensare l'input spinto (0 dB a meta' corsa)
    juce::NormalisableRange<float> outR (-24.0f, 12.0f); outR.setSkewForCentre (0.0f);
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "in", 1 },   "Input Gain",   inR, 0.0f));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 },  "Dry/Wet",      juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out", 1 },  "Output",       outR, 0.0f));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "hiss", 1 }, "Hiss",         juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "ips", 1 }, "Tape Speed", juce::StringArray { "7.5 IPS", "15 IPS", "30 IPS" }, 1));
    return l;
}

bool MeteoraTapeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == l.getMainInputChannelSet();
}

void MeteoraTapeProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sr);
    setLatencySamples (engine.latencySamples());
}

void MeteoraTapeProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    bypassed.store (false);
    lastProcessMs.store (juce::Time::getMillisecondCounter());
    const int n = buf.getNumSamples();
    const int nch = juce::jmin (2, buf.getNumChannels());
    if (nch == 0 || n == 0) return;

    engine.setParams (P ("in"), P ("out"), P ("mix") / 100.0, P ("hiss"), (int) std::lround (P ("ips")));
    double inS = 0, outS = 0, inP = 0, outP = 0;

    float* l = buf.getWritePointer (0);
    float* r = nch > 1 ? buf.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        float x[2] = { l[i], r != nullptr ? r[i] : l[i] };
        engine.process (x, 1.0);
        inS += engine.inSq; outS += engine.outSq;
        inP = juce::jmax (inP, engine.inPk); outP = juce::jmax (outP, engine.outPk);
        l[i] = x[0];
        if (r != nullptr) r[i] = x[1];
    }
    // VU: 0 VU = -18 dBFS RMS
    inVu.store ((float) (10.0 * std::log10 (juce::jmax (inS / n, 1.0e-12)) + 18.0));
    outVu.store ((float) (10.0 * std::log10 (juce::jmax (outS / n, 1.0e-12)) + 18.0));
    inPeakDb.store ((float) (20.0 * std::log10 (juce::jmax (inP, 1.0e-6))));
    outPeakDb.store ((float) (20.0 * std::log10 (juce::jmax (outP, 1.0e-6))));
}

juce::AudioProcessorEditor* MeteoraTapeProcessor::createEditor() { return new MeteoraTapeEditor (*this); }

void MeteoraTapeProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, d);
}

void MeteoraTapeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MeteoraTapeProcessor(); }

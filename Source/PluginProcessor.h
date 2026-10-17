#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "MeteoraTapeEngine.h"

class MeteoraTapeProcessor : public juce::AudioProcessor
{
public:
    MeteoraTapeProcessor();
    ~MeteoraTapeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override
    {
        bypassed.store (true);
        lastProcessMs.store (juce::Time::getMillisecondCounter());
        juce::AudioProcessor::processBlockBypassed (b, m);
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Meteora Tape"; }
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

    // acceso = la DAW fa passare l'audio nel plugin e il plugin non e' in bypass (i nastri girano)
    bool isRunning() const
    {
        return ! bypassed.load() && juce::Time::getMillisecondCounter() - lastProcessMs.load() < 500u;
    }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inVu { -60.0f }, outVu { -60.0f }, inPeakDb { -100.0f }, outPeakDb { -100.0f };
    std::atomic<juce::uint32> lastProcessMs { 0 };
    std::atomic<bool> bypassed { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float P (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    MeteoraTapeEngine engine;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeteoraTapeProcessor)
};

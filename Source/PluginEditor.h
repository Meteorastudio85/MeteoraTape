#pragma once
// ===========================================================================
//  Meteora Tape - interfaccia "J-47 TAPE" (Meteora Plugins)
// ===========================================================================
#include "PluginProcessor.h"
#include <array>
#include <memory>
#include <vector>

// le manopole sono disegnate dall'editor: lo slider resta invisibile
class TapeInvisibleLnF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override {}
};

class MeteoraTapeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MeteoraTapeEditor (MeteoraTapeProcessor&);
    ~MeteoraTapeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Reel { juce::Image face, faceScaled, light, hub; float cx, cy, a, b, hx, hy, hw, hh; };

    void timerCallback() override;
    void drawReel (juce::Graphics&, const Reel&) const;
    juce::String valueText (int i) const;
    float P (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setIps (int v);

    MeteoraTapeProcessor& proc;
    TapeInvisibleLnF lnf;
    juce::Image bg, bgScaled, bgHi, glowImg;      // bgHi: doppia densita' per le parti che ruotano (piu' nitide)
    float hiK = 2.0f;
    std::array<Reel, 2> reels;
    float bgK = 1.0f;
    bool sizeReady = false;
    std::array<juce::Slider, 2> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> atts;
    int lastKnob = -1, lastIps = -1, showIps = 0;
    float glow = 0.0f, tube = 0.0f, flick = 1.0f, inNeedle = -40.0f, outNeedle = -40.0f;
    float reelAngle = 0.0f, reelSpeed = 0.0f, ipsAngle = 0.0f, satRed = 0.0f;
    // INPUT e OUTPUT collegati: alzando l'input l'output scende della stessa quantita'
    bool linkDrag = false;
    float linkIn0 = 0.0f, linkOut0 = 0.0f, prevIn = 0.0f;
    void linkOutput (float newIn);
    juce::Random rnd;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeteoraTapeEditor)
};

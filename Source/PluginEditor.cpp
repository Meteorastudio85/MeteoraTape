#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
    constexpr float kW = 1376.0f, kH = 768.0f;
    constexpr int kDefaultWidth = 1080;
    constexpr const char* kSizeKey = "uiWidthV2";

    // manopole che ruotano (foto ritagliata e girata): INPUT GAIN e SATURATION
    struct TapeKnob { const char* id; const char* name; float cx, cy, r; };
    const TapeKnob kKnobs[2] = {
        { "in",  "Input Gain",  710.0f, 573.0f, 58.0f },
        { "out", "Output",      936.0f, 579.0f, 55.0f },
    };
    constexpr float kSweep = 145.0f;                      // corsa delle manopole: -145 ... +145 gradi

    // pomello TAPE SPEED (stesso pomello degli altri due): scatta su 7.5 / 15 / 30
    constexpr float kSelX = 1166.0f, kSelY = 574.0f, kSelR = 47.0f;
    const float kSelAngle[3] = { -37.0f, 0.0f, 38.0f };

    // VU: perni, raggio della lancetta, 0 VU = -18 dBFS
    const float kVuX[2] = { 232.0f, 472.0f };
    constexpr float kVuY = 593.0f;

    // lampadine dei canali in alto
    constexpr float kLampY = 64.0f;
}

MeteoraTapeEditor::MeteoraTapeEditor (MeteoraTapeProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    bg = juce::ImageCache::getFromMemory (BinaryData::ui_jpg, BinaryData::ui_jpgSize);
    glowImg = juce::ImageCache::getFromMemory (BinaryData::logo_glow_png, BinaryData::logo_glow_pngSize);
    reels[0] = { juce::ImageCache::getFromMemory (BinaryData::reelL_face_jpg, BinaryData::reelL_face_jpgSize), {},
                 juce::ImageCache::getFromMemory (BinaryData::reelL_light_png, BinaryData::reelL_light_pngSize),
                 juce::ImageCache::getFromMemory (BinaryData::hubL_png, BinaryData::hubL_pngSize),
                 398.0f, 185.0f, 248.0f, 84.0f, 326.0f, 76.0f, 148.5f, 125.5f };
    reels[1] = { juce::ImageCache::getFromMemory (BinaryData::reelR_face_jpg, BinaryData::reelR_face_jpgSize), {},
                 juce::ImageCache::getFromMemory (BinaryData::reelR_light_png, BinaryData::reelR_light_pngSize),
                 juce::ImageCache::getFromMemory (BinaryData::hubR_png, BinaryData::hubR_pngSize),
                 976.0f, 185.0f, 248.0f, 84.0f, 907.0f, 76.0f, 148.5f, 125.5f };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& s = knobs[i];
        s.setLookAndFeel (&lnf);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity (240);
        addAndMakeVisible (s);
        atts.push_back (std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (kKnobs[i].id), s));
        s.onValueChange = [this, i]
        {
            lastKnob = (int) i; showIps = 0;
            if (i == 0) linkOutput ((float) knobs[0].getValue());
            repaint();
        };
    }
    // collegamento INPUT -> OUTPUT durante il trascinamento (riferito al punto di partenza: nessuna deriva)
    knobs[0].onDragStart = [this]
    {
        linkDrag = true; linkIn0 = P ("in"); linkOut0 = P ("out");
        if (auto* o = proc.apvts.getParameter ("out")) o->beginChangeGesture();
    };
    knobs[0].onDragEnd = [this]
    {
        linkDrag = false;
        if (auto* o = proc.apvts.getParameter ("out")) o->endChangeGesture();
    };
    prevIn = P ("in");
    lastIps = juce::jlimit (0, 2, (int) std::lround (P ("ips")));
    ipsAngle = kSelAngle[lastIps];

    // dimensione d'apertura (letta prima dei limiti, che fanno subito un primo ridimensionamento)
    const int w = juce::jlimit (688, 2064, (int) proc.apvts.state.getProperty (kSizeKey, kDefaultWidth));
    setResizable (true, true);
    setResizeLimits (688, 384, 2064, 1152);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (w, juce::roundToInt ((float) w * kH / kW));
    sizeReady = true;
    resized();
    startTimerHz (30);
}

MeteoraTapeEditor::~MeteoraTapeEditor()
{
    for (auto& s : knobs) s.setLookAndFeel (nullptr);
}

void MeteoraTapeEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    if (sizeReady) proc.apvts.state.setProperty (kSizeKey, getWidth(), nullptr);
    bgK = juce::jmax (1.0f, juce::Component::getApproximateScaleFactorForComponent (this));
    bgScaled = bg.rescaled (juce::roundToInt ((float) getWidth() * bgK), juce::roundToInt ((float) getHeight() * bgK),
                            juce::Graphics::highResamplingQuality);
    // pomelli e bobine ruotano partendo da un'immagine a densita' doppia: la rotazione non impasta i dettagli
    const int hiW = juce::jmin (bg.getWidth(), juce::roundToInt ((float) getWidth() * bgK * 2.0f));
    bgHi = hiW >= bg.getWidth() ? bg : bg.rescaled (hiW, juce::roundToInt ((float) hiW * kH / kW), juce::Graphics::highResamplingQuality);
    hiK = (float) bgHi.getWidth() / (float) getWidth();
    const float s = (float) getWidth() / kW;
    for (auto& r : reels)
    {
        const int n = juce::jmin (r.face.getWidth(), juce::jmax (8, juce::roundToInt (2.0f * r.a * s * bgK * 2.0f)));
        r.faceScaled = n >= r.face.getWidth() ? r.face : r.face.rescaled (n, n, juce::Graphics::highResamplingQuality);
    }
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = kKnobs[i];
        knobs[i].setBounds (juce::Rectangle<float> ((k.cx - k.r) * s, (k.cy - k.r) * s, 2.0f * k.r * s, 2.0f * k.r * s).toNearestInt());
    }
}

void MeteoraTapeEditor::drawReel (juce::Graphics& g, const Reel& r) const
{
    const float s = (float) getWidth() / kW;
    {
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path face;
    face.addEllipse ((r.cx - r.a) * s, (r.cy - r.b) * s, 2.0f * r.a * s, 2.0f * r.b * s);
    g.reduceClipRegion (face);
    // la faccia della bobina e' un cerchio: si gira e poi si schiaccia in prospettiva
    const float n = (float) r.faceScaled.getWidth();
    const float faceK = n / (2.0f * r.a * s);          // pixel della texture per unita' della finestra
    const auto t = juce::AffineTransform::translation (-n * 0.5f, -n * 0.5f)
                       .scaled (1.0f / faceK)
                       .rotated (reelAngle)
                       .scaled (1.0f, r.b / r.a)
                       .translated (r.cx * s, r.cy * s);
    g.drawImageTransformed (r.faceScaled, t);
    // luce e riflessi della foto: restano fermi mentre la bobina gira
    g.drawImage (r.light, juce::Rectangle<float> ((r.cx - r.a) * s, (r.cy - r.b) * s, 2.0f * r.a * s, 2.0f * r.b * s));
    }
    // mozzo fermo sopra
    g.drawImage (r.hub, juce::Rectangle<float> (r.hx * s, r.hy * s, r.hw * s, r.hh * s));
}

juce::String MeteoraTapeEditor::valueText (int i) const
{
    const double v = knobs[(size_t) i].getValue();
    return (v >= 0.0 ? "+" : "") + juce::String (v, 1) + " dB";
}

void MeteoraTapeEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / kW;
    auto sc = [s] (float v) { return v * s; };
    const auto base = juce::AffineTransform::scale (1.0f / bgK);
    const auto baseHi = juce::AffineTransform::scale (1.0f / hiK);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (bgScaled, base);

    // bobine che girano
    for (const auto& r : reels) drawReel (g, r);

    // VU dei canali in alto e loro lampadine: accesi solo quando il plugin lavora
    for (int i = 0; i < 8; ++i)
    {
        const float lx = 207.5f + 125.0f * (float) i;
        if (glow < 0.999f)
        {
            g.setColour (juce::Colour (0xff0c0a08).withAlpha (0.62f * (1.0f - glow)));
            g.fillRect (juce::Rectangle<float> (sc (lx - 13.0f), 0.0f, sc (106.0f), sc (46.0f)));
            g.setColour (juce::Colour (0xff15110c).withAlpha (0.8f * (1.0f - glow)));
            g.fillRect (juce::Rectangle<float> (sc (lx - 6.5f), sc (kLampY - 6.5f), sc (13.0f), sc (13.0f)));
        }
        if (glow > 0.001f)
        {
            juce::ColourGradient cg (juce::Colour (0xffffb347).withAlpha (0.45f * glow), sc (lx), sc (kLampY),
                                     juce::Colour (0xffffb347).withAlpha (0.0f), sc (lx + 16.0f), sc (kLampY), true);
            g.setGradientFill (cg);
            g.fillEllipse (sc (lx - 16.0f), sc (kLampY - 16.0f), sc (32.0f), sc (32.0f));
        }
    }

    // manopole INPUT GAIN e SATURATION: la foto della manopola gira
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = kKnobs[i];
        const double prop = knobs[i].valueToProportionOfLength (knobs[i].getValue());
        const float deg = (float) (-kSweep + 2.0 * kSweep * prop);
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path c;
        c.addEllipse (sc (k.cx - k.r), sc (k.cy - k.r), sc (2.0f * k.r), sc (2.0f * k.r));
        g.reduceClipRegion (c);
        g.drawImageTransformed (bgHi, baseHi.rotated (juce::degreesToRadians (deg), sc (k.cx), sc (k.cy)));
    }

    // pomello TAPE SPEED
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path c;
        c.addEllipse (sc (kSelX - kSelR), sc (kSelY - kSelR), sc (2.0f * kSelR), sc (2.0f * kSelR));
        g.reduceClipRegion (c);
        g.drawImageTransformed (bgHi, baseHi.rotated (juce::degreesToRadians (ipsAngle), sc (kSelX), sc (kSelY)));
    }

    // VU INPUT e OUTPUT: quadranti illuminati solo quando lavora, lancette vere
    for (int m = 0; m < 2; ++m)
    {
        const float px = kVuX[m];
        if (glow < 0.999f)
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path face;
            face.addEllipse (sc (px - 86.0f), sc (kVuY - 133.0f), sc (172.0f), sc (172.0f));
            g.reduceClipRegion (face);
            g.setColour (juce::Colour (0xff0e0b08).withAlpha (0.66f * (1.0f - glow)));
            g.fillRect (getLocalBounds());
        }
        // VU OUTPUT: arrossisce quando il nastro satura
        if (m == 1 && satRed * glow > 0.003f)
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path face;
            face.addEllipse (sc (px - 86.0f), sc (kVuY - 133.0f), sc (172.0f), sc (172.0f));
            g.reduceClipRegion (face);
            const float k = satRed * glow;
            juce::ColourGradient cg (juce::Colour (0xffff3a1c).withAlpha (0.55f * k), sc (px), sc (kVuY - 55.0f),
                                     juce::Colour (0xffb01008).withAlpha (0.35f * k), sc (px + 86.0f), sc (kVuY - 55.0f), true);
            g.setGradientFill (cg);
            g.fillRect (getLocalBounds());
        }
        const float vu = m == 0 ? inNeedle : outNeedle;
        const float p = juce::jlimit (0.0f, 1.12f, std::pow (10.0f, vu / 20.0f) / 1.4125f);
        const float a = juce::degreesToRadians (-46.0f + 92.0f * (p - 0.0708f) / 0.9292f);
        auto pt = [&] (float rr) { return juce::Point<float> (sc (px + std::sin (a) * rr), sc (kVuY - std::cos (a) * rr)); };
        const juce::Point<float> off (sc (1.4f), sc (1.6f));
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.drawLine (juce::Line<float> (pt (30.0f) + off, pt (86.0f) + off), sc (1.6f));
        g.setColour (juce::Colour (0xff1c140d));
        g.drawLine (juce::Line<float> (pt (30.0f), pt (86.0f)), sc (1.6f));
    }

    // logo Meteora in basso: si accende come il filamento di una valvola
    if (tube > 0.003f)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.setOpacity (juce::jlimit (0.0f, 1.0f, 0.55f * std::pow (tube, 1.4f) * flick));   // luce piu' morbida
        g.drawImage (glowImg, juce::Rectangle<float> (sc (667.5f), sc (677.0f), sc (88.5f), sc (88.5f)));
    }

    // valore della manopola o della velocita' che si sta toccando
    if (lastKnob >= 0 || showIps > 0)
    {
        auto d = juce::Rectangle<float> (sc (538.0f), sc (396.0f), sc (300.0f), sc (30.0f));
        g.setColour (juce::Colour (0xcc14130f));
        g.fillRoundedRectangle (d, sc (6.0f));
        g.setColour (juce::Colour (0xffe8c49a));
        g.setFont (sc (17.0f));
        const juce::String txt = showIps > 0
            ? juce::String ("Tape Speed   ") + (lastIps == 0 ? "7.5" : lastIps == 1 ? "15" : "30") + " IPS"
            : juce::String (kKnobs[lastKnob].name) + "   " + valueText (lastKnob);
        g.drawText (txt, d.toNearestInt(), juce::Justification::centred);
    }
}

void MeteoraTapeEditor::linkOutput (float newIn)
{
    auto* o = proc.apvts.getParameter ("out");
    if (o == nullptr) { prevIn = newIn; return; }
    if (linkDrag)
    {
        const float nv = juce::jlimit (-24.0f, 12.0f, linkOut0 - (newIn - linkIn0));
        o->setValueNotifyingHost (o->convertTo0to1 (nv));
    }
    else if (knobs[0].isMouseOver())     // rotella del mouse sul pomello INPUT
    {
        const float nv = juce::jlimit (-24.0f, 12.0f, P ("out") - (newIn - prevIn));
        o->beginChangeGesture();
        o->setValueNotifyingHost (o->convertTo0to1 (nv));
        o->endChangeGesture();
    }
    prevIn = newIn;
}

void MeteoraTapeEditor::setIps (int v)
{
    if (auto* prm = proc.apvts.getParameter ("ips"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (0, 2, v)));
        prm->endChangeGesture();
    }
}

void MeteoraTapeEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        const std::array<int, 6> pcts { 50, 67, 75, 100, 125, 150 };
        juce::PopupMenu m;
        for (int i = 0; i < (int) pcts.size(); ++i)
        {
            const int w = juce::roundToInt (kW * (float) pcts[(size_t) i] / 100.0f);
            m.addItem (i + 1, "Dimensione " + juce::String (pcts[(size_t) i]) + " %", true, std::abs (getWidth() - w) < 4);
        }
        juce::Component::SafePointer<MeteoraTapeEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, pcts] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            const int w = juce::roundToInt (kW * (float) pcts[(size_t) (r - 1)] / 100.0f);
            safe->setSize (w, juce::roundToInt ((float) w * kH / kW));
        });
        return;
    }
    // TAPE SPEED: si sceglie la velocita' piu' vicina al punto cliccato (o la successiva cliccando sul pomello)
    const auto p = e.position / ((float) getWidth() / kW);
    const float dx = p.x - kSelX, dy = p.y - kSelY;
    if (juce::Rectangle<float> (1080.0f, 470.0f, 180.0f, 190.0f).contains (p))
    {
        int v;
        if (std::hypot (dx, dy) < 36.0f) v = (juce::jlimit (0, 2, (int) std::lround (P ("ips"))) + 1) % 3;
        else
        {
            const float ang = juce::radiansToDegrees (std::atan2 (dx, -dy));
            v = ang < -18.5f ? 0 : (ang > 19.0f ? 2 : 1);
        }
        setIps (v);
        showIps = 60; lastKnob = -1;
        repaint();
    }
}

void MeteoraTapeEditor::timerCallback()
{
    const bool running = proc.isRunning();
    const int ips = juce::jlimit (0, 2, (int) std::lround (P ("ips")));
    bool dirty = false, reelsMoved = false;
    if (ips != lastIps) { lastIps = ips; showIps = 60; dirty = true; }
    if (showIps > 0 && --showIps == 0) dirty = true;

    // selettore che scatta sulla posizione
    const float tgtA = kSelAngle[ips];
    if (std::abs (tgtA - ipsAngle) > 0.05f) { ipsAngle += (tgtA - ipsAngle) * 0.35f; dirty = true; }

    // luci: VU e lampadine morbide, logo-valvola che si scalda piano e tremola appena
    const float gPrev = glow, tPrev = tube, fPrev = flick;
    glow += ((running ? 1.0f : 0.0f) - glow) * 0.12f;
    if (running) tube += (1.0f - tube) * 0.045f; else tube *= 0.88f;
    if (tube < 0.002f) tube = 0.0f;
    flick = running ? 0.96f + 0.04f * rnd.nextFloat() : 1.0f;
    if (std::abs (glow - gPrev) > 0.003f || std::abs (tube - tPrev) > 0.002f || std::abs (flick - fPrev) > 0.004f) dirty = true;

    // rosso del VU OUTPUT: da +6 VU sul nastro (circa 1.2% di distorsione) fino a pieno a +14 VU
    {
        const float sTarget = running ? juce::jlimit (0.0f, 1.0f, (proc.inVu.load() - 6.0f) / 8.0f) : 0.0f;
        const float sPrev = satRed;
        satRed += (sTarget - satRed) * (sTarget > satRed ? 0.5f : 0.08f);
        if (std::abs (satRed - sPrev) > 0.003f) dirty = true;
    }
    // lancette con la balistica dei VU (~300 ms); a riposo tornano a sinistra
    const float iT = running ? proc.inVu.load() : -40.0f, oT = running ? proc.outVu.load() : -40.0f;
    const float iPrev = inNeedle, oPrev = outNeedle;
    inNeedle += (iT - inNeedle) * 0.22f;
    outNeedle += (oT - outNeedle) * 0.22f;
    if (std::abs (inNeedle - iPrev) > 0.02f || std::abs (outNeedle - oPrev) > 0.02f) dirty = true;

    // bobine: girano quando il plugin lavora, piu' veloci a 30 IPS; partono e si fermano con un po' d'inerzia
    const float revPerSec = running ? (ips == 0 ? 0.3f : ips == 1 ? 0.6f : 1.2f) : 0.0f;
    reelSpeed += (revPerSec - reelSpeed) * (running ? 0.08f : 0.05f);
    if (reelSpeed > 0.002f)
    {
        reelAngle -= reelSpeed * juce::MathConstants<float>::twoPi / 30.0f;   // senso antiorario, come in riproduzione
        if (reelAngle < -juce::MathConstants<float>::twoPi) reelAngle += juce::MathConstants<float>::twoPi;
        reelsMoved = true;
    }
    if (dirty) repaint();
    else if (reelsMoved)
    {
        // solo la zona delle bobine: meno lavoro per la CPU
        const float sl = (float) getWidth() / kW;
        repaint (juce::Rectangle<float> (146.0f * sl, 70.0f * sl, 1084.0f * sl, 206.0f * sl).getSmallestIntegerContainer());
    }
}

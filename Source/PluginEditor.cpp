#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour kLime (0xffb6f500), kRed (0xffe4002b), kBlue (0xff1e8bff), kInk (0xff020604);
    const float kPi = juce::MathConstants<float>::pi;

    juce::String pct (double v)  { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }
    juce::String db  (double v)  { return juce::String (v, 1) + " dB"; }

    void drawCan (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour body, juce::Colour accent,
                  float fill, juce::Colour liquid, const juce::String& name)
    {
        const float capH = juce::jmin (10.f, r.getHeight() * 0.07f);
        auto b = r.withTrimmedTop (capH * 0.5f).withTrimmedBottom (capH * 0.5f);
        const float X = b.getX(), Y = b.getY(), W = b.getWidth(), H = b.getHeight();
        juce::Path bp; bp.addRoundedRectangle (X, Y, W, H, 5.f);

        juce::ColourGradient gr (body.darker (0.6f), X, 0, body.darker (0.6f), X + W, 0, false);
        gr.addColour (0.22, body.brighter (0.4f)); gr.addColour (0.55, body);
        g.setGradientFill (gr); g.fillPath (bp);

        g.saveState();
        g.reduceClipRegion (bp);
        auto tri = [&] (juce::Colour c, float ax, float ay, float bx, float by, float cx, float cy)
        { juce::Path s; s.addTriangle (X + W * ax, Y + H * ay, X + W * bx, Y + H * by, X + W * cx, Y + H * cy);
          g.setColour (c); g.fillPath (s); };
        tri (body.darker (0.55f), 0.f, .25f, 1.f, .40f, .1f, .55f);
        tri (body.darker (0.55f), .2f, .62f, 1.f, .70f, .5f, .95f);
        tri (accent,              .35f, .05f, .9f, .12f, .6f, .28f);
        tri (kRed,                .55f, .50f, 1.f, .46f, .8f, .64f);
        tri (juce::Colours::white.withAlpha (0.85f), 0.f, .78f, .45f, .70f, .25f, .90f);
        if (fill > 0.f)
        {
            const float ly = Y + H - H * fill;
            g.setColour (liquid.withAlpha (0.5f)); g.fillRect (X, ly, W, Y + H - ly);
            g.setColour (liquid.brighter (0.6f)); g.fillRect (X, ly, W, 2.f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.2f)); g.fillRect (X + W * 0.12f, Y, W * 0.1f, H);
        g.restoreState();

        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.strokePath (bp, juce::PathStrokeType (1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e8e8), X, 0, juce::Colour (0xff6c7075), X + W, 0, false));
        g.fillEllipse (X + 1.f, r.getY(), W - 2.f, capH * 1.3f);
        g.setColour (juce::Colour (0xff50555a)); g.fillEllipse (X + 1.f, r.getBottom() - capH * 1.3f, W - 2.f, capH * 1.3f);

        g.setFont (juce::Font (juce::jmin (36.f, W * 0.3f), juce::Font::bold | juce::Font::italic));
        auto tr = juce::Rectangle<float> (X, Y + H * 0.06f, W, H * 0.18f);
        g.setColour (juce::Colours::black.withAlpha (0.75f)); g.drawFittedText (name, tr.translated (2, 2).toNearestInt(), juce::Justification::centred, 1);
        g.setColour (juce::Colours::white);                   g.drawFittedText (name, tr.toNearestInt(), juce::Justification::centred, 1);
    }

    void drawSpeaker (juce::Graphics& g, juce::Point<float> c, float r, float level)
    {
        g.setColour (juce::Colour (0xff101010)); g.fillRoundedRectangle (c.x - r, c.y - r, 2 * r, 2 * r, 12.f);
        g.setColour (juce::Colour (0xff2b2b2b)); g.fillEllipse (c.x - r * 0.88f, c.y - r * 0.88f, r * 1.76f, r * 1.76f);
        g.setColour (juce::Colours::black);      g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f);
        const float cr = r * (0.55f + level * 0.14f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff555a55), c.x - cr, c.y - cr, juce::Colour (0xff121412), c.x + cr, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        const float dc = r * 0.22f * (1.f + level * 0.35f);
        g.setColour (juce::Colour (0xff0b0b0b)); g.fillEllipse (c.x - dc, c.y - dc, dc * 2, dc * 2);
        g.setColour (kLime.withAlpha (0.25f + level * 0.6f)); g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 2.f);
        g.setColour (juce::Colour (0xff3c3c3c));
        for (int sx : { -1, 1 }) for (int sy : { -1, 1 }) g.fillEllipse (c.x + sx * r * 0.86f - 3.f, c.y + sy * r * 0.86f - 3.f, 6.f, 6.f);
    }

    void drawCat (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& img)
    {
        juce::Path clip; clip.addRoundedRectangle (r, 16.f);
        g.saveState(); g.reduceClipRegion (clip);
        if (img.isValid())
            g.drawImage (img, r, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        else
        {
            g.fillAll (juce::Colour (0xff6b6b5a)); g.setColour (juce::Colour (0xff3a3a30));
            auto c = r.getCentre();
            g.fillEllipse (c.x - 40, c.y - 30, 80, 70);
            g.setColour (kLime); g.fillEllipse (c.x - 22, c.y - 8, 10, 10); g.fillEllipse (c.x + 12, c.y - 8, 10, 10);
        }
        g.restoreState();
        g.setColour (kLime); g.strokePath (clip, juce::PathStrokeType (3.f));
    }
}

// ---------------------------------------------------------------- knobs
CanKnob::CanKnob (juce::String flavor, juce::Colour body, juce::Colour accent, juce::Colour liquid, Fmt f)
    : flavorName (std::move (flavor)), bodyC (body), accentC (accent), liquidC (liquid), fmt (std::move (f))
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (180);
}

void CanKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto canR = b.removeFromTop (b.getHeight() - 22.f).reduced (4.f, 0.f);
    drawCan (g, canR, bodyC, accentC, (float) valueToProportionOfLength (getValue()), liquidC, flavorName);
    g.setColour (juce::Colours::white); g.setFont (juce::Font (13.f, juce::Font::bold));
    g.drawFittedText (fmt (getValue()), b.toNearestInt(), juce::Justification::centred, 1);
}

LidKnob::LidKnob (Fmt f) : fmt (std::move (f))
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (220);
}

void LidKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (10.f);
    auto c = b.getCentre();
    const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float p = (float) valueToProportionOfLength (getValue());
    const float a0 = juce::degreesToRadians (-135.f), a1 = juce::degreesToRadians (135.f), a = a0 + p * (a1 - a0);

    juce::Path track, val;
    track.addCentredArc (c.x, c.y, rad + 4, rad + 4, 0, a0, a1, true);
    val.addCentredArc   (c.x, c.y, rad + 4, rad + 4, 0, a0, a,  true);
    g.setColour (juce::Colour (0xff0a1a0f));
    g.strokePath (track, juce::PathStrokeType (9.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (kBlue, c.x - rad, c.y, kRed, c.x + rad, c.y, false));
    g.strokePath (val, juce::PathStrokeType (7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto lid = juce::Rectangle<float> (c.x - rad + 4, c.y - rad + 4, 2 * (rad - 4), 2 * (rad - 4));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f4f4), lid.getX(), lid.getY(), juce::Colour (0xff7c8186), lid.getRight(), lid.getBottom(), false));
    g.fillEllipse (lid);
    g.setColour (juce::Colour (0xff3a3d40)); g.drawEllipse (lid, 3.f);
    g.setColour (juce::Colour (0xff9a9fa4)); g.drawEllipse (lid.reduced (14.f), 2.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8f949a), lid.getX(), lid.getY(), juce::Colour (0xffd6d9dc), lid.getRight(), lid.getBottom(), false));
    g.fillEllipse (lid.reduced (30.f));

    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (a, c.x, c.y));
    juce::Rectangle<float> tab (c.x - rad * 0.2f, c.y - rad * 0.55f, rad * 0.4f, rad * 0.66f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafafa), tab.getX(), tab.getY(), juce::Colour (0xff80858a), tab.getRight(), tab.getBottom(), false));
    g.fillRoundedRectangle (tab, tab.getWidth() * 0.4f);
    g.setColour (juce::Colour (0xff30343a)); g.drawRoundedRectangle (tab, tab.getWidth() * 0.4f, 1.5f);
    g.setColour (juce::Colour (0xff1c221e));
    g.fillEllipse (tab.getX() + tab.getWidth() * 0.2f, tab.getY() + tab.getHeight() * 0.1f, tab.getWidth() * 0.6f, tab.getHeight() * 0.4f);
    juce::Path tri;
    tri.addTriangle (c.x - 10, c.y - rad + 24, c.x + 10, c.y - rad + 24, c.x, c.y - rad + 2);
    g.setColour (kRed); g.fillPath (tri);
    g.setColour (juce::Colours::white); g.strokePath (tri, juce::PathStrokeType (1.5f));
    g.restoreState();

    g.setColour (juce::Colour (0xff10240f)); g.setFont (juce::Font (15.f, juce::Font::bold | juce::Font::italic));
    g.drawFittedText (RU ("ГАЗ ") + fmt (getValue()), juce::Rectangle<int> ((int) c.x - 50, (int) (c.y + rad * 0.5f), 100, 20), juce::Justification::centred, 1);
}

CapKnob::CapKnob (juce::String name, juce::Colour cap, Fmt f) : label (std::move (name)), capC (cap), fmt (std::move (f))
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (160);
}

void CapKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto tx = b.removeFromBottom (34.f);
    auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 5.f;
    const float p = (float) valueToProportionOfLength (getValue());
    const float a0 = juce::degreesToRadians (-135.f), a1 = juce::degreesToRadians (135.f), a = a0 + p * (a1 - a0);

    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r + 3.5f, r + 3.5f, 0, a0, a, true);
    g.setColour (kLime); g.strokePath (arc, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path star; const int teeth = 18;
    for (int i = 0; i < teeth * 2; ++i)
    {
        const float ang = i * kPi / teeth, rr = (i % 2) ? r * 0.88f : r;
        const float x = c.x + rr * std::sin (ang), y = c.y - rr * std::cos (ang);
        if (i == 0) star.startNewSubPath (x, y); else star.lineTo (x, y);
    }
    star.closeSubPath();
    g.setGradientFill (juce::ColourGradient (capC.brighter (0.6f), c.x - r, c.y - r, capC.darker (0.7f), c.x + r, c.y + r, false));
    g.fillPath (star);
    g.setColour (capC.darker (0.25f)); g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
    g.setColour (juce::Colours::white);
    g.drawLine (c.x + r * 0.15f * std::sin (a), c.y - r * 0.15f * std::cos (a), c.x + r * 0.6f * std::sin (a), c.y - r * 0.6f * std::cos (a), 3.f);

    g.setFont (juce::Font (11.f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.85f)); g.drawFittedText (label, tx.removeFromTop (16.f).toNearestInt(), juce::Justification::centred, 1);
    g.setColour (kLime); g.drawFittedText (fmt (getValue()), tx.toNearestInt(), juce::Justification::centred, 1);
}

void TubeMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (3.f);
    juce::Path pill; pill.addRoundedRectangle (b, b.getWidth() * 0.5f);
    g.setColour (juce::Colours::white); g.fillPath (pill);
    g.saveState(); g.reduceClipRegion (pill);
    const float fh = b.getHeight() * level, top = b.getBottom() - fh;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6ad41a), 0, top, juce::Colour (0xff1c8a2a), 0, b.getBottom(), false));
    g.fillRect (b.getX(), top, b.getWidth(), fh);
    if (level > 0.88f) { g.setColour (kRed.withAlpha (0.8f)); g.fillRect (b.getX(), top, b.getWidth(), 6.f); }
    g.setColour (juce::Colour (0xffe8ff3a));
    for (float y = b.getBottom() - 24.f; y > top; y -= 58.f) g.fillEllipse (b.getCentreX() - 2.f, y, 4.f, 4.f);
    g.restoreState();
    g.setColour (juce::Colours::black); g.strokePath (pill, juce::PathStrokeType (2.f));
}

// ---------------------------------------------------------------- editor
GazyavaEditor::GazyavaEditor (GazyavaProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      lid ([] (double v) { return pct (v); }),
      canBass   (RU ("ЛАЙМ"),  juce::Colour (0xff43a80d), juce::Colour (0xffb6f500), juce::Colour (0xffd8ff3a), [] (double v) { return RU ("БАС ") + db (v * 18.0); }),
      canCrunch (RU ("ВИШНЯ"), juce::Colour (0xffe0306c), juce::Colour (0xffff9ac0), juce::Colour (0xffff4f93), [] (double v) { return RU ("ХРУСТ ") + pct (v); }),
      canIce    (RU ("ЛЁД"),   juce::Colour (0xff1a78d6), juce::Colour (0xff9fe0ff), juce::Colour (0xff3ab8ff), [] (double v) { return RU ("ЛЁД ") + pct (v); }),
      kIn    (RU ("ВХОД"),    juce::Colour (0xff3d8f14), db),
      kFreq  (RU ("ЧАСТОТА"), juce::Colour (0xff3d8f14), [] (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; }),
      kOvl   (RU ("ПЕРЕГРУЗ"), juce::Colour (0xffd0102a), pct),
      kWidth (RU ("ШИРИНА"),  juce::Colour (0xff1a78d6), [] (double v) { return juce::String (juce::roundToInt (v * 200.0)) + "%"; }),
      kMix   (RU ("МИКС"),    juce::Colour (0xff3d8f14), pct),
      kOut   (RU ("ВЫХОД"),   juce::Colour (0xff3d8f14), db)
{
    setSize (920, 600);
    catL = juce::ImageCache::getFromMemory (BinaryData::manul_left_jpg,  BinaryData::manul_left_jpgSize);
    catR = juce::ImageCache::getFromMemory (BinaryData::manul_right_jpg, BinaryData::manul_right_jpgSize);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &lid, &canBass, &canCrunch, &canIce, &kIn, &kFreq, &kOvl, &kWidth, &kMix, &kOut, &tubeL, &tubeR })
        addAndMakeVisible (c);

    auto& s = proc.apvts;
    aLid = std::make_unique<SA> (s, "drive", lid);       aBass = std::make_unique<SA> (s, "bass", canBass);
    aCrunch = std::make_unique<SA> (s, "crunch", canCrunch); aIce = std::make_unique<SA> (s, "ice", canIce);
    aIn = std::make_unique<SA> (s, "in", kIn);           aFreq = std::make_unique<SA> (s, "freq", kFreq);
    aOvl = std::make_unique<SA> (s, "overload", kOvl);   aWidth = std::make_unique<SA> (s, "width", kWidth);
    aMix = std::make_unique<SA> (s, "mix", kMix);        aOut = std::make_unique<SA> (s, "out", kOut);

    const char* names[4] = { "ЛАМПА", "ЛЕНТА", "ФАЗЗ", "ФОЛД" };
    for (int i = 0; i < 4; ++i)
    {
        auto& bt = modeBtn[i];
        bt.setButtonText (RU (names[i])); bt.setClickingTogglesState (true); bt.setRadioGroupId (77);
        bt.onClick = [this, i] { if (modeBtn[i].getToggleState()) modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (bt);
    }
    modeAtt = std::make_unique<juce::ParameterAttachment> (*s.getParameter ("mode"), [this] (float v)
    { const int idx = juce::roundToInt (v); for (int j = 0; j < 4; ++j) modeBtn[j].setToggleState (j == idx, juce::dontSendNotification); });
    modeAtt->sendInitialUpdate();

    limBtn.setButtonText (RU ("ЛИМИТЕР")); monoBtn.setButtonText (RU ("МОНО-БАС"));
    for (auto* bt : { &limBtn, &monoBtn }) { bt->setClickingTogglesState (true); addAndMakeVisible (bt); }
    aLim = std::make_unique<BA> (s, "limiter", limBtn); aMono = std::make_unique<BA> (s, "monobass", monoBtn);

    for (auto* bt : { &modeBtn[0], &modeBtn[1], &modeBtn[2], &modeBtn[3], &limBtn, &monoBtn })
    {
        bt->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff08140c));
        bt->setColour (juce::TextButton::buttonOnColourId, kLime);
        bt->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        bt->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }
    startTimerHz (30);
}

void GazyavaEditor::timerCallback()
{
    auto toMeter = [] (float g) { return juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (g, -48.f) + 48.f) / 48.f); };
    lvIn  = juce::jmax (toMeter (proc.inLevel.exchange (0.f)),  lvIn  * 0.88f);
    lvOut = juce::jmax (toMeter (proc.outLevel.exchange (0.f)), lvOut * 0.88f);
    tubeL.setLevel (lvIn); tubeR.setLevel (lvOut);
    repaint (12, 20, 150, 420); repaint (758, 20, 150, 420);
}

void GazyavaEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a5a22), 460, 190, kInk, 0, 600, true));
    g.fillAll();
    juce::Path stripe; stripe.addQuadrilateral (0, 380, 920, 250, 920, 310, 0, 450);
    g.setGradientFill (juce::ColourGradient (kRed.withAlpha (0.16f), 0, 0, kBlue.withAlpha (0.16f), 920, 0, false)); g.fillPath (stripe);

    g.setFont (juce::Font (58.f, juce::Font::bold | juce::Font::italic));
    juce::Rectangle<int> tr (200, 6, 520, 66);
    const auto title = RU ("#ГАЗЯВА");
    g.setColour (juce::Colours::black); g.drawFittedText (title, tr.translated (5, 5), juce::Justification::centred, 1);
    g.setColour (kRed);                 g.drawFittedText (title, tr.translated (3, 3), juce::Justification::centred, 1);
    g.setColour (kLime);                g.drawFittedText (title, tr, juce::Justification::centred, 1);

    drawCan (g, { 12.f, 20.f, 150.f, 420.f },  juce::Colour (0xff43a80d), juce::Colour (0xffb6f500), -1.f, kLime, "DEW");
    drawCan (g, { 758.f, 20.f, 150.f, 420.f }, juce::Colour (0xff43a80d), juce::Colour (0xffb6f500), -1.f, kLime, "DEW");
    drawSpeaker (g, { 87.f, 185.f }, 56.f, lvIn);
    drawSpeaker (g, { 833.f, 185.f }, 56.f, lvOut);

    drawCat (g, { 12.f, 452.f, 150.f, 136.f }, catL);
    drawCat (g, { 758.f, 452.f, 150.f, 136.f }, catR);
}

void GazyavaEditor::resized()
{
    tubeL.setBounds (178, 40, 58, 380);  tubeR.setBounds (684, 40, 58, 380);
    lid.setBounds (346, 70, 228, 228);
    for (int i = 0; i < 4; ++i) modeBtn[i].setBounds (325 + i * 68, 304, 64, 24);
    limBtn.setBounds (352, 334, 100, 24); monoBtn.setBounds (468, 334, 100, 24);
    canBass.setBounds (319, 366, 82, 128); canCrunch.setBounds (419, 366, 82, 128); canIce.setBounds (519, 366, 82, 128);
    CapKnob* ks[6] = { &kIn, &kFreq, &kOvl, &kWidth, &kMix, &kOut };
    for (int i = 0; i < 6; ++i) ks[i]->setBounds (250 + i * 70, 500, 70, 96);
}

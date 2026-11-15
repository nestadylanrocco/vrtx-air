#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

//==============================================================================
namespace Atmos
{
    const Colour bg      { 0xff06070a };
    const Colour cyan    { 0xff5ee7ff };
    const Colour violet  { 0xffa77bff };
    const Colour pink    { 0xffff6bd6 };
    const Colour midDeep { 0xff2bb8ff };
    const Colour hiDeep  { 0xff6a7bff };
    const Colour green   { 0xff4dffb0 };
    const Colour textCol { 0xfff5f7fc };

    constexpr float a0 = MathConstants<float>::pi * 1.25f; // ore 7:30
    constexpr float a1 = MathConstants<float>::pi * 2.75f; // ore 4:30

    inline Point<float> polar (Point<float> c, float r, float angle)
    {
        return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
    }

    inline void text (Graphics& g, const String& s, float x, float y, const Font& f,
                      Colour c, Justification j = Justification::centredLeft)
    {
        const float w = 480.0f, h = f.getHeight() * 2.0f;
        Rectangle<float> r;
        if (j == Justification::centred)           r = { x - w * 0.5f, y - h * 0.5f, w, h };
        else if (j == Justification::centredRight) r = { x - w,        y - h * 0.5f, w, h };
        else                                       r = { x,            y - h * 0.5f, w, h };
        g.setColour (c);
        g.setFont (f);
        g.drawText (s, r, j, false);
    }

    inline String minusSign() { return String::fromUTF8 ("\xe2\x88\x92"); }

    // Arco con sfumatura da colore A a colore B (a segmenti opachi)
    inline void gradientArc (Graphics& g, Point<float> c, float r, float from, float to,
                             Colour ca, Colour cb, float width)
    {
        const int segs = jmax (2, (int) (std::abs (to - from) / 0.04f));
        for (int i = 0; i < segs; ++i)
        {
            const float t0 = (float) i / (float) segs, t1 = (float) (i + 1) / (float) segs;
            Path p;
            p.addCentredArc (c.x, c.y, r, r, 0.0f, from + (to - from) * t0, from + (to - from) * t1, true);
            g.setColour (ca.interpolatedWith (cb, t1));
            g.strokePath (p, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    inline void glowArc (Graphics& g, Point<float> c, float r, float from, float to, Colour col, float width, float spread = 1.0f)
    {
        Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
        const float widths[] = { width + 20.0f * spread, width + 12.0f * spread, width + 6.0f * spread };
        const float alphas[] = { 0.05f, 0.09f, 0.16f };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (col.withAlpha (alphas[i]));
            g.strokePath (p, PathStrokeType (widths[i], PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    inline void glowDot (Graphics& g, Point<float> p, float r, Colour glow)
    {
        for (int i = 6; i >= 1; --i)
        {
            const float gr = r + (float) i * 2.0f;
            g.setColour (glow.withAlpha (0.05f * (float) (7 - i) / 3.0f));
            g.fillEllipse (p.x - gr, p.y - gr, gr * 2.0f, gr * 2.0f);
        }
        g.setColour (Colours::white);
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
    }
}

//==============================================================================
VrtxFonts::VrtxFonts()
{
    displayBold  = Typeface::createSystemTypefaceFor (BinaryData::InterDisplayBold_otf,  (size_t) BinaryData::InterDisplayBold_otfSize);
    displayLight = Typeface::createSystemTypefaceFor (BinaryData::InterDisplayLight_otf, (size_t) BinaryData::InterDisplayLight_otfSize);
    medium       = Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf,       (size_t) BinaryData::InterMedium_otfSize);
    semiBold     = Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf,     (size_t) BinaryData::InterSemiBold_otfSize);
}

Font VrtxFonts::make (const Typeface::Ptr& tf, float height, float tracking) const
{
    auto opts = tf != nullptr ? FontOptions (tf).withHeight (height) : FontOptions (height);
    if (tracking != 0.0f)
        opts = opts.withKerningFactor (tracking / height);
    return Font (opts);
}

//==============================================================================
void AtmosLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                         float pos, float startAngle, float endAngle, Slider& s)
{
    const auto area = Rectangle<int> (x, y, width, height).toFloat();
    if (s.getProperties()["kind"].toString() == "trim")
        drawTrim (g, area, pos, startAngle, endAngle, s);
    else
        drawRing (g, area, pos, startAngle, endAngle, s);
}

void AtmosLookAndFeel::drawRing (Graphics& g, Rectangle<float> area, float pos, float a0, float a1, Slider& s)
{
    const bool isHigh = s.getProperties()["kind"].toString() == "high";
    const Colour c2 = isHigh ? Atmos::violet : Atmos::cyan;
    const Colour c1 = (isHigh ? Atmos::hiDeep : Atmos::midDeep).interpolatedWith (Colour (0xff0b0c10), 0.45f);

    const auto c = area.getCentre();
    const float R = 62.0f;
    const float angle = a0 + pos * (a1 - a0);

    // pozzo
    {
        const float wr = R + 14.0f;
        g.setGradientFill (ColourGradient (Colour (0xff15171d), c.x, c.y - 20.0f,
                                           Colour (0xff08090c), c.x, c.y - 20.0f + wr + 20.0f, true));
        g.fillEllipse (c.x - wr, c.y - wr, wr * 2.0f, wr * 2.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawEllipse (c.x - wr, c.y - wr, wr * 2.0f, wr * 2.0f, 1.0f);
    }

    // tacche
    for (int i = 0; i <= 20; ++i)
    {
        const float t = (float) i / 20.0f;
        const float a = a0 + (a1 - a0) * t;
        const bool major = (i % 5) == 0;
        const bool lit = t <= pos + 0.0001f && pos > 0.0f;
        g.setColour (lit ? c2.withAlpha (0.9f) : Colours::white.withAlpha (0.14f));
        g.drawLine (Line<float> (Atmos::polar (c, R + 20.0f, a), Atmos::polar (c, R + (major ? 26.0f : 23.0f), a)),
                    major ? 1.6f : 1.0f);
    }

    // traccia
    {
        Path track;
        track.addCentredArc (c.x, c.y, R, R, 0.0f, a0, a1, true);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.strokePath (track, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // arco luminoso
    if (pos > 0.002f)
    {
        Atmos::glowArc (g, c, R, a0, angle, c2, 7.0f);
        Atmos::gradientArc (g, c, R, a0, angle, c1, c2, 7.0f);
    }
    Atmos::glowDot (g, Atmos::polar (c, R, angle), 4.5f, c2);

    // disco centrale
    {
        const float dr = 48.0f;
        Path disc;
        disc.addEllipse (c.x - dr, c.y - dr, dr * 2.0f, dr * 2.0f);
        DropShadow (Colours::black.withAlpha (0.8f), 18, { 0, 6 }).drawForPath (g, disc);
        g.setGradientFill (ColourGradient (Colour (0xff22252e), c.x, c.y - dr, Colour (0xff0e1014), c.x, c.y + dr, false));
        g.fillPath (disc);
        Path rim;
        rim.addCentredArc (c.x, c.y, dr, dr, 0.0f, -1.25f, 1.25f, true);
        g.setColour (Colours::white.withAlpha (0.09f));
        g.strokePath (rim, PathStrokeType (1.0f));
    }

    // numero
    const String num (roundToInt (s.getValue()));
    const auto numFont = fonts->make (fonts->displayLight, 34.0f, -1.0f);
    const auto pctFont = fonts->make (fonts->medium, 11.0f);
    const float wNum = GlyphArrangement::getStringWidth (numFont, num);
    const float wPct = GlyphArrangement::getStringWidth (pctFont, "%");
    const float startX = c.x - (wNum + 3.0f + wPct) * 0.5f;
    Atmos::text (g, num, startX, c.y - 1.0f, numFont, Atmos::textCol);
    Atmos::text (g, "%", startX + wNum + 3.0f, c.y - 8.0f, pctFont, Colours::white.withAlpha (0.4f));
}

void AtmosLookAndFeel::drawTrim (Graphics& g, Rectangle<float> area, float pos, float a0, float a1, Slider& s)
{
    const Point<float> c { area.getCentreX(), area.getY() + 46.0f };
    const float r = 30.0f;
    const float angle = a0 + pos * (a1 - a0);
    const float mid = (a0 + a1) * 0.5f;

    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.strokePath (track, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (std::abs (angle - mid) > 0.01f)
    {
        const float from = jmin (mid, angle), to = jmax (mid, angle);
        Atmos::glowArc (g, c, r, from, to, Colours::white, 4.0f, 0.5f);
        Path v;
        v.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
        g.setColour (Colour (0xffe8ecf5));
        g.strokePath (v, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float dr = 22.0f;
    Path disc;
    disc.addEllipse (c.x - dr, c.y - dr, dr * 2.0f, dr * 2.0f);
    DropShadow (Colours::black.withAlpha (0.7f), 10, { 0, 4 }).drawForPath (g, disc);
    g.setGradientFill (ColourGradient (Colour (0xff23262f), c.x, c.y - dr, Colour (0xff0e1014), c.x, c.y + dr, false));
    g.fillPath (disc);

    g.setColour (Colours::white);
    g.drawLine (Line<float> (Atmos::polar (c, 8.0f, angle), Atmos::polar (c, 17.0f, angle)), 2.0f);

    const float v = (float) s.getValue();
    String label = (std::abs (v) < 0.05f) ? String ("0.0") : (v > 0.0f ? "+" : Atmos::minusSign()) + String (std::abs (v), 1);
    Atmos::text (g, label + " dB", c.x, c.y + 48.0f, fonts->make (fonts->medium, 12.0f), Colour (0xfff2f4f9), Justification::centred);
}

//==============================================================================
AirScope::AirScope (VRTXAirProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    hfHist.fill (-100.0f);
}

void AirScope::update()
{
    const float m  = proc.midBoostDb.load();
    const float hi = proc.highBoostDb.load();
    const float hf = proc.hfLevelDb.load();

    // movimento morbido ma reattivo
    midNow  += (m  - midNow)  * 0.5f;
    highNow += (hi - highNow) * 0.5f;
    hfNow   += (hf - hfNow)   * (hf > hfNow ? 0.7f : 0.3f);

    midHist[(size_t) head]  = midNow;
    highHist[(size_t) head] = highNow;
    hfHist[(size_t) head]   = hfNow;
    head = (head + 1) % historySize;
    repaint();
}

void AirScope::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();

    Path frame;
    frame.addRoundedRectangle (b.reduced (0.5f), 20.0f);
    g.setColour (Colour (0xff08090d).withAlpha (0.55f));
    g.fillPath (frame);

    g.saveState();
    g.reduceClipRegion (frame);

    const float top = 56.0f, bottom = h - 28.0f;
    auto yBoost = [&] (float db) { return bottom - jlimit (0.0f, 1.0f, db / 18.0f) * (bottom - top); };
    auto yLevel = [&] (float db) { return bottom - jlimit (0.0f, 1.0f, (db + 72.0f) / 60.0f) * (bottom - top) * 0.9f; };
    auto xAt    = [&] (int i)    { return 16.0f + (w - 64.0f) * (float) i / (float) (historySize - 1); };
    auto at     = [&] (const std::array<float, historySize>& a, int i) { return a[(size_t) ((head + i) % historySize)]; };

    // griglia: righe dB e un secondo per colonna
    g.setColour (Colours::white.withAlpha (0.05f));
    for (float d : { 6.0f, 12.0f, 18.0f })
        g.drawHorizontalLine (roundToInt (yBoost (d)), 16.0f, w - 48.0f);
    for (int s = 0; s <= 6; ++s)
        g.drawVerticalLine (roundToInt (xAt (s * (historySize - 1) / 6)), top - 8.0f, bottom);
    const float dash[] = { 3.0f, 5.0f };
    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawDashedLine (Line<float> (16.0f, bottom, w - 48.0f, bottom), dash, 2, 1.0f);

    auto areaPath = [&] (const std::array<float, historySize>& a, auto mapY)
    {
        Path line, fill;
        for (int i = 0; i < historySize; ++i)
        {
            const Point<float> p { xAt (i), mapY (at (a, i)) };
            if (i == 0) { line.startNewSubPath (p); fill.startNewSubPath (p.x, bottom); fill.lineTo (p); }
            else        { line.lineTo (p); fill.lineTo (p); }
        }
        fill.lineTo (xAt (historySize - 1), bottom);
        fill.closeSubPath();
        return std::make_pair (line, fill);
    };

    // 1) acuti della voce in ingresso (grigio)
    {
        auto [line, fill] = areaPath (hfHist, yLevel);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.13f), 0, top, Colours::white.withAlpha (0.01f), 0, bottom, false));
        g.fillPath (fill);
        g.setColour (Colours::white.withAlpha (0.18f));
        g.strokePath (line, PathStrokeType (1.0f));
    }

    // 2) aria aggiunta: HIGH (viola) dietro, MID (ciano) davanti
    auto drawBoost = [&] (const std::array<float, historySize>& a, Colour c)
    {
        auto [line, fill] = areaPath (a, yBoost);
        g.setGradientFill (ColourGradient (c.withAlpha (0.30f), 0, top, c.withAlpha (0.0f), 0, bottom, false));
        g.fillPath (fill);
        const float widths[] = { 14.0f, 8.0f, 4.5f };
        const float alphas[] = { 0.05f, 0.10f, 0.18f };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (c.withAlpha (alphas[i]));
            g.strokePath (line, PathStrokeType (widths[i], PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (line, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        Atmos::glowDot (g, { xAt (historySize - 1), yBoost (at (a, historySize - 1)) }, 3.5f, c);
    };
    drawBoost (highHist, Atmos::violet);
    drawBoost (midHist,  Atmos::cyan);

    // testi
    const auto title = fonts->make (fonts->semiBold, 9.0f, 2.0f);
    Atmos::text (g, "AIR SCOPE", 20.0f, 24.0f, title, Colours::white.withAlpha (0.5f));
    Atmos::text (g, "AIR ADDED TO DETAILS", 20.0f, 40.0f, fonts->make (fonts->medium, 9.0f, 0.5f), Colours::white.withAlpha (0.28f));

    auto readout = [&] (float rightX, const String& label, float db, Colour c)
    {
        const auto big = fonts->make (fonts->displayLight, 24.0f, -0.5f);
        const auto unit = fonts->make (fonts->medium, 10.0f);
        const String num = "+" + String (jmax (0.0f, db), 1);
        const float wNum = GlyphArrangement::getStringWidth (big, num);
        const float wUnit = GlyphArrangement::getStringWidth (unit, "dB");
        Atmos::text (g, num, rightX - wUnit - 4.0f - wNum, 28.0f, big, Atmos::textCol);
        Atmos::text (g, "dB", rightX - wUnit, 32.0f, unit, Colours::white.withAlpha (0.4f));
        Atmos::text (g, label, rightX - wUnit - 4.0f - wNum - 10.0f, 29.0f, title, c, Justification::centredRight);
    };
    readout (w - 20.0f,  "HIGH", highNow, Atmos::violet);
    readout (w - 160.0f, "MID",  midNow,  Atmos::cyan);

    const auto small = fonts->make (fonts->medium, 9.0f);
    for (auto [t, d] : { std::pair<const char*, float> { "+18", 18.0f }, { "+9", 9.0f }, { "0 dB", 0.0f } })
        Atmos::text (g, t, w - 42.0f, yBoost (d), small, Colours::white.withAlpha (0.3f));
    Atmos::text (g, "-6s", xAt (0) + 2.0f, h - 13.0f, small, Colours::white.withAlpha (0.3f));
    Atmos::text (g, "NOW", xAt (historySize - 1), h - 13.0f, small, Colours::white.withAlpha (0.3f), Justification::centredRight);

    g.restoreState();
    g.setColour (Colours::white.withAlpha (0.07f));
    g.strokePath (frame, PathStrokeType (1.0f));
}

//==============================================================================
float OutputMeter::levelToFraction (float db)
{
    // stessa scala disegnata accanto: 0, -6, -18, -36, -60
    static const float dbs[]   = { 0.0f, -6.0f, -18.0f, -36.0f, -60.0f };
    static const float fracs[] = { 1.0f, 1.0f - 22.0f / 120.0f, 1.0f - 58.0f / 120.0f, 1.0f - 98.0f / 120.0f, 0.0f };
    if (db >= 0.0f)   return 1.0f;
    if (db <= -60.0f) return 0.0f;
    for (int i = 0; i < 4; ++i)
        if (db <= dbs[i] && db >= dbs[i + 1])
            return jmap (db, dbs[i + 1], dbs[i], fracs[i + 1], fracs[i]);
    return 0.0f;
}

void OutputMeter::update()
{
    const float tL = levelToFraction (Decibels::gainToDecibels (proc.peakL.exchange (0.0f), -100.0f));
    const float tR = levelToFraction (Decibels::gainToDecibels (proc.peakR.exchange (0.0f), -100.0f));
    levelL = tL > levelL ? tL : jmax (tL, levelL - 0.025f);
    levelR = tR > levelR ? tR : jmax (tR, levelR - 0.025f);
    repaint();
}

void OutputMeter::paint (Graphics& g)
{
    const float my = 6.0f, mh = 120.0f;
    const float xs[] = { 4.0f, 20.0f };
    const float lv[] = { levelL, levelR };

    for (int i = 0; i < 2; ++i)
    {
        const Rectangle<float> bar (xs[i], my, 8.0f, mh);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (bar, 4.0f);

        const float hgt = mh * lv[i];
        if (hgt > 0.5f)
        {
            const auto fillR = bar.withTop (my + mh - hgt);
            g.setColour (Atmos::violet.withAlpha (0.12f));
            g.fillRoundedRectangle (fillR.expanded (3.0f), 6.0f);
            ColourGradient mg (Atmos::cyan, 0, my + mh, Atmos::pink, 0, my, false);
            mg.addColour (0.7, Atmos::violet);
            g.setGradientFill (mg);
            g.fillRoundedRectangle (fillR, 4.0f);
        }
    }

    const auto lab = fonts->make (fonts->semiBold, 8.5f);
    Atmos::text (g, "L", 8.0f,  my + mh + 14.0f, lab, Colours::white.withAlpha (0.4f), Justification::centred);
    Atmos::text (g, "R", 24.0f, my + mh + 14.0f, lab, Colours::white.withAlpha (0.4f), Justification::centred);

    const auto sc = fonts->make (fonts->medium, 8.0f);
    const std::pair<const char*, float> marks[] = { { "0", 0.0f }, { "-6", 22.0f }, { "-18", 58.0f }, { "-36", 98.0f } };
    for (auto& [t, off] : marks)
        Atmos::text (g, t, 48.0f, my + off, sc, Colours::white.withAlpha (0.3f));
}

//==============================================================================
VRTXAirEditor::VRTXAirEditor (VRTXAirProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), meter (p)
{
    // grana di sfondo (generata una volta sola)
    noiseTile = Image (Image::ARGB, 192, 192, true);
    Random rng (42);
    for (int y = 0; y < noiseTile.getHeight(); ++y)
        for (int x = 0; x < noiseTile.getWidth(); ++x)
            if (rng.nextFloat() < 0.18f)
                noiseTile.setPixelAt (x, y, Colours::white.withAlpha (rng.nextFloat() * 0.035f));

    addAndMakeVisible (display);
    addAndMakeVisible (meter);

    setupKnob (midKnob,  "mid");
    setupKnob (highKnob, "high");
    setupKnob (trimKnob, "trim");

    midAtt  = std::make_unique<Attachment> (proc.apvts, "mid",  midKnob);
    highAtt = std::make_unique<Attachment> (proc.apvts, "high", highKnob);
    trimAtt = std::make_unique<Attachment> (proc.apvts, "trim", trimKnob);

    setSize (760, 496);
    startTimerHz (30);
}

VRTXAirEditor::~VRTXAirEditor()
{
    stopTimer();
    for (auto* s : { &midKnob, &highKnob, &trimKnob })
        s->setLookAndFeel (nullptr);
}

void VRTXAirEditor::setupKnob (Slider& s, const String& kind)
{
    s.getProperties().set ("kind", kind);
    s.setLookAndFeel (&lnf);
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (Atmos::a0, Atmos::a1, true);
    s.setMouseDragSensitivity (kind == "trim" ? 200 : 260);
    s.setDoubleClickReturnValue (true, 0.0);
    s.setScrollWheelEnabled (true);
    addAndMakeVisible (s);
}

void VRTXAirEditor::paint (Graphics& g)
{
    g.fillAll (Atmos::bg);

    // aurora
    auto blob = [&g] (float cx, float cy, float rx, float ry, Colour c, float alpha)
    {
        Graphics::ScopedSaveState save (g);
        g.addTransform (AffineTransform::scale (1.0f, ry / rx, cx, cy));
        const float r = rx * 1.45f;
        g.setGradientFill (ColourGradient (c.withAlpha (alpha), cx, cy, c.withAlpha (0.0f), cx + r, cy, true));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    };
    blob (200.0f, 150.0f, 220.0f, 90.0f,  Atmos::cyan,   0.24f);
    blob (560.0f, 140.0f, 240.0f, 100.0f, Atmos::violet, 0.30f);
    blob (700.0f, 380.0f, 160.0f, 90.0f,  Atmos::pink,   0.11f);

    g.setTiledImageFill (noiseTile, 0, 0, 1.0f);
    g.fillAll();

    // logo
    {
        const Point<float> c { 40.0f, 36.0f };
        const Colour cols[] = { Atmos::cyan, Atmos::violet, Colours::white.withAlpha (0.9f) };
        for (int i = 0; i < 3; ++i)
        {
            const float r = 11.0f - (float) i * 4.0f;
            const float start = (float) i * 2.1f + MathConstants<float>::halfPi;
            Path arc;
            arc.addCentredArc (c.x, c.y, r, r, 0.0f, start, start + 4.2f, true);
            g.setColour (cols[i]);
            g.strokePath (arc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
    const auto vrtx = fonts->make (fonts->displayBold, 17.0f, 3.5f);
    const auto air  = fonts->make (fonts->displayLight, 17.0f, 3.5f);
    Atmos::text (g, "VRTX", 62.0f, 36.0f, vrtx, Colour (0xfff4f6fb));
    Atmos::text (g, "AIR", 62.0f + GlyphArrangement::getStringWidth (vrtx, "VRTX") + 2.0f, 36.0f, air, Atmos::cyan);

    // pillola stato
    {
        const Rectangle<float> pill (588.0f, 22.0f, 148.0f, 28.0f);
        g.setColour (Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (pill, 14.0f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (pill, 14.0f, 1.0f);
        for (int i = 3; i >= 1; --i)
        {
            const float r = 3.5f + (float) i * 2.0f;
            g.setColour (Atmos::green.withAlpha (0.10f * (float) (4 - i)));
            g.fillEllipse (606.0f - r, 36.0f - r, r * 2.0f, r * 2.0f);
        }
        g.setColour (Atmos::green);
        g.fillEllipse (602.5f, 32.5f, 7.0f, 7.0f);
        Atmos::text (g, String ("DYNAMIC  ") + String::fromUTF8 ("\xc2\xb7") + String ("  4") + String::fromUTF8 ("\xc3\x97") + " HQ",
                     620.0f, 36.0f, fonts->make (fonts->medium, 9.5f, 1.2f), Colours::white.withAlpha (0.55f));
    }

    // etichette manopole
    const auto lab = fonts->make (fonts->semiBold, 11.0f, 3.0f);
    const auto rng = fonts->make (fonts->medium, 9.5f, 1.0f);
    const String dash = String::fromUTF8 (" \xe2\x80\x93 ");
    Atmos::text (g, "MID AIR",  150.0f, 450.0f, lab, Colours::white.withAlpha (0.82f), Justification::centred);
    Atmos::text (g, "3" + dash + "9 kHz", 150.0f, 466.0f, rng, Colours::white.withAlpha (0.35f), Justification::centred);
    Atmos::text (g, "HIGH AIR", 390.0f, 450.0f, lab, Colours::white.withAlpha (0.82f), Justification::centred);
    Atmos::text (g, "9" + dash + "20 kHz", 390.0f, 466.0f, rng, Colours::white.withAlpha (0.35f), Justification::centred);

    // pannello output
    {
        const Rectangle<float> panel (536.0f, 288.0f, 200.0f, 184.0f);
        g.setColour (Colours::white.withAlpha (0.03f));
        g.fillRoundedRectangle (panel, 18.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (panel, 18.0f, 1.0f);
        Atmos::text (g, "OUTPUT", 556.0f, 308.0f, fonts->make (fonts->semiBold, 9.5f, 2.5f), Colours::white.withAlpha (0.45f));
        Atmos::text (g, "TRIM", 600.0f, 336.0f, fonts->make (fonts->semiBold, 9.0f, 2.0f), Colours::white.withAlpha (0.4f), Justification::centred);
    }
}

void VRTXAirEditor::resized()
{
    display.setBounds (24, 68, 712, 196);
    midKnob.setBounds  (54,  252, 192, 192);
    highKnob.setBounds (294, 252, 192, 192);
    trimKnob.setBounds (556, 344, 88, 108);
    meter.setBounds (668, 316, 72, 150);
}

void VRTXAirEditor::timerCallback()
{
    display.update();
    meter.update();
}

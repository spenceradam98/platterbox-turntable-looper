#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr int W = 1100, H = 690;
constexpr float kStart = MathConstants<float>::pi * -0.75f, kEnd = MathConstants<float>::pi * 0.75f;
std::vector<std::pair<Rectangle<int>, String>> captions;   // small labels painted by the editor
}

const std::vector<Theme>& themes()
{
    static const std::vector<Theme> list {
        { "Cream",    Colour (0xffe9e2d0), Colour (0xfff4efe3), Colour (0xffcfc6b0), Colour (0xff2a2724), Colour (0xff7d776b),
                      Colour (0xffe8622c), Colour (0xff2f8f83), Colour (0xff9c9c9c), Colour (0xff161616), Colour (0xffe8622c), Colour (0xff2a2724) },
        { "Midnight", Colour (0xff17181c), Colour (0xff212329), Colour (0xff353841), Colour (0xffe6e8ee), Colour (0xff8a8f9c),
                      Colour (0xff3fd0e0), Colour (0xffff5c8a), Colour (0xff3a3d44), Colour (0xff0b0b0c), Colour (0xff3fd0e0), Colour (0xff4a4e59) },
        { "Seafoam",  Colour (0xffa9d6c4), Colour (0xffc4e6d8), Colour (0xff86bba5), Colour (0xff1f3530), Colour (0xff4d6b62),
                      Colour (0xffd8443a), Colour (0xff2b6cb0), Colour (0xffd9d9d9), Colour (0xff1a1a1a), Colour (0xfff2c14e), Colour (0xfff6f3ea) },
        { "Cherry",   Colour (0xffb3202a), Colour (0xffc8333c), Colour (0xff8e1720), Colour (0xfffff1e0), Colour (0xfff0b9b0),
                      Colour (0xffffd166), Colour (0xffffffff), Colour (0xffc0c0c0), Colour (0xff121212), Colour (0xfffff1e0), Colour (0xfff8ead8) },
    };
    return list;
}

// ============================================================================ LookAndFeel
void PlatterboxLookAndFeel::setTheme (const Theme& t)
{
    th = &t;
    setColour (ComboBox::backgroundColourId, t.panel.darker (0.05f));
    setColour (ComboBox::textColourId, t.ink);
    setColour (ComboBox::outlineColourId, t.edge);
    setColour (ComboBox::arrowColourId, t.inkDim);
    setColour (PopupMenu::backgroundColourId, t.panel);
    setColour (PopupMenu::textColourId, t.ink);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.4f));
    setColour (PopupMenu::highlightedTextColourId, t.ink);
    setColour (TextButton::textColourOffId, t.ink);
    setColour (TextButton::textColourOnId, t.accent.getPerceivedBrightness() > 0.55f ? Colour (0xff1a1a1a) : Colours::white);
    setColour (ToggleButton::textColourId, t.ink);
    setColour (BubbleComponent::backgroundColourId, t.panel);
    setColour (BubbleComponent::outlineColourId, t.edge);
    setColour (TooltipWindow::textColourId, t.ink);
    setColour (Label::textColourId, t.ink);
}

void PlatterboxLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto& t = *th;
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;

    // value arc
    Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, start, end, true);
    g.setColour (t.edge); g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    const float from = bipolar ? (start + end) * 0.5f : start;
    Path val; val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, jmin (from, angle), jmax (from, angle), true);
    g.setColour (t.accent); g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // knob body
    auto body = Rectangle<float> (d * 0.68f, d * 0.68f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillEllipse (body.translated (0, 2.0f));
    g.setGradientFill (ColourGradient (t.knob.brighter (0.25f), body.getX(), body.getY(), t.knob.darker (0.3f), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (t.knob.darker (0.5f)); g.drawEllipse (body, 1.0f);
    const auto pointerCol = t.knob.getPerceivedBrightness() > 0.5f ? Colour (0xff1e1e1e) : Colours::white;
    g.setColour (pointerCol);
    g.drawLine (Line<float> (c.getPointOnCircumference (body.getWidth() * 0.12f, angle), c.getPointOnCircumference (body.getWidth() * 0.46f, angle)), 2.5f);
}

void PlatterboxLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                               Slider::SliderStyle style, Slider& s)
{
    const auto& t = *th;
    const bool vertical = style == Slider::LinearVertical;
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    // slot
    auto slot = vertical ? Rectangle<float> (6.0f, b.getHeight()).withCentre (b.getCentre())
                         : Rectangle<float> (b.getWidth(), 6.0f).withCentre (b.getCentre());
    g.setColour (t.mat.withAlpha (0.85f)); g.fillRoundedRectangle (slot, 3.0f);
    // ticks
    g.setColour (t.inkDim.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float f = (float) i / 10.0f;
        if (vertical) { const float yy = b.getY() + f * b.getHeight(); g.drawLine (b.getX() + 2, yy, b.getX() + (i == 5 ? 12.0f : 8.0f), yy, 1.0f); }
        else { const float xx = b.getX() + f * b.getWidth(); g.drawLine (xx, b.getBottom() - 2, xx, b.getBottom() - (i == 5 ? 10.0f : 6.0f), 1.0f); }
    }
    // cap
    const bool centreDetent = s.getMinimum() < 0;
    auto cap = vertical ? Rectangle<float> (jmin (40.0f, b.getWidth() - 4), 22.0f).withCentre ({ b.getCentreX(), pos })
                        : Rectangle<float> (26.0f, jmin (40.0f, b.getHeight() - 4)).withCentre ({ pos, b.getCentreY() });
    g.setColour (Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (cap.translated (0, 2), 3.0f);
    g.setGradientFill (ColourGradient (t.knob.brighter (0.3f), cap.getX(), cap.getY(), t.knob.darker (0.3f), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (centreDetent && std::abs (s.getValue()) < 0.01 ? t.accent2 : t.accent);
    if (vertical) g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 8, 2.0f));
    else g.fillRect (cap.withSizeKeepingCentre (2.0f, cap.getHeight() - 8));
}

void PlatterboxLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const auto& t = *th;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    Colour fill = b.getToggleState() ? t.accent : t.panel.darker (0.06f);
    if (b.getProperties().contains ("led")) fill = Colour::fromString (b.getProperties()["led"].toString());
    if (highlighted) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.15f);
    g.setColour (Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r.translated (0, 1.5f), 5.0f);
    g.setColour (fill); g.fillRoundedRectangle (r, 5.0f);
    g.setColour (t.edge.darker (0.2f)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void PlatterboxLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto& t = *th;
    auto r = b.getLocalBounds().toFloat();
    auto led = Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 8.0f, r.getCentreY() });
    g.setColour (b.getToggleState() ? t.accent : t.mat.withAlpha (0.35f));
    g.fillEllipse (led);
    if (b.getToggleState()) { g.setColour (t.accent.withAlpha (0.3f)); g.fillEllipse (led.expanded (3.0f)); }
    g.setColour (highlighted ? t.ink : t.ink.withAlpha (0.85f));
    g.setFont (FontOptions (11.5f, Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (18.0f), Justification::centredLeft);
}

// ============================================================================ Platter
float PlatterView::angleAt (Point<float> p) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return std::atan2 (p.y - c.y, p.x - c.x) / MathConstants<float>::twoPi;   // revolutions, clockwise positive
}

void PlatterView::mouseDown (const MouseEvent& e)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    if (e.position.getDistanceFrom (c) > getWidth() * 0.47f) return;
    holding = true; lastAngle = angleAt (e.position); lastTime = Time::getMillisecondCounterHiRes(); velocity = 0; idleTicks = 0;
    proc.setScratch (true, 0.0);
}

void PlatterView::mouseDrag (const MouseEvent& e)
{
    if (! holding) return;
    const float a = angleAt (e.position);
    float d = a - lastAngle;
    if (d > 0.5f) d -= 1.0f;
    if (d < -0.5f) d += 1.0f;
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = jmax (1.0, now - lastTime) * 0.001;
    velocity = 0.5 * velocity + 0.5 * (d / dt);
    lastAngle = a; lastTime = now; idleTicks = 0;
    proc.setScratch (true, velocity);
}

void PlatterView::mouseUp (const MouseEvent&)
{
    if (holding) proc.setScratch (false, 0.0);
    holding = false;
}

void PlatterView::tick()
{
    if (holding && ++idleTicks > 2) { velocity = 0; proc.setScratch (true, 0.0); }   // hand resting: record held still
    repaint();
}

void PlatterView::paint (Graphics& g)
{
    const auto& t = *theme;
    const auto& eng = proc.getEngine();
    auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.48f;
    const float rot = (float) eng.platterAngle() * MathConstants<float>::twoPi;

    // platter rim with strobe dots
    g.setColour (Colours::black.withAlpha (0.3f));
    g.fillEllipse (Rectangle<float> (R * 2, R * 2).withCentre (c.translated (0, 4)));
    g.setGradientFill (ColourGradient (t.platter.brighter (0.4f), c.x - R, c.y - R, t.platter.darker (0.4f), c.x + R, c.y + R, false));
    g.fillEllipse (Rectangle<float> (R * 2, R * 2).withCentre (c));
    g.setColour (t.platter.darker (0.6f));
    for (int i = 0; i < 90; ++i)
    {
        const float a = rot + MathConstants<float>::twoPi * (float) i / 90.0f;
        g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (c.getPointOnCircumference (R - 5.0f, a)));
    }

    // record
    const float rr = R * 0.92f;
    g.setColour (t.mat);
    g.fillEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre (c));
    for (float gr = rr * 0.98f; gr > rr * 0.38f; gr -= 3.0f)
    {
        g.setColour (Colours::white.withAlpha (0.035f + 0.02f * std::sin (gr * 0.37f)));
        g.drawEllipse (Rectangle<float> (gr * 2, gr * 2).withCentre (c), 1.0f);
    }
    // sheen that rotates so the spin is visible
    for (int k = 0; k < 2; ++k)
    {
        Path sheen;
        const float a0 = (float) k * MathConstants<float>::pi + 0.6f;   // fixed reflections
        sheen.addPieSegment (Rectangle<float> (rr * 2, rr * 2).withCentre (c), a0, a0 + 0.35f, 0.4f);
        g.setColour (Colours::white.withAlpha (0.05f)); g.fillPath (sheen);
    }

    // label (rotates)
    const float lr = rr * 0.34f;
    auto labelRect = Rectangle<float> (lr * 2, lr * 2).withCentre (c);
    g.setColour (t.label); g.fillEllipse (labelRect);
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (rot, c.x, c.y));
        const auto labelInk = t.label.getPerceivedBrightness() > 0.55f ? Colour (0xff1a1a1a) : Colours::white;
        g.setColour (labelInk);
        g.setFont (FontOptions (13.0f, Font::bold));
        auto name = proc.getRecordName();
        g.drawFittedText (name.isEmpty() ? String ("drop a file") : name, labelRect.reduced (lr * 0.25f, lr * 0.55f).translated (0, -lr * 0.35f).toNearestInt(), Justification::centred, 2);
        g.setFont (FontOptions (10.0f));
        g.drawText ("PLATTERBOX", labelRect.reduced (lr * 0.2f).translated (0, lr * 0.45f).toNearestInt(), Justification::centred);
        g.setColour (labelInk.withAlpha (0.6f));
        g.drawEllipse (labelRect.reduced (lr * 0.1f), 1.0f);
    }
    g.setColour (Colours::lightgrey); g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (c));

    // tonearm: pivot top right, needle follows the playing position from outer to inner groove
    const double len = proc.getRecordSeconds();
    const double prog = len > 0 ? jlimit (0.0, 1.0, eng.positionSeconds() / len) : 0.0;
    const Point<float> pivot (b.getRight() - R * 0.08f, b.getY() + R * 0.12f);
    const float needleR = rr * (0.95f - 0.55f * (float) prog);
    const float dist = pivot.getDistanceFrom (c);
    const float arm = dist * 0.9f;
    // intersection of circle(c, needleR) and circle(pivot, arm)
    const float a = (needleR * needleR - arm * arm + dist * dist) / (2.0f * dist);
    const float hh = std::sqrt (jmax (0.0f, needleR * needleR - a * a));
    const auto dir = (pivot - c) / dist;
    const Point<float> mid = c + dir * a;
    const Point<float> needle (mid.x + hh * dir.y, mid.y - hh * dir.x);
    g.setColour (Colours::black.withAlpha (0.3f));
    g.drawLine (Line<float> (pivot.translated (3, 5), needle.translated (3, 5)), 6.0f);
    g.setColour (t.platter.brighter (0.5f));
    g.drawLine (Line<float> (pivot, needle), 5.0f);
    g.setColour (t.knob);
    g.fillRoundedRectangle (Rectangle<float> (16.0f, 26.0f).withCentre (needle), 3.0f);
    g.fillEllipse (Rectangle<float> (30.0f, 30.0f).withCentre (pivot));
    g.setColour (t.accent); g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (pivot));
}

// ============================================================================ Wave strip
void WaveStrip::seekTo (float x)
{
    if (proc.getRecordSeconds() <= 0) return;
    proc.requestSeek (jlimit (0.0, 1.0, (double) x / getWidth()) * proc.getRecordSeconds());
}

void WaveStrip::paint (Graphics& g)
{
    const auto& t = *theme;
    auto b = getLocalBounds().toFloat();
    g.setColour (t.mat); g.fillRoundedRectangle (b, 5.0f);
    const auto& ov = proc.getOverview();
    if (ov.empty())
    {
        g.setColour (t.inkDim); g.setFont (FontOptions (12.0f));
        g.drawText ("Load or drop an audio file (WAV, AIFF, FLAC, OGG, MP3)", b, Justification::centred);
        return;
    }
    const int cols = (int) b.getWidth() - 8;
    g.setColour (t.accent.withAlpha (0.75f));
    for (int x = 0; x < cols; ++x)
    {
        const size_t i = (size_t) ((double) x / cols * (double) ov.size());
        const float hgt = ov[jmin (i, ov.size() - 1)] * (b.getHeight() - 8) * 0.5f;
        g.drawVerticalLine (4 + x, b.getCentreY() - hgt, b.getCentreY() + hgt);
    }
    const double prog = jlimit (0.0, 1.0, proc.getEngine().positionSeconds() / jmax (1e-6, proc.getRecordSeconds()));
    g.setColour (Colours::white);
    g.fillRect (Rectangle<float> (2.0f, b.getHeight()).withX (4.0f + (float) prog * (float) cols - 1.0f));
}

// ============================================================================ Editor
PlatterboxEditor::PlatterboxEditor (PlatterboxProcessor& p)
    : AudioProcessorEditor (p), proc (p), platter (p), wave (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (platter);
    addAndMakeVisible (wave);

    // header
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);
    for (size_t i = 0; i < themes().size(); ++i) themeBox.addItem (String ("Theme: ") + themes()[i].name, (int) i + 1);
    themeBox.onChange = [this] { applyTheme (themeBox.getSelectedId() - 1); };
    addAndMakeVisible (themeBox);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load a record", proc.getRecordFile(), proc.getFormats().getWildcardForAllFormats());
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
    };
    addAndMakeVisible (loadButton);

    // transport
    motorButton.onClick = [this]
    {
        if (auto* prm = proc.apvts.getParameter ("motor")) prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    };
    addAndMakeVisible (motorButton);
    const char* speedNames[] = { "33", "45", "78" };
    for (int i = 0; i < 3; ++i)
    {
        speedButtons[(size_t) i].setButtonText (speedNames[i]);
        speedButtons[(size_t) i].onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter ("speed")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        };
        addAndMakeVisible (speedButtons[(size_t) i]);
    }
    toggle ("reverse", "REVERSE");
    toggle ("repeat", "REPEAT");
    combo ("cutAt");

    // pitch
    knob ("pitch", "PITCH", Slider::LinearVertical);
    knobById["pitch"]->slider.setDoubleClickReturnValue (true, 0.0);
    combo ("pitchRange");

    // vinyl
    for (auto [id, label] : { std::pair { "crackle", "CRACKLE" }, { "hiss", "HISS" }, { "rumble", "RUMBLE" }, { "wow", "WOW" },
                              { "wear", "WEAR" }, { "drive", "TUBE" }, { "startTime", "START" }, { "brakeTime", "BRAKE" } })
        knob (id, label);
    combo ("cartridge");
    toggle ("mono", "MONO");

    // looper
    loopMain.onClick = [this] { proc.sendLoopCommand (pb::LoopCommand::recPlayDub); };
    loopStop.onClick = [this] { proc.sendLoopCommand (pb::LoopCommand::stop); };
    loopClear.onClick = [this] { proc.sendLoopCommand (pb::LoopCommand::clear); };
    loopUndo.onClick = [this] { proc.sendLoopCommand (pb::LoopCommand::undo); };
    loopHalf.onClick = [this] { proc.sendLoopCommand (pb::LoopCommand::half); };
    loopDouble.onClick = [this] { proc.sendLoopCommand (pb::LoopCommand::doubleLen); };
    loopRev.onClick = [this] { proc.sendLoopCommand (pb::LoopCommand::reverse); };
    for (auto* b : { &loopMain, &loopStop, &loopClear, &loopUndo, &loopHalf, &loopDouble, &loopRev }) addAndMakeVisible (*b);
    knob ("loopLevel", "LOOP LVL");
    toggle ("loopFollow", "FOLLOW PITCH");

    // filter
    combo ("filterType");
    knob ("filter", "FILTER");
    knobById["filter"]->slider.setDoubleClickReturnValue (true, 0.0);
    knob ("resonance", "RES / HIGH");
    knob ("modRate", "MOD RATE");
    knob ("modDepth", "MOD DEPTH");

    // delay
    knob ("delayMs", "TIME");
    knob ("feedback", "FEEDBACK");
    toggle ("delaySync", "SYNC");
    combo ("delayDiv");
    knob ("delayLevel", "LEVEL", Slider::LinearVertical);

    // mixer
    knob ("deckLevel", "DECK");
    knob ("lineLevel", "LINE IN");
    knob ("crossfader", "CROSSFADER", Slider::LinearHorizontal);
    knobById["crossfader"]->slider.setDoubleClickReturnValue (true, 0.0);
    toggle ("hamster", "HAMSTER");
    combo ("faderCurve");
    toggle ("speaker", "SPEAKER");
    knob ("output", "OUTPUT");
    knobById["output"]->slider.setDoubleClickReturnValue (true, 0.0);

    themeBox.setSelectedId (jlimit (0, (int) themes().size() - 1, proc.getTheme()) + 1, dontSendNotification);
    applyTheme (themeBox.getSelectedId() - 1);
    setWantsKeyboardFocus (true);
    setSize (W, H);
    timerCallback();   // show the current transport/looper state straight away
    startTimerHz (40);
}

PlatterboxEditor::~PlatterboxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

PlatterboxEditor::Knob& PlatterboxEditor::knob (const String& id, const String& label, Slider::SliderStyle style)
{
    auto k = std::make_unique<Knob>();
    k->label = label;
    k->slider.setSliderStyle (style);
    k->slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k->slider.setPopupDisplayEnabled (true, true, this);
    k->attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    addAndMakeVisible (k->slider);
    knobById[id] = k.get();
    byId[id] = &k->slider;
    knobs.push_back (std::move (k));
    return *knobs.back();
}

ToggleButton& PlatterboxEditor::toggle (const String& id, const String& label)
{
    auto t = std::make_unique<ToggleButton> (label);
    buttonAttach.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *t));
    addAndMakeVisible (*t);
    byId[id] = t.get();
    toggles.push_back (std::move (t));
    return *toggles.back();
}

ComboBox& PlatterboxEditor::combo (const String& id)
{
    auto c = std::make_unique<ComboBox>();
    if (auto* prm = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->addItemList (prm->choices, 1);
    comboAttach.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c));
    addAndMakeVisible (*c);
    byId[id] = c.get();
    combos.push_back (std::move (c));
    return *combos.back();
}

void PlatterboxEditor::applyTheme (int index)
{
    index = jlimit (0, (int) themes().size() - 1, index);
    proc.setTheme (index);
    lnf.setTheme (themes()[(size_t) index]);
    platter.theme = wave.theme = &themes()[(size_t) index];
    sendLookAndFeelChange();
    repaint();
}

void PlatterboxEditor::loadFile (const File& f)
{
    String err;
    status = proc.loadFile (f, err) ? String() : err;
    repaint();
}

void PlatterboxEditor::filesDropped (const StringArray& files, int, int)
{
    if (! files.isEmpty()) loadFile (File (files[0]));
}

void PlatterboxEditor::placeKnob (Knob& k, Rectangle<int> area)
{
    k.slider.setBounds (area.withTrimmedBottom (16));
}

void PlatterboxEditor::section (Graphics& g, Rectangle<int> r, const String& title)
{
    const auto& t = lnf.theme();
    g.setColour (t.panel); g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (t.edge); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (t.inkDim); g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (title, r.reduced (10, 6).removeFromTop (14), Justification::topLeft);
}

namespace
{
// section rectangles (shared by paint and resized)
const Rectangle<int> rLeft (14, 58, 432, 618), rPitch (456, 58, 70, 420), rVinyl (536, 58, 550, 128), rLoop (536, 194, 550, 128),
                     rFilter (536, 330, 300, 148), rDelay (844, 330, 242, 148), rMix (456, 486, 630, 190);
}

void PlatterboxEditor::paint (Graphics& g)
{
    const auto& t = lnf.theme();
    g.fillAll (t.body);
    // header
    g.setColour (t.ink); g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("PLATTERBOX", 18, 10, 220, 34, Justification::centredLeft);
    g.setColour (t.inkDim); g.setFont (FontOptions (11.0f));
    g.drawText ("portable turntable  .  looper  .  fx", 186, 18, 220, 20, Justification::centredLeft);

    section (g, rLeft, "DECK");
    section (g, rPitch, "PITCH");
    section (g, rVinyl, "VINYL & MOTOR");
    section (g, rLoop, "LOOPER");
    section (g, rFilter, "FILTER");
    section (g, rDelay, "DELAY");
    section (g, rMix, "MIXER");

    // knob captions
    g.setFont (FontOptions (10.5f, Font::bold));
    for (auto& k : knobs)
    {
        if (k->slider.getSliderStyle() == Slider::LinearHorizontal || k->label == "PITCH") continue;
        auto sb = k->slider.getBounds();
        g.setColour (t.ink.withAlpha (0.8f));
        g.drawText (k->label, sb.getX() - 10, sb.getBottom() + 1, sb.getWidth() + 20, 14, Justification::centred);
    }
    g.setColour (t.inkDim); g.setFont (FontOptions (10.5f, Font::bold));
    for (auto& [r, s] : captions) g.drawText (s, r, Justification::centredLeft);

    // looper progress
    const auto& lp = proc.getEngine().getLooper();
    auto bar = Rectangle<int> (rLoop.getX() + 12, rLoop.getBottom() - 22, 330, 8).toFloat();
    g.setColour (t.mat.withAlpha (0.8f)); g.fillRoundedRectangle (bar, 4.0f);
    if (lp.state() != pb::LoopState::empty)
    {
        const float f = lp.state() == pb::LoopState::recording ? 1.0f : (float) lp.progress();
        g.setColour (lp.state() == pb::LoopState::recording ? Colour (0xffe0342a) : t.accent);
        g.fillRoundedRectangle (bar.withWidth (jmax (4.0f, bar.getWidth() * f)), 4.0f);
    }
    g.setColour (t.ink); g.setFont (FontOptions (11.0f));
    const char* stateNames[] = { "empty", "recording", "playing", "overdubbing", "stopped" };
    g.drawText (String (stateNames[(int) lp.state()]) + (lp.lengthSeconds() > 0 ? "  " + String (lp.lengthSeconds(), 2) + " s" : String())
                    + (lp.isReversed() ? "  rev" : ""),
                (int) bar.getRight() + 10, (int) bar.getY() - 4, 180, 16, Justification::centredLeft);

    // output meter
    auto m = Rectangle<int> (rMix.getRight() - 26, rMix.getY() + 28, 10, rMix.getHeight() - 44).toFloat();
    g.setColour (t.mat); g.fillRoundedRectangle (m, 3.0f);
    const float lvl = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (meter, -60.0f) + 60.0f) / 63.0f);
    g.setColour (meter > 0.98f ? Colour (0xffe0342a) : t.accent2);
    g.fillRoundedRectangle (m.withTop (m.getBottom() - m.getHeight() * lvl), 3.0f);

    // platter readout + status
    g.setColour (t.ink); g.setFont (FontOptions (12.0f, Font::bold));
    const double rpm = proc.getEngine().platterSpeed() * pb::Engine::rpmOf (proc.getEngine().getParams().cutAt);
    const double pos = proc.getEngine().positionSeconds();
    g.drawText (String (std::abs (rpm), 1) + " rpm" + (rpm < -0.05 ? " (rev)" : "") + "    "
                    + String ((int) pos / 60) + ":" + String ((int) pos % 60).paddedLeft ('0', 2)
                    + " / " + String ((int) proc.getRecordSeconds() / 60) + ":" + String ((int) proc.getRecordSeconds() % 60).paddedLeft ('0', 2),
                rLeft.getX() + 12, rLeft.getBottom() - 26, 300, 18, Justification::centredLeft);
    if (status.isNotEmpty())
    {
        g.setColour (Colour (0xffe0342a)); g.setFont (FontOptions (11.0f));
        g.drawFittedText (status, rLeft.getX() + 12, rLeft.getBottom() - 46, rLeft.getWidth() - 24, 18, Justification::centredLeft, 1);
    }
}

void PlatterboxEditor::resized()
{
    captions.clear();
    auto caption = [] (int x, int y, int w, const String& s) { captions.push_back ({ { x, y, w, 14 }, s }); };

    // header
    presetBox.setBounds (440, 14, 200, 26);
    loadButton.setBounds (650, 14, 80, 26);
    themeBox.setBounds (W - 180, 14, 166, 26);

    // deck
    platter.setBounds (rLeft.getX() + 6, rLeft.getY() + 22, 420, 400);
    wave.setBounds (rLeft.getX() + 12, rLeft.getY() + 428, rLeft.getWidth() - 24, 48);
    int x = rLeft.getX() + 12, y = rLeft.getY() + 486;
    motorButton.setBounds (x, y, 132, 40);
    for (int i = 0; i < 3; ++i) speedButtons[(size_t) i].setBounds (x + 142 + i * 52, y, 48, 40);
    byId["reverse"]->setBounds (x + 302, y, 110, 20);
    byId["repeat"]->setBounds (x + 302, y + 20, 110, 20);
    caption (x, y + 52, 120, "FILES CUT AT (RPM)");
    byId["cutAt"]->setBounds (x + 124, y + 48, 70, 22);

    // pitch
    knobById["pitch"]->slider.setBounds (rPitch.getX() + 10, rPitch.getY() + 26, 50, 340);
    byId["pitchRange"]->setBounds (rPitch.getX() + 6, rPitch.getBottom() - 34, 58, 24);

    // vinyl
    {
        const char* ids[] = { "crackle", "hiss", "rumble", "wow", "wear", "drive", "startTime", "brakeTime" };
        for (int i = 0; i < 8; ++i) placeKnob (*knobById[ids[i]], { rVinyl.getX() + 8 + i * 54, rVinyl.getY() + 26, 54, 72 });
        caption (rVinyl.getX() + 446, rVinyl.getY() + 24, 100, "CARTRIDGE");
        byId["cartridge"]->setBounds (rVinyl.getX() + 446, rVinyl.getY() + 40, 96, 24);
        byId["mono"]->setBounds (rVinyl.getX() + 446, rVinyl.getY() + 72, 96, 22);
    }

    // looper
    {
        const int lx = rLoop.getX() + 12, ly = rLoop.getY() + 26;
        loopMain.setBounds (lx, ly, 88, 56);
        int bx = lx + 96;
        for (auto* b : { &loopStop, &loopClear, &loopUndo, &loopHalf, &loopDouble, &loopRev }) { b->setBounds (bx, ly + 8, 44, 40); bx += 48; }
        placeKnob (*knobById["loopLevel"], { rLoop.getRight() - 170, ly - 6, 64, 80 });
        byId["loopFollow"]->setBounds (rLoop.getRight() - 104, ly + 18, 100, 22);
    }

    // filter
    {
        caption (rFilter.getX() + 12, rFilter.getY() + 26, 40, "TYPE");
        byId["filterType"]->setBounds (rFilter.getX() + 50, rFilter.getY() + 22, 80, 22);
        placeKnob (*knobById["filter"], { rFilter.getX() + 8, rFilter.getY() + 50, 90, 96 });
        placeKnob (*knobById["resonance"], { rFilter.getX() + 104, rFilter.getY() + 62, 62, 76 });
        placeKnob (*knobById["modRate"], { rFilter.getX() + 168, rFilter.getY() + 62, 62, 76 });
        placeKnob (*knobById["modDepth"], { rFilter.getX() + 232, rFilter.getY() + 62, 62, 76 });
    }

    // delay
    {
        placeKnob (*knobById["delayMs"], { rDelay.getX() + 8, rDelay.getY() + 24, 62, 74 });
        placeKnob (*knobById["feedback"], { rDelay.getX() + 74, rDelay.getY() + 24, 62, 74 });
        byId["delaySync"]->setBounds (rDelay.getX() + 12, rDelay.getY() + 102, 64, 20);
        byId["delayDiv"]->setBounds (rDelay.getX() + 78, rDelay.getY() + 100, 72, 22);
        knobById["delayLevel"]->slider.setBounds (rDelay.getRight() - 72, rDelay.getY() + 22, 50, 104);
    }

    // mixer
    {
        const int mx = rMix.getX() + 10, my = rMix.getY() + 26;
        placeKnob (*knobById["deckLevel"], { mx, my, 66, 80 });
        placeKnob (*knobById["lineLevel"], { mx + 70, my, 66, 80 });
        knobById["crossfader"]->slider.setBounds (mx + 160, my + 70, 300, 50);
        caption (mx + 160, my + 120, 60, "DECK");
        caption (mx + 280, my + 120, 120, "CROSSFADER");
        caption (mx + 428, my + 120, 60, "LINE");
        byId["hamster"]->setBounds (mx + 160, my + 6, 100, 22);
        caption (mx + 270, my + 10, 50, "CURVE");
        byId["faderCurve"]->setBounds (mx + 316, my + 6, 90, 22);
        byId["speaker"]->setBounds (mx + 10, my + 104, 110, 22);
        placeKnob (*knobById["output"], { rMix.getRight() - 112, my + 30, 72, 86 });
    }
}

void PlatterboxEditor::timerCallback()
{
    const auto& t = lnf.theme();
    platter.tick();
    wave.repaint();
    meter = jmax (proc.getEngine().outputPeak(), meter * 0.85f);

    const auto& eng = proc.getEngine();
    motorButton.setToggleState (eng.getParams().motorOn, dontSendNotification);
    for (int i = 0; i < 3; ++i) speedButtons[(size_t) i].setToggleState (eng.getParams().speed == i, dontSendNotification);

    const auto st = eng.getLooper().state();
    String txt = "REC"; Colour led = t.panel.darker (0.06f);
    switch (st)
    {
        case pb::LoopState::empty: txt = "REC"; break;
        case pb::LoopState::recording: txt = "PLAY"; led = Colour (0xffe0342a); break;
        case pb::LoopState::playing: txt = "DUB"; led = Colour (0xff3faa5a); break;
        case pb::LoopState::overdubbing: txt = "PLAY"; led = Colour (0xffef8a25); break;
        case pb::LoopState::stopped: txt = "PLAY"; break;
    }
    if (loopMain.getButtonText() != txt) loopMain.setButtonText (txt);
    loopMain.getProperties().set ("led", led.toString());
    loopUndo.setEnabled (eng.getLooper().canUndo());
    loopRev.setToggleState (eng.getLooper().isReversed(), dontSendNotification);
    loopMain.repaint();

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    repaint (rLoop.getX(), rLoop.getBottom() - 30, 520, 26);
    repaint (rMix.getRight() - 30, rMix.getY(), 30, rMix.getHeight());
    repaint (rLeft.getX(), rLeft.getBottom() - 50, rLeft.getWidth(), 50);
}

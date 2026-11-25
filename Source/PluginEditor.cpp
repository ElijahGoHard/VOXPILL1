#include "PluginEditor.h"

namespace pal
{
    const juce::Colour pink    { 0xffff3fb4 };
    const juce::Colour pinkHi  { 0xffff8fd6 };
    const juce::Colour pinkDim { 0xff9a2a78 };
    const juce::Colour screenA { 0xff3a0a2e };
    const juce::Colour screenB { 0xff1a0315 };
    const juce::Colour body    { 0xffe6e6ea };
    const juce::Colour bodyDk  { 0xffbcbcc4 };
    const juce::Colour ink     { 0xff2a2a33 };
}
static juce::Font mono (float h, bool bold = false)
{ return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain)); }

// ============================================================ look and feel
struct CrtLook : juce::LookAndFeel_V4
{
    CrtLook()
    {
        setColour (juce::PopupMenu::backgroundColourId, pal::screenB);
        setColour (juce::PopupMenu::textColourId, pal::pinkHi);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::pinkDim);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::textColourId, pal::pinkHi);
        setColour (juce::Slider::textBoxTextColourId, pal::pinkHi);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (12.f); }
    juce::Font getPopupMenuFont() override { return mono (12.f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return mono (11.f); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        float d = juce::jmin (b.getWidth(), b.getHeight()), r = d * 0.5f;
        auto c = b.getCentre();
        // tick ring
        for (int i = 0; i <= 20; ++i)
        {
            float a = a0 + (a1 - a0) * (float) i / 20.f, rr = r + 2.f;
            bool lit = ((float) i / 20.f) <= pos + 0.001f;
            g.setColour (lit ? pal::pink : pal::bodyDk.darker (0.1f));
            g.fillEllipse (c.x + std::sin (a) * rr - 1.4f, c.y - std::cos (a) * rr - 1.4f, 2.8f, 2.8f);
        }
        auto kr = juce::Rectangle<float> (d * 0.82f, d * 0.82f).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillEllipse (kr.translated (0, 3.f).expanded (1.f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbfbfd), kr.getX(), kr.getY(), juce::Colour (0xffcfcfd8), kr.getRight(), kr.getBottom(), false));
        g.fillEllipse (kr);
        g.setColour (juce::Colours::white.withAlpha (0.8f)); g.drawEllipse (kr.reduced (1.f), 1.f);
        g.setColour (pal::bodyDk); g.drawEllipse (kr, 1.f);
        float a = a0 + (a1 - a0) * pos;
        juce::Path p; p.addRoundedRectangle (-1.8f, -kr.getHeight() * 0.46f, 3.6f, kr.getHeight() * 0.26f, 1.8f);
        g.setColour (s.isEnabled() ? pal::pink : pal::bodyDk);
        g.fillPath (p, juce::AffineTransform::rotation (a).translated (c));
    }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        float cy = (float) y + (float) h * 0.5f;
        g.setColour (pal::pinkDim.withAlpha (0.5f)); g.fillRoundedRectangle ((float) x, cy - 2.f, (float) w, 4.f, 2.f);
        float x0 = s.getMinimum() < 0 ? (float) x + (float) w * 0.5f : (float) x;
        g.setColour (pal::pink); g.fillRoundedRectangle (juce::jmin (x0, pos), cy - 2.f, std::abs (pos - x0), 4.f, 2.f);
        g.setColour (pal::pinkHi); g.fillRect (pos - 3.f, cy - 7.f, 6.f, 14.f);
    }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto b = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (pal::pinkDim.withAlpha (0.35f)); g.fillRect (b);
        g.setColour (pal::pink); g.drawRect (b, 1.f);
        juce::Path t; t.addTriangle ((float) w - 16, (float) h * 0.4f, (float) w - 8, (float) h * 0.4f, (float) w - 12, (float) h * 0.65f);
        g.fillPath (t);
    }
    void positionComboBoxText (juce::ComboBox& c, juce::Label& l) override { l.setBounds (4, 0, c.getWidth() - 20, c.getHeight()); l.setFont (getComboBoxFont (c)); }
};

// ============================================================ chip button
struct Chip : juce::Button
{
    Chip (const juce::String& t) : juce::Button (t) { setClickingTogglesState (false); }
    bool active = false, light = false;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.f);
        if (light)
        {
            g.setColour (active ? pal::pink : pal::bodyDk.brighter (0.3f)); g.fillRoundedRectangle (b, 15.f);
            g.setColour (active ? juce::Colours::white : pal::ink.withAlpha (0.7f)); g.setFont (mono (11.f, true));
            g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred); return;
        }
        g.setColour (active ? pal::pink : (over ? pal::pinkDim : pal::pinkDim.withAlpha (0.35f)));
        g.fillRoundedRectangle (b, 3.f);
        g.setColour (pal::pink.withAlpha (down ? 1.f : 0.7f)); g.drawRoundedRectangle (b, 3.f, 1.f);
        g.setColour (active ? juce::Colours::black : pal::pinkHi); g.setFont (mono (9.f, true));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
};

// ============================================================ content
struct VoxPillEditor::Content : juce::Component, juce::Timer, juce::TextEditor::Listener
{
    enum Tab { Talk, Tweak, Help };

    VoxPillProcessor& proc;
    CrtLook look;
    Tab tab = Talk;
    juce::TextEditor input;
    juce::OwnedArray<Chip> chips;
    Chip tabTalk { "TALK" }, tabTweak { "TWEAK" }, tabHelp { "HELP" }, resetBtn { "RESET" }, sendBtn { "SEND" }, rawBtn { "RAW" }, afterBtn { "AFTER" };

    struct Knob { juce::Slider s; juce::String label; juce::Rectangle<int> area; std::unique_ptr<juce::SliderParameterAttachment> att; };
    juce::OwnedArray<Knob> knobs;
    struct Row { juce::Slider s; juce::String label; std::unique_ptr<juce::SliderParameterAttachment> att; };
    juce::OwnedArray<Row> rows;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> keyAtt, scaleAtt;

    int reveal = 100000; juce::String revealTarget;
    float blink = 0, t = 0, bounce = 0;
    int placeholderTick = 0;

    juce::Rectangle<int> screen { 56, 56, 648, 298 }, termArea { 72, 94, 470, 176 };

    Content (VoxPillProcessor& p) : proc (p)
    {
        setLookAndFeel (&look);
        setSize (VoxPillEditor::baseW, VoxPillEditor::baseH);

        auto mk = [&] (const char* id, const char* label, int cx, int cy, int d, bool big)
        {
            auto* k = knobs.add (new Knob());
            k->label = label; k->area = juce::Rectangle<int> (d, d).withCentre ({ cx, cy });
            k->s.setSliderStyle (juce::Slider::RotaryVerticalDrag); k->s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            k->s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            k->s.setPopupDisplayEnabled (true, true, this);
            addAndMakeVisible (k->s); k->s.setBounds (k->area);
            k->att = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), k->s, nullptr);
            juce::ignoreUnused (big);
        };
        mk ("volume", "VOLUME", 130, 444, 88, true); mk ("dose", "DOSE", 290, 444, 88, true);
        mk ("tone", "TONE", 450, 444, 88, true);     mk ("mix", "MIX", 610, 444, 88, true);
        mk ("air", "AIR", 130, 572, 52, false);      mk ("gate", "GATE", 215, 572, 52, false); mk ("comp", "COMP", 300, 572, 52, false);

        // tweak rows
        struct R { const char* id; const char* label; };
        for (auto r : { R { "tune", "TUNE" }, R { "tuneSpeed", "TUNE SPEED" }, R { "deess", "DE-ESS" }, R { "width", "WIDTH" }, R { "chorus", "CHORUS" },
                        R { "dbl", "DOUBLER" }, R { "reverb", "REVERB" }, R { "echo", "ECHO" }, R { "sat", "SATURATE" } })
        {
            auto* row = rows.add (new Row()); row->label = r.label;
            row->s.setSliderStyle (juce::Slider::LinearHorizontal); row->s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
            row->s.setColour (juce::Slider::textBoxTextColourId, pal::pinkHi);
            row->s.setColour (juce::Slider::textBoxBackgroundColourId, pal::pinkDim.withAlpha (0.25f));
            row->s.setColour (juce::Slider::textBoxOutlineColourId, pal::pink.withAlpha (0.6f));
            row->s.setColour (juce::Label::textColourId, pal::pinkHi);
            addChildComponent (row->s);
            row->att = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (r.id), row->s, nullptr);
        }
        keyBox.addItemList (proc.apvts.getParameter ("key")->getAllValueStrings(), 1);
        scaleBox.addItemList (proc.apvts.getParameter ("scale")->getAllValueStrings(), 1);
        addChildComponent (keyBox); addChildComponent (scaleBox);
        keyAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.apvts.getParameter ("key"), keyBox, nullptr);
        scaleAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.apvts.getParameter ("scale"), scaleBox, nullptr);

        // prompt
        input.setMultiLine (false); input.setReturnKeyStartsNewLine (false);
        input.setFont (mono (13.f)); input.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        input.setColour (juce::TextEditor::textColourId, juce::Colours::white); input.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        input.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        input.setColour (juce::CaretComponent::caretColourId, pal::pink); input.setColour (juce::TextEditor::highlightColourId, pal::pinkDim);
        input.setTextToShowWhenEmpty ("type what you want to hear...", pal::pinkDim);
        input.setIndents (2, 3); input.addListener (this);
        addAndMakeVisible (input);

        for (auto* b : { &tabTalk, &tabTweak, &tabHelp, &resetBtn, &sendBtn }) addAndMakeVisible (b);
        tabTalk.onClick = [this] { setTab (Talk); }; tabTweak.onClick = [this] { setTab (Tweak); }; tabHelp.onClick = [this] { setTab (Help); };
        resetBtn.onClick = [this] { submit ("reset"); }; sendBtn.onClick = [this] { submit (input.getText()); };
        for (auto* b : { &rawBtn, &afterBtn }) addAndMakeVisible (b);
        rawBtn.onClick = [this] { setAfter (false); }; afterBtn.onClick = [this] { setAfter (true); };

        for (auto& n : VoxPillProcessor::presetNames())
        {
            auto* c = chips.add (new Chip (n.toUpperCase())); addAndMakeVisible (c);
            c->onClick = [this, n] { submit (n); };
        }
        if (proc.history.isEmpty()) { proc.history.add ("DOSE: yeah i hear you. say the word."); }
        layoutAll();
        startTimerHz (30);
    }
    ~Content() override { setLookAndFeel (nullptr); }

    void setAfter (bool a) { if (auto* p = proc.apvts.getParameter ("after")) p->setValueNotifyingHost (a ? 1.f : 0.f); }
    void setTab (Tab tb) { tab = tb; layoutAll(); repaint(); }

    void submit (const juce::String& text)
    {
        if (text.trim().isEmpty()) return;
        proc.runPrompt (text.trim());
        input.clear(); revealTarget = proc.history[proc.history.size() - 1]; reveal = 0; bounce = 1.f;
        if (tab != Talk) setTab (Talk);
    }
    void textEditorReturnKeyPressed (juce::TextEditor&) override { submit (input.getText()); }

    void layoutAll()
    {
        const bool talk = tab == Talk, tweak = tab == Tweak;
        input.setVisible (talk); sendBtn.setVisible (talk);
        for (auto* c : chips) c->setVisible (talk);
        for (auto* r : rows) r->s.setVisible (tweak);
        keyBox.setVisible (tweak); scaleBox.setVisible (tweak);

        tabTalk.setBounds (72, 62, 64, 22); tabTweak.setBounds (140, 62, 64, 22); tabHelp.setBounds (208, 62, 64, 22);
        resetBtn.setBounds (628, 62, 62, 22);
        tabTalk.active = tab == Talk; tabTweak.active = tab == Tweak; tabHelp.active = tab == Help;
        input.setBounds (92, 286, 384, 26); sendBtn.setBounds (484, 286, 58, 26);
        const int cw = 66; int cx = 72;
        for (auto* c : chips) { c->setBounds (cx, 322, cw, 22); cx += cw + 2; }
        rawBtn.light = afterBtn.light = true; rawBtn.setBounds (560, 570, 70, 34); afterBtn.setBounds (636, 570, 80, 34);
        // tweak rows: two columns
        for (int i = 0; i < rows.size(); ++i)
        {
            int col = i / 5, ri = i % 5;
            rows[i]->s.setBounds (72 + col * 312 + 92, 96 + ri * 34, 210, 26);
        }
        keyBox.setBounds (72 + 312 + 92, 96 + 4 * 34 + 34, 96, 24); scaleBox.setBounds (72 + 312 + 92 + 104, 96 + 4 * 34 + 34, 110, 24);
    }

    void timerCallback() override
    {
        t += 1.f / 30.f; bounce *= 0.9f;
        if (blink > 0) blink -= 0.15f; else if (juce::Random::getSystemRandom().nextInt (90) == 0) blink = 1.f;
        if (reveal < revealTarget.length()) reveal += 2;
        rawBtn.active = ! (afterBtn.active = proc.apvts.getRawParameterValue ("after")->load() > 0.5f);
        rawBtn.repaint(); afterBtn.repaint();
        for (auto* c : chips) { c->active = false; }
        repaint();
    }

    // ------------------------------------------------------------ paint
    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.fillAll (juce::Colour (0xff141418));
        auto body = full.reduced (12.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f4f7), 0, body.getY(), pal::bodyDk, 0, body.getBottom(), false));
        g.fillRoundedRectangle (body, 22.f);
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawRoundedRectangle (body.reduced (1.f), 22.f, 1.5f);
        // screws + grille
        for (auto p : { juce::Point<float> { 30, 30 }, { 730, 30 }, { 30, 610 }, { 730, 610 } })
        { g.setColour (pal::bodyDk); g.fillEllipse (p.x - 5, p.y - 5, 10, 10); g.setColour (pal::ink.withAlpha (0.5f)); g.drawLine (p.x - 3, p.y - 3, p.x + 3, p.y + 3, 1.2f); }

        // bezel
        auto bez = juce::Rectangle<float> (40, 40, 680, 330);
        g.setColour (juce::Colour (0xff9d9da6)); g.fillRoundedRectangle (bez, 18.f);
        g.setColour (juce::Colour (0xff4a4a55)); g.fillRoundedRectangle (bez.reduced (6.f), 14.f);

        // screen
        auto sc = screen.toFloat();
        g.saveState(); juce::Path clip; clip.addRoundedRectangle (sc, 10.f); g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (pal::screenA, sc.getCentreX(), sc.getCentreY(), pal::screenB, sc.getX(), sc.getY(), true));
        g.fillRect (sc);
        float lvl = juce::jlimit (0.f, 1.f, proc.outLevel.load() * 2.f);
        g.setColour (pal::pink.withAlpha (0.05f + 0.07f * lvl)); g.fillRect (sc);
        paintTab (g);
        // scanlines + flicker
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        for (float y = sc.getY(); y < sc.getBottom(); y += 3.f) g.fillRect (sc.getX(), y, sc.getWidth(), 1.f);
        float sweep = std::fmod (t * 60.f, sc.getHeight() + 80.f) - 40.f;
        g.setGradientFill (juce::ColourGradient (pal::pink.withAlpha (0.f), 0, sc.getY() + sweep - 30, pal::pink.withAlpha (0.07f), 0, sc.getY() + sweep, false));
        g.fillRect (sc.getX(), sc.getY() + sweep - 30, sc.getWidth(), 30.f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, sc.getCentreX(), sc.getCentreY(), juce::Colours::black.withAlpha (0.55f), sc.getX(), sc.getY(), true));
        g.fillRect (sc);
        g.restoreState();
        g.setColour (pal::pink.withAlpha (0.5f)); g.drawRoundedRectangle (sc, 10.f, 1.5f);

        // logo plate
        g.setColour (pal::ink); g.setFont (mono (15.f, true));
        g.drawText ("VOX", 270, 374, 40, 22, juce::Justification::centredRight);
        g.setColour (pal::pink); g.drawText ("PILL", 310, 374, 60, 22, juce::Justification::centredLeft);
        g.setColour (pal::ink.withAlpha (0.5f)); g.setFont (mono (9.f));
        g.drawText ("TALK-TO-IT VOCAL CHAIN", 380, 374, 240, 22, juce::Justification::centredLeft);

        // knob labels
        g.setFont (mono (10.f, true)); g.setColour (pal::ink);
        for (auto* k : knobs)
            g.drawText (k->label, k->area.getX() - 20, k->area.getBottom() + 1, k->area.getWidth() + 40, 14, juce::Justification::centred);

        // meters
        struct M { const char* n; float v; juce::Colour c; };
        float inL = juce::jlimit (0.f, 1.f, proc.inLevel.load()), outL = juce::jlimit (0.f, 1.f, proc.outLevel.load());
        float gr = juce::jlimit (0.f, 1.f, proc.grDb.load() / 18.f);
        int my = 536;
        for (auto m : { M { "IN", inL, pal::ink }, M { "OUT", outL, pal::pink }, M { "GR", gr, pal::pinkDim } })
        {
            g.setColour (pal::ink); g.setFont (mono (9.f, true)); g.drawText (m.n, 380, my, 34, 12, juce::Justification::centredLeft);
            g.setColour (pal::bodyDk); g.fillRoundedRectangle (414.f, (float) my + 2, 130.f, 8.f, 3.f);
            g.setColour (m.c); g.fillRoundedRectangle (414.f, (float) my + 2, juce::jmax (4.f, 130.f * m.v), 8.f, 3.f);
            my += 18;
        }
        g.setColour (pal::bodyDk.brighter (0.15f)); g.fillRoundedRectangle (556.f, 566.f, 164.f, 42.f, 21.f);
        g.setColour (pal::ink.withAlpha (0.6f)); g.setFont (mono (9.f, true)); g.drawText ("A / B  -  HEAR THE DIFFERENCE", 560, 550, 200, 12, juce::Justification::centredLeft);
    }

    void paintTab (juce::Graphics& g)
    {
        // status line
        g.setFont (mono (9.f)); g.setColour (pal::pinkDim.brighter (0.3f));
        float hz = proc.detectedHz.load();
        juce::String st = "VOXPILL v1.0  |  KEY " + proc.apvts.getParameter ("key")->getCurrentValueAsText() + " " + proc.apvts.getParameter ("scale")->getCurrentValueAsText().toUpperCase();
        if (hz > 0) st << "  |  F0 " << juce::String (juce::roundToInt (hz)) << "HZ";
        g.drawText (st, 280, 62, 340, 22, juce::Justification::centredLeft);
        g.setColour (pal::pink.withAlpha (0.4f)); g.drawLine (72, 88, 688, 88, 1.f);

        if (tab == Talk)
        {
            // history
            juce::AttributedString as; as.setWordWrap (juce::AttributedString::byWord);
            int from = juce::jmax (0, proc.history.size() - 14);
            for (int i = from; i < proc.history.size(); ++i)
            {
                bool last = i == proc.history.size() - 1;
                juce::String line = proc.history[i];
                if (last && reveal < line.length()) line = line.substring (0, reveal) + "_";
                bool user = line.startsWith ("> ");
                as.append (line + "\n", mono (12.f), user ? pal::pinkDim.brighter (0.5f) : (last ? juce::Colours::white : pal::pinkHi));
            }
            juce::TextLayout tl; tl.createLayout (as, (float) termArea.getWidth());
            g.saveState(); g.reduceClipRegion (termArea);
            float off = juce::jmin (0.f, (float) termArea.getHeight() - tl.getHeight());
            tl.draw (g, juce::Rectangle<float> ((float) termArea.getX(), (float) termArea.getY() + off, (float) termArea.getWidth(), tl.getHeight() + 4));
            g.restoreState();
            // prompt box
            g.setColour (pal::pink.withAlpha (0.8f)); g.drawRect (juce::Rectangle<int> (76, 282, 470, 34), 1);
            g.setFont (mono (13.f, true)); g.drawText (">", 76, 286, 12, 26, juce::Justification::centred);
            drawMascot (g, { 570.f, 100.f, 120.f, 170.f });
        }
        else if (tab == Tweak)
        {
            g.setFont (mono (11.f, true)); g.setColour (pal::pinkHi);
            for (int i = 0; i < rows.size(); ++i)
                g.drawText (rows[i]->label, rows[i]->s.getX() - 92, rows[i]->s.getY(), 88, 26, juce::Justification::centredLeft);
            g.drawText ("KEY / SCALE", keyBox.getX() - 92, keyBox.getY(), 90, 24, juce::Justification::centredLeft);
            g.setFont (mono (10.f)); g.setColour (pal::pinkDim.brighter (0.5f));
            g.drawText ("tune needs a mono, in-tune-ish vocal. set key + scale first.", 72, 292, 600, 40, juce::Justification::topLeft);
        }
        else
        {
            g.setFont (mono (12.f)); g.setColour (pal::pinkHi);
            g.drawFittedText (
                "TYPE WHAT YOU WANT. THE CHAIN MOVES.\n\n"
                "  \"darker and wider\"\n  \"hard tune in f minor\"\n  \"less harsh, more air\"\n  \"thicker with a slap echo\"\n"
                "  \"squash it a bit, kill the room noise\"\n  \"dreamy with lots of reverb\"\n  \"reset\"\n\n"
                "OR TAP A STYLE CHIP ON THE TALK TAB.\nRAW / AFTER = A/B THE CHAIN.",
                juce::Rectangle<int> (72, 98, 600, 240), juce::Justification::topLeft, 20);
        }
    }

    void drawMascot (juce::Graphics& g, juce::Rectangle<float> r)
    {
        float lvl = juce::jlimit (0.f, 1.f, proc.outLevel.load() * 3.f);
        float hop = std::sin (t * 4.f) * 2.f * lvl + bounce * -8.f * std::abs (std::sin (t * 12.f));
        auto c = r.getCentre().translated (0, hop + 6.f);
        juce::Path pill; auto body = juce::Rectangle<float> (60.f, 104.f).withCentre (c);
        pill.addRoundedRectangle (body, 30.f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (std::sin (t * 1.3f) * 0.07f + 0.12f, c.x, c.y));
        // feet
        g.setColour (pal::pinkHi); g.fillEllipse (c.x - 24, body.getBottom() - 4, 18, 10); g.fillEllipse (c.x + 6, body.getBottom() - 4, 18, 10);
        // halves
        g.saveState(); g.reduceClipRegion (juce::Rectangle<int> ((int) body.getX() - 2, (int) body.getY() - 2, 70, (int) (body.getHeight() * 0.5f) + 2));
        g.setColour (pal::pink); g.fillPath (pill); g.restoreState();
        g.saveState(); g.reduceClipRegion (juce::Rectangle<int> ((int) body.getX() - 2, (int) c.y, 70, 60));
        g.setColour (juce::Colour (0xffffd9f0)); g.fillPath (pill); g.restoreState();
        g.setColour (juce::Colours::white.withAlpha (0.5f)); g.fillRoundedRectangle (body.getX() + 8, body.getY() + 10, 6.f, 34.f, 3.f);
        g.setColour (pal::pinkHi); g.strokePath (pill, juce::PathStrokeType (1.5f));
        // arms
        g.setColour (pal::pink); g.drawLine (body.getX() + 2, c.y + 4, body.getX() - 10, c.y + 18 - lvl * 14, 4.f);
        g.drawLine (body.getRight() - 2, c.y + 4, body.getRight() + 10, c.y + 18 - lvl * 14, 4.f);
        // eyes
        float ey = body.getY() + 40.f; float eh = blink > 0 ? 2.f : 11.f;
        g.setColour (juce::Colour (0xff1a0315));
        g.fillEllipse (c.x - 19, ey, 14, eh); g.fillEllipse (c.x + 5, ey, 14, eh);
        if (blink <= 0) { g.setColour (juce::Colours::white); g.fillEllipse (c.x - 15, ey + 2, 4, 4); g.fillEllipse (c.x + 9, ey + 2, 4, 4); }
        // mouth
        float mo = 2.f + lvl * 9.f;
        g.setColour (juce::Colour (0xff1a0315)); g.fillEllipse (c.x - 6, body.getY() + 62.f, 12, mo);
        // little horns
        g.setColour (pal::pinkHi); juce::Path h1; h1.addTriangle (c.x - 22, body.getY() + 8, c.x - 12, body.getY() - 6, c.x - 6, body.getY() + 4); g.fillPath (h1);
        juce::Path h2; h2.addTriangle (c.x + 22, body.getY() + 8, c.x + 12, body.getY() - 6, c.x + 6, body.getY() + 4); g.fillPath (h2);
        g.restoreState();
    }
};

// ============================================================ editor wrapper
VoxPillEditor::VoxPillEditor (VoxPillProcessor& p) : AudioProcessorEditor (&p)
{
    content = std::make_unique<Content> (p);
    addAndMakeVisible (*content);
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);
}
VoxPillEditor::~VoxPillEditor() { content.reset(); }
void VoxPillEditor::resized()
{
    content->setBounds (0, 0, baseW, baseH);
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
}

#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;

static const char* kNotes[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

juce::AudioProcessorValueTreeState::ParameterLayout VoxPillProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (const char* id, const char* name, float def)
    { return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.f, 1.f, 0.001f), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; })); };
    l.add (std::make_unique<APF> (juce::ParameterID { "volume", 1 }, "Volume", juce::NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f,
           juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (pct ("dose", "Dose", 0.8f));
    l.add (std::make_unique<APF> (juce::ParameterID { "tone", 1 }, "Tone", juce::NormalisableRange<float> (-1.f, 1.f, 0.001f), 0.f));
    l.add (pct ("mix", "Mix", 1.f));
    l.add (pct ("air", "Air", 0.3f));
    l.add (pct ("gate", "Gate", 0.f));
    l.add (pct ("comp", "Comp", 0.4f));
    l.add (pct ("tune", "Tune", 0.f));
    l.add (pct ("tuneSpeed", "Tune Speed", 0.5f));
    l.add (std::make_unique<APC> (juce::ParameterID { "key", 1 }, "Key", juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 0));
    l.add (std::make_unique<APC> (juce::ParameterID { "scale", 1 }, "Scale", juce::StringArray { "Chromatic", "Major", "Minor" }, 0));
    l.add (pct ("deess", "De-ess", 0.3f));
    l.add (std::make_unique<APF> (juce::ParameterID { "width", 1 }, "Width", juce::NormalisableRange<float> (0.f, 2.f, 0.01f), 1.f));
    l.add (pct ("chorus", "Chorus", 0.f));
    l.add (pct ("dbl", "Doubler", 0.f));
    l.add (pct ("reverb", "Reverb", 0.1f));
    l.add (pct ("echo", "Echo", 0.f));
    l.add (pct ("sat", "Saturation", 0.1f));
    l.add (std::make_unique<APB> (juce::ParameterID { "after", 1 }, "After (A/B)", true));
    return l;
}

VoxPillProcessor::VoxPillProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

bool VoxPillProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == l.getMainOutputChannelSet();
}

void VoxPillProcessor::prepareToPlay (double rate, int block)
{
    sr = rate;
    juce::dsp::ProcessSpec spec { rate, (juce::uint32) block, 1 };
    for (auto* f : { &hp[0], &hp[1], &lowMid[0], &lowMid[1], &presence[0], &presence[1], &air[0], &air[1], &tilt[0], &tilt[1],
                     &essBand[0], &essBand[1], &essCut[0], &essCut[1], &toneLP[0], &toneLP[1] })
    { f->prepare (spec); f->reset(); }
    for (int c = 0; c < 2; ++c)
    {
        *hp[c].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (rate, 80.f, 0.707f);
        *essBand[c].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (rate, 5500.f, 0.707f);
        dl[c].assign ((size_t) (rate * 0.12), 0.f);
        echo[c].assign ((size_t) (rate * 2.5), 0.f);
        shifter[c].prepare (rate);
    }
    det.prepare (rate);
    dw = ew = 0; lfoPh = 0; gateEnv = 0; gateGain = 1; compEnv = 0; smoothGr = 0; curRatio = 1.f;
    reverb.reset();
    volSm.reset (rate, 0.03); mixSm.reset (rate, 0.03);
    volSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (get ("volume")));
    mixSm.setCurrentAndTargetValue (get ("after") > 0.5f ? 1.f : 0.f);
    dry.setSize (2, block);
}

void VoxPillProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    if (buf.getNumChannels() < 2) return;
    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    for (int c = 0; c < 2; ++c) dry.copyFrom (c, 0, buf, c, 0, n);

    // --- parameters
    const float dose = get ("dose"), tone = get ("tone"), airA = get ("air"), gateA = get ("gate"), compA = get ("comp") * (0.4f + 0.6f * dose);
    const float tuneA = get ("tune"), tuneSp = get ("tuneSpeed");
    const float deessA = get ("deess") * (0.5f + 0.5f * dose), width = get ("width");
    const float chorusA = get ("chorus"), dblA = get ("dbl"), revA = get ("reverb"), echoA = get ("echo"), satA = get ("sat") * (0.4f + 0.6f * dose);
    const int key = (int) get ("key"), scale = (int) get ("scale");
    const bool after = get ("after") > 0.5f;
    volSm.setTargetValue (juce::Decibels::decibelsToGain (get ("volume")));
    mixSm.setTargetValue (after ? get ("mix") : 0.f);

    // --- EQ coefficients (per block)
    for (int c = 0; c < 2; ++c)
    {
        *lowMid[c].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 320.f, 0.9f, juce::Decibels::decibelsToGain (-2.5f * dose));
        *presence[c].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 3200.f, 0.8f, juce::Decibels::decibelsToGain (2.f * dose + 2.f * juce::jmax (0.f, tone)));
        *air[c].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 10000.f, 0.7f, juce::Decibels::decibelsToGain (airA * 9.f));
        *tilt[c].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2200.f, 0.6f, juce::Decibels::decibelsToGain (tone * 7.f));
        float lpHz = tone < 0 ? juce::jmap (tone, -1.f, 0.f, 3500.f, 20000.f) : 20000.f;
        *toneLP[c].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, juce::jmin (lpHz, (float) (sr * 0.45)), 0.6f);
    }

    // --- tempo for echo
    double bpm = 120;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    const int echoSamples = juce::jlimit (1000, (int) echo[0].size() - 2, (int) (sr * 60.0 / juce::jmax (40.0, bpm) * 0.75));

    const float gateThr = juce::Decibels::decibelsToGain (-75.f + gateA * 45.f);
    const float aG = std::exp (-1.f / (float) (sr * 0.002f)), rG = std::exp (-1.f / (float) (sr * 0.12f));
    const float cA = std::exp (-1.f / (float) (sr * 0.006f)), cR = std::exp (-1.f / (float) (sr * 0.09f));
    const float eA = std::exp (-1.f / (float) (sr * 0.001f)), eR = std::exp (-1.f / (float) (sr * 0.02f));
    const float compThr = -10.f - compA * 16.f, ratio = 1.5f + compA * 5.f;
    const float makeup = juce::Decibels::decibelsToGain ((-compThr) * (1.f - 1.f / ratio) * 0.38f);
    const float tau = 0.0015f + 0.25f * std::pow (1.f - tuneSp, 2.f);
    const float ratioSmooth = 1.f - std::exp (-1.f / (float) (sr * tau));
    const float satDrive = 1.f + satA * 5.f, satNorm = 1.f / satDrive;
    const float pi2 = juce::MathConstants<float>::twoPi;
    const float lfoInc = 0.45f / (float) sr;
    const float echoFb = 0.38f, echoMix = echoA * 0.45f * (0.5f + 0.5f * dose);

    float* L = buf.getWritePointer (0);
    float* R = buf.getWritePointer (1);
    float inPk = 0, outPk = 0, grMax = 0;
    float targetRatio = 1.f;

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        inPk = juce::jmax (inPk, std::abs (l), std::abs (r));

        // gate / expander
        if (gateA > 0.f)
        {
            float lvl = juce::jmax (std::abs (l), std::abs (r));
            gateEnv = lvl > gateEnv ? aG * gateEnv + (1 - aG) * lvl : rG * gateEnv + (1 - rG) * lvl;
            float tg = gateEnv > gateThr ? 1.f : 0.08f;
            gateGain += (tg - gateGain) * (tg > gateGain ? 0.01f : 0.0015f);
            l *= gateGain; r *= gateGain;
        }
        l = hp[0].processSample (l); r = hp[1].processSample (r);

        // pitch correction
        if (tuneA > 0.001f)
        {
            det.push (0.5f * (l + r));
            if ((i & 31) == 0)
            {
                targetRatio = 1.f;
                const float f0 = det.f0;
                if (f0 > 60.f && f0 < 900.f)
                {
                    float m = 69.f + 12.f * std::log2 (f0 / 440.f);
                    float rel = std::fmod (m - (float) key + 120.f, 12.f), base = m - rel;
                    static const int major[] = { 0,2,4,5,7,9,11 }, minor[] = { 0,2,3,5,7,8,10 };
                    float bestD = 99.f, tgt = m;
                    for (int o = -1; o <= 1; ++o)
                        for (int k = 0; k < (scale == 0 ? 12 : 7); ++k)
                        {
                            float nt = base + (float) key * 0.f + (float) o * 12.f + (scale == 0 ? (float) k : (float) (scale == 1 ? major[k] : minor[k]));
                            float d = std::abs (nt - m);
                            if (d < bestD) { bestD = d; tgt = nt; }
                        }
                    float semis = juce::jlimit (-2.f, 2.f, (tgt - m) * tuneA);
                    targetRatio = std::pow (2.f, semis / 12.f);
                }
            }
            curRatio += (targetRatio - curRatio) * ratioSmooth;
            float b = juce::jlimit (0.f, 1.f, std::abs (1.f - curRatio) * 120.f);
            float sl = shifter[0].process (l, curRatio), sr2 = shifter[1].process (r, curRatio);
            l += (sl - l) * b; r += (sr2 - r) * b;
        }

        // EQ
        l = air[0].processSample (tilt[0].processSample (presence[0].processSample (lowMid[0].processSample (l))));
        r = air[1].processSample (tilt[1].processSample (presence[1].processSample (lowMid[1].processSample (r))));
        l = toneLP[0].processSample (l); r = toneLP[1].processSample (r);

        // de-esser
        if (deessA > 0.001f)
        {
            float sl = essBand[0].processSample (l), sr2 = essBand[1].processSample (r);
            float lvl = juce::jmax (std::abs (sl), std::abs (sr2));
            deessEnv = lvl > deessEnv ? eA * deessEnv + (1 - eA) * lvl : eR * deessEnv + (1 - eR) * lvl;
            float thr = juce::jmap (deessA, 0.f, 1.f, 0.12f, 0.015f);
            float g = deessEnv > thr ? std::pow (thr / deessEnv, 0.8f * deessA + 0.1f) : 1.f;
            l -= sl * (1.f - g); r -= sr2 * (1.f - g);
        }

        // compressor
        {
            float lvl = juce::jmax (std::abs (l), std::abs (r));
            compEnv = lvl > compEnv ? cA * compEnv + (1 - cA) * lvl : cR * compEnv + (1 - cR) * lvl;
            float db = juce::Decibels::gainToDecibels (compEnv + 1e-9f), gr = 0.f;
            const float knee = 6.f;
            float over = db - compThr;
            if (over > knee * 0.5f) gr = over * (1.f - 1.f / ratio);
            else if (over > -knee * 0.5f) gr = (1.f - 1.f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.f * knee);
            float g = juce::Decibels::decibelsToGain (-gr) * (compA > 0.001f ? makeup : 1.f);
            if (compA > 0.001f) { l *= g; r *= g; }
            grMax = juce::jmax (grMax, gr);
        }

        // saturation
        if (satA > 0.001f) { l = std::tanh (l * satDrive) * satNorm; r = std::tanh (r * satDrive) * satNorm; }

        // chorus + doubler (shared delay line)
        float wl = l, wr = r;
        if (chorusA > 0.001f || dblA > 0.001f)
        {
            dl[0][(size_t) dw] = l; dl[1][(size_t) dw] = r;
            lfoPh += lfoInc; if (lfoPh > 1.f) lfoPh -= 1.f;
            const int N = (int) dl[0].size();
            auto rd = [&] (int c, float ms) { float d = ms * 0.001f * (float) sr; float p = (float) dw - d; while (p < 0) p += (float) N;
                int i0 = (int) p; float f = p - (float) i0; return dl[c][(size_t) (i0 % N)] * (1 - f) + dl[c][(size_t) ((i0 + 1) % N)] * f; };
            float s1 = std::sin (pi2 * lfoPh), s2 = std::sin (pi2 * (lfoPh + 0.5f)), s3 = std::sin (pi2 * lfoPh * 1.7f);
            float chL = rd (0, 13.f + 4.f * s1), chR = rd (1, 13.f + 4.f * s2);
            float dbL = rd (0, 21.f + 1.5f * s3), dbR = rd (1, 31.f - 1.5f * s3);
            wl += chL * chorusA * 0.6f + (dbL * 0.9f + dbR * 0.2f) * dblA * 0.55f;
            wr += chR * chorusA * 0.6f + (dbR * 0.9f + dbL * 0.2f) * dblA * 0.55f;
            dw = (dw + 1) % N;
        }
        l = wl; r = wr;

        // width (mid/side)
        { float m = 0.5f * (l + r), s = 0.5f * (l - r); s *= width; l = m + s; r = m - s; }

        // ping-pong echo
        if (echoA > 0.001f)
        {
            const int N = (int) echo[0].size();
            int rp = (ew - echoSamples + N) % N;
            float eL = echo[0][(size_t) rp], eR2 = echo[1][(size_t) rp];
            echo[0][(size_t) ew] = 0.5f * (l + r) * 0.8f + eR2 * echoFb;
            echo[1][(size_t) ew] = eL * echoFb;
            l += eL * echoMix; r += eR2 * echoMix;
            ew = (ew + 1) % N;
        }
        L[i] = l; R[i] = r;
    }

    // reverb
    if (revA > 0.001f)
    {
        juce::Reverb::Parameters rp; rp.roomSize = 0.55f + 0.3f * revA; rp.damping = 0.55f; rp.wetLevel = revA * 0.3f * (0.5f + 0.5f * dose);
        rp.dryLevel = 1.f; rp.width = 1.f; reverb.setParameters (rp);
        reverb.processStereo (L, R, n);
    }

    // volume + A/B mix + meters
    for (int i = 0; i < n; ++i)
    {
        float v = volSm.getNextValue(), m = mixSm.getNextValue();
        auto soft = [] (float x) { const float th = 0.89f, a = std::abs (x); return a <= th ? x : std::copysign (th + 0.1f * std::tanh ((a - th) / 0.1f), x); };
        L[i] = (dry.getSample (0, i) * (1.f - m) + soft (L[i]) * m) * (after ? v : 1.f);
        R[i] = (dry.getSample (1, i) * (1.f - m) + soft (R[i]) * m) * (after ? v : 1.f);
        outPk = juce::jmax (outPk, std::abs (L[i]), std::abs (R[i]));
    }
    inLevel = juce::jmax (inPk, inLevel.load() * 0.92f); outLevel = juce::jmax (outPk, outLevel.load() * 0.92f);
    grDb = juce::jmax (grMax, grDb.load() * 0.9f);
    detectedHz = tuneA > 0.001f ? det.f0 : 0.f;
}

// ============================================================ prompt engine
void VoxPillProcessor::set (const char* id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}
float VoxPillProcessor::nudge (const char* id, float d)
{
    auto* p = apvts.getParameter (id);
    auto r = p->getNormalisableRange();
    float v = juce::jlimit (r.start, r.end, get (id) + d * (r.end - r.start));
    set (id, v);
    return v;
}

struct PresetDef { const char* name; float tune, speed, deess, comp, sat, air, tone, width, chorus, dbl, rev, echo, gate, dose; };
static const PresetDef kPresets[] = {
    // name           tune speed deess comp  sat  air  tone  width chor  dbl   rev  echo gate dose
    { "clean pop",    0.0f, 0.5f, 0.45f, 0.45f, 0.10f, 0.40f,  0.15f, 1.15f, 0.00f, 0.10f, 0.12f, 0.00f, 0.00f, 0.8f },
    { "melodic trap", 0.8f, 0.9f, 0.40f, 0.55f, 0.20f, 0.35f,  0.00f, 1.35f, 0.10f, 0.35f, 0.18f, 0.22f, 0.10f, 0.9f },
    { "dark rnb",     0.2f, 0.4f, 0.35f, 0.50f, 0.18f, 0.12f, -0.45f, 1.30f, 0.25f, 0.25f, 0.28f, 0.12f, 0.00f, 0.8f },
    { "airy ballad",  0.1f, 0.3f, 0.50f, 0.40f, 0.05f, 0.75f,  0.30f, 1.25f, 0.20f, 0.15f, 0.40f, 0.10f, 0.00f, 0.7f },
    { "hyperpop",     1.0f, 1.0f, 0.55f, 0.75f, 0.40f, 0.85f,  0.40f, 1.70f, 0.45f, 0.40f, 0.20f, 0.15f, 0.00f, 1.0f },
    { "lofi tape",    0.0f, 0.5f, 0.20f, 0.60f, 0.55f, 0.00f, -0.70f, 0.80f, 0.35f, 0.00f, 0.22f, 0.10f, 0.00f, 0.9f },
    { "radio talk",   0.0f, 0.5f, 0.55f, 0.80f, 0.15f, 0.20f,  0.10f, 0.60f, 0.00f, 0.00f, 0.00f, 0.00f, 0.35f, 0.9f },
    { "rage",         0.5f, 0.8f, 0.35f, 0.85f, 0.65f, 0.30f,  0.10f, 1.45f, 0.00f, 0.50f, 0.10f, 0.18f, 0.20f, 1.0f },
    { "drill",        0.3f, 0.7f, 0.50f, 0.70f, 0.30f, 0.25f, -0.15f, 1.05f, 0.00f, 0.20f, 0.20f, 0.00f, 0.25f, 0.9f },
};
juce::StringArray VoxPillProcessor::presetNames()
{ juce::StringArray s; for (auto& p : kPresets) s.add (p.name); return s; }

void VoxPillProcessor::applyPreset (const juce::String& name)
{
    for (auto& p : kPresets)
        if (name.equalsIgnoreCase (p.name))
        {
            set ("tune", p.tune); set ("tuneSpeed", p.speed); set ("deess", p.deess); set ("comp", p.comp); set ("sat", p.sat);
            set ("air", p.air); set ("tone", p.tone); set ("width", p.width); set ("chorus", p.chorus); set ("dbl", p.dbl);
            set ("reverb", p.rev); set ("echo", p.echo); set ("gate", p.gate); set ("dose", p.dose); set ("after", 1.f);
            return;
        }
}
void VoxPillProcessor::resetAll()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID != "key" && rp->paramID != "scale") rp->setValueNotifyingHost (rp->getDefaultValue());
}

juce::String VoxPillProcessor::runPrompt (const juce::String& raw)
{
    auto finish = [&] (const juce::String& reply) { lastReply = reply; history.add ("> " + raw); history.add (reply);
        while (history.size() > 30) history.remove (0); return reply; };
    juce::String whole = " " + raw.toLowerCase().replace (",", " ").replace (".", " ").replace ("-", " ") + " ";
    auto hasIn = [] (const juce::String& t, std::initializer_list<const char*> ws) { for (auto w : ws) if (t.contains (w)) return true; return false; };

    if (hasIn (whole, { " reset", "start over", "clean slate" })) { resetAll(); return finish ("DOSE: wiped. fresh chain."); }
    for (auto& p : kPresets)
    {
        juce::String n (p.name);
        if (whole.contains (n) || (n.contains ("trap") && whole.contains (" trap")) || (n.contains ("rnb") && hasIn (whole, { " r&b", " rnb" }))
            || (n.contains ("lofi") && hasIn (whole, { "lo fi", "lofi" })) || (n.contains ("hyper") && whole.contains ("hyper")))
        { applyPreset (p.name); return finish (juce::String ("DOSE: loaded '") + p.name + "' chain."); }
    }

    juce::StringArray changed;
    auto touch = [&] (const char* id) { if (! changed.contains (id)) changed.add (id); };

    // split into clauses so "less harsh, more air" works per part
    juce::StringArray rawClauses; rawClauses.addTokens (raw.toLowerCase().replace (".", "|").replace (",", "|").replace (" and ", "|").replace (" but ", "|").replace (" then ", "|").replace (" with ", "|").replace (" plus ", "|"), "|", "");
    for (auto& rc : rawClauses)
    {
        juce::String t = " " + rc.replace ("-", " ").trim() + " ";
        if (t.trim().isEmpty()) continue;
        auto has = [&] (std::initializer_list<const char*> ws) { return hasIn (t, ws); };

        float mag = 1.f;
        if (has ({ " bit ", "slight", "little", "subtle", "touch" })) mag = 0.5f;
        if (has ({ " very ", " way ", " lot ", " lots ", "much", "super", "heavy", "hard ", "extreme", "insane", "crazy", " more " })) mag = 2.f;
        if (has ({ " more " }) && ! has ({ " very ", " way ", " lot ", " lots ", "super", "extreme" })) mag = 1.f;
        const bool negate = has ({ " less ", " reduce", " lower", " not so", " too much", " tone down", " dial back", " down " });
        const bool kill = has ({ " no ", " remove", " without", " kill", " off ", " none", " turn off", " get rid" });
        const float sg = (negate ? -1.f : 1.f) * mag;
        auto amt = [&] (const char* id, float d, bool killZeros = true)
        {
            if (kill && killZeros) set (id, juce::String (id) == "width" ? 1.f : 0.f); else nudge (id, d * (kill ? 1.f : sg)); touch (id);
        };

        // key / scale
        for (int k = 11; k >= 0; --k)
        {
            juce::String nn = juce::String (kNotes[k]).toLowerCase();
            bool done = false;
            for (auto sc : { "min", "maj" })
                if (t.contains (" " + nn + sc) || t.contains (" " + nn + " " + sc))
                { set ("key", (float) k); set ("scale", juce::String (sc) == "min" ? 2.f : 1.f); touch ("key"); touch ("scale");
                  if (get ("tune") < 0.3f) { set ("tune", 0.7f); touch ("tune"); } done = true; break; }
            if (done) break;
        }
        if (has ({ "autotune", "auto tune", " tune", "pitch", "in key", "robot", "t pain", "tpain", "off key", "pitchy" }))
        {
            if (kill || (negate && ! has ({ "off key", "pitchy" }))) { set ("tune", 0.f); touch ("tune"); }
            else
            {
                set ("tune", juce::jlimit (0.f, 1.f, get ("tune") + 0.5f * mag)); touch ("tune");
                if (has ({ "robot", "t pain", "tpain", "hard", "autotune", "auto tune", "snap" })) { set ("tuneSpeed", 1.f); touch ("tuneSpeed"); }
                else if (has ({ "natural", "subtle", "transparent", "gentle" })) { set ("tuneSpeed", 0.3f); touch ("tuneSpeed"); }
            }
        }
        if (has ({ "bright", "crisp", " air", "shine", "shimmer", "open", "sparkle", "dull", "muffled" })) { amt ("air", 0.2f, false); nudge ("tone", 0.08f * sg); touch ("tone"); }
        if (has ({ "dark", "mellow", "smooth", "warm", "moody" }))
        { nudge ("tone", -0.14f * sg); touch ("tone"); nudge ("air", -0.1f * sg); touch ("air"); if (has ({ "warm" })) amt ("sat", 0.1f, false); }
        if (has ({ "wide", "stereo", "huge", "big", "spacious" })) { nudge ("width", (negate ? -0.2f : 0.2f) * mag); touch ("width"); }
        if (has ({ "narrow", "mono", "center", "centre" })) { nudge ("width", -0.2f); touch ("width"); }
        if (has ({ "double", "stack", "layer", "fat", "thick", "ad lib", "adlib" })) amt ("dbl", 0.25f);
        if (has ({ "chorus", "lush", "dreamy" })) amt ("chorus", 0.25f);
        if (has ({ "reverb", "space", "hall", "ethereal", "atmos", " wet", "cathedral", "dreamy" })) amt ("reverb", 0.2f);
        if (has ({ " dry", "tight", "upfront", "up front", "in your face", "close" })) { set ("reverb", 0.f); set ("echo", 0.f); touch ("reverb"); touch ("echo"); }
        if (has ({ "echo", "delay", "slap", "throw", "repeat" })) amt ("echo", 0.2f);
        if (has ({ "compress", "punch", "even", "consistent", "steady", "loud", "control", "level", "squash", "glue" })) amt ("comp", 0.25f);
        if (has ({ "harsh", "sibil", " ess", "s's", "piercing", "sharp", "hiss", "spit" })) amt ("deess", 0.3f, false);
        if (has ({ "dirty", "gritty", "grit", "distort", "crunch", "saturat", "hot ", "aggressive", "analog", "tape" })) amt ("sat", 0.25f);
        if (has ({ "noise", "quiet", "silent", "gate", "background", "bleed", "clean up", "cleanup" })) amt ("gate", 0.25f, false);
        if (has ({ "louder", "volume up", "boost" })) { nudge ("volume", 0.1f); touch ("volume"); }
        if (has ({ "quieter", "softer", "volume down" })) { nudge ("volume", -0.1f); touch ("volume"); }
        if (has ({ "more dose", "stronger", "intense" })) { nudge ("dose", 0.2f); touch ("dose"); }
        if (has ({ "less dose", "weaker" })) { nudge ("dose", -0.2f); touch ("dose"); }
    }

    if (changed.isEmpty())
        return finish ("DOSE: didn't catch that. try 'darker and wider', 'hard tune in f minor' or a style like 'melodic trap'.");
    set ("after", 1.f);
    juce::String reply = "DOSE: done.";
    for (auto& id : changed)
        if (auto* p = apvts.getParameter (id))
            reply << "  " << p->getName (20).toLowerCase() << " " << p->getCurrentValueAsText().toUpperCase();
    return finish (reply);
}

void VoxPillProcessor::getStateInformation (juce::MemoryBlock& d)
{ if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d); }
void VoxPillProcessor::setStateInformation (const void* data, int size)
{ if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml)); }

juce::AudioProcessorEditor* VoxPillProcessor::createEditor() { return new VoxPillEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxPillProcessor(); }

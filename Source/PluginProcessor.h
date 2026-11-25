#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ---------------------------------------------------------------- pitch detect
class PitchDetector
{
public:
    void prepare (double sr) { sampleRate = sr; buf.assign (2048, 0.f); diff.assign (1024, 0.f); pos = 0; }
    // push mono samples; returns last detected f0 (0 = unvoiced)
    void push (float s)
    {
        buf[(size_t) pos] = s;
        pos = (pos + 1) % (int) buf.size();
        if (++counter >= 512) { counter = 0; analyse(); }
    }
    float f0 = 0.f;
private:
    void analyse()
    {
        const int N = (int) buf.size(), W = 1024;
        const int minLag = (int) (sampleRate / 800.0), maxLag = juce::jmin (W - 1, (int) (sampleRate / 70.0));
        std::vector<float> w ((size_t) N);
        for (int i = 0; i < N; ++i) w[(size_t) i] = buf[(size_t) ((pos + i) % N)];
        double energy = 0; for (int i = N - W; i < N; ++i) energy += w[(size_t) i] * w[(size_t) i];
        if (energy / W < 1e-6) { f0 = 0; return; }
        diff[0] = 1;
        double run = 0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double d = 0;
            for (int i = 0; i < W; ++i) { double x = w[(size_t) (N - W + i)] - w[(size_t) (N - W + i - tau)]; d += x * x; }
            run += d;
            diff[(size_t) tau] = (float) (d * tau / (run + 1e-12));
        }
        int best = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0) { f0 = 0; return; }
        float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        float den = a - 2 * b + c, off = std::abs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.f;
        f0 = (float) (sampleRate / (best + off));
    }
    double sampleRate = 44100; std::vector<float> buf, diff; int pos = 0, counter = 0;
};

// ---------------------------------------------------------------- pitch shifter
class PitchShifter
{
public:
    void prepare (double sr)
    {
        size = (int) (sr * 0.2); line.assign ((size_t) size, 0.f); w = 0; phase = 0;
        grain = (float) (sr * 0.040);
    }
    float process (float in, float ratio)
    {
        line[(size_t) w] = in; w = (w + 1) % size;
        phase += (1.f - ratio) / grain; phase -= std::floor (phase);
        float p2 = phase + 0.5f; p2 -= std::floor (p2);
        auto tap = [&] (float p) { float d = p * grain + 2.f; float r = (float) w - d; while (r < 0) r += (float) size;
            int i = (int) r; float f = r - (float) i; return line[(size_t) i % (size_t) size] * (1 - f) + line[(size_t) (i + 1) % (size_t) size] * f; };
        float g1 = std::sin (juce::MathConstants<float>::pi * phase), g2 = std::sin (juce::MathConstants<float>::pi * p2);
        return tap (phase) * g1 * g1 + tap (p2) * g2 * g2;
    }
private:
    std::vector<float> line; int size = 1, w = 0; float phase = 0, grain = 1;
};

// ---------------------------------------------------------------- processor
class VoxPillProcessor : public juce::AudioProcessor
{
public:
    VoxPillProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VoxPill"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "STATE", layout() };

    // Prompt interpreter: returns the "dose" reply line shown on the CRT
    juce::String runPrompt (const juce::String& text);
    void applyPreset (const juce::String& name);
    void resetAll();
    static juce::StringArray presetNames();

    std::atomic<float> inLevel { 0 }, outLevel { 0 }, grDb { 0 }, detectedHz { 0 };
    juce::String lastReply { "yeah i hear you. say the word." };
    juce::StringArray history;

private:
    void set (const char* id, float v);
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float nudge (const char* id, float d);

    double sr = 44100;
    // gate/comp/deess state
    float gateEnv = 0, gateGain = 1, compEnv = 0, deessEnv = 0, smoothGr = 0;
    // per-channel filters
    using Filt = juce::dsp::IIR::Filter<float>;
    Filt hp[2], lowMid[2], presence[2], air[2], tilt[2], essBand[2], essCut[2], toneLP[2];
    PitchDetector det; PitchShifter shifter[2];
    float curRatio = 1.f;
    // doubler / chorus
    std::vector<float> dl[2]; int dw = 0; float lfoPh = 0;
    // delay
    std::vector<float> echo[2]; int ew = 0;
    juce::Reverb reverb;
    juce::LinearSmoothedValue<float> volSm, mixSm;
    juce::AudioBuffer<float> dry;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> latencyDelay { 8192 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxPillProcessor)
};

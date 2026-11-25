#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

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

    std::atomic<float> inLevel { 0 }, outLevel { 0 }, grDb { 0 };
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

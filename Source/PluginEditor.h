#pragma once
#include "PluginProcessor.h"

class VoxPillEditor : public juce::AudioProcessorEditor
{
public:
    explicit VoxPillEditor (VoxPillProcessor&);
    ~VoxPillEditor() override;
    void resized() override;
private:
    struct Content;
    std::unique_ptr<Content> content;
    static constexpr int baseW = 760, baseH = 640;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxPillEditor)
};

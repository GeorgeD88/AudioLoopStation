#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** VU meter with 30fps update rate and peak decay */
class VUMeterComponent final : public juce::Component,
                               public juce::Timer
{
public:
    using LevelSupplier = std::function<float()>;

    explicit VUMeterComponent(LevelSupplier levelSupplier);
    ~VUMeterComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LevelSupplier getLevel;
    float currentLevel{0.0f};
    float peakLevel{0.0f};
    static constexpr int timerHz = 30;
    static constexpr float peakDecayPerFrame = 0.92f;  // ~-12dB/sec at 30fps

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMeterComponent)
};

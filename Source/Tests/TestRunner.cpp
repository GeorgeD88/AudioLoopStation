#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    juce::UnitTestRunner runner;
    runner.runAllTests();

    return 0;
}

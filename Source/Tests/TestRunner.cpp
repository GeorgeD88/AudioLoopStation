#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    juce::UnitTestRunner runner;
    // Log each test's start and end to the console
    runner.setPassesAreLogged(true);

    runner.runAllTests();

    // Loop through the results and print them to the CLion console
    std::cout << "\n--- TEST RESULTS ---" << std::endl;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult(i);
        std::cout << "Test: " << result->unitTestName << " | Passes: " << result->passes
                  << " | Failures: " << result->failures << std::endl;

        if (result->failures > 0)
        {
            for (auto& message : result->messages)
                std::cout << "   -> " << message << std::endl;
        }
    }
    std::cout << "--------------------\n" << std::endl;

    // Return a non-zero exit code if any tests failed
    for (int i = 0; i < runner.getNumResults(); ++i) {
        if (runner.getResult(i)->failures > 0) return 1;
    }

    return 0;
}
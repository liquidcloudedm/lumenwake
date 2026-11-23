#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

// Usage: lw_hosttest path/to/Lumenwake.vst3 [--record seconds [parameter=value ...]]
// where a parameter is its index or its name ("FX Strobe On=0") and the value is 0 to 1.
// Loads the plugin as a VST3 host would and checks the basics.
//
// With --record it also plays the part of the host transport: stopped, then
// playing for the given time, then stopped again, feeding a drum loop at real
// speed. Run it with LUMENWAKE_AUTOREC=1 and LUMENWAKE_REC_DIR=<folder> set and
// the plugin should leave exactly one video there, as long as the playing part.
namespace
{
    double testRate = 48000.0;      // set LW_TEST_RATE to try another sample rate

    struct TestPlayHead : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing.load());
            info.setBpm (120.0);
            info.setTimeInSamples (samples.load());
            info.setTimeInSeconds ((double) samples.load() / testRate);
            info.setPpqPosition ((double) samples.load() / testRate * 120.0 / 60.0);
            return info;
        }

        std::atomic<bool> playing { false };
        std::atomic<juce::int64> samples { 0 };
    };

    // A 120 BPM kick, off-beat hats and a held chord.
    void fillDrumLoop (juce::AudioBuffer<float>& buffer, juce::int64 position, juce::Random& random)
    {
        const double twoPi = juce::MathConstants<double>::twoPi;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double t = (double) (position + i) / testRate;
            const double tb = std::fmod (t, 0.5), th = std::fmod (t + 0.25, 0.25);
            double s = std::sin (twoPi * (50.0 * tb + 3.0 * (1.0 - std::exp (-tb * 30.0)))) * std::exp (-tb * 9.0) * 0.8;
            s += (random.nextDouble() * 2.0 - 1.0) * std::exp (-th * 40.0) * 0.18;
            s += 0.10 * std::sin (twoPi * 330.0 * t) + 0.07 * std::sin (twoPi * 495.0 * t);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, (float) s);
        }
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: lw_hosttest <plugin.vst3>" << std::endl;
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    int failures = 0;

    const auto rateSetting = juce::SystemStats::getEnvironmentVariable ("LW_TEST_RATE", {});

    if (rateSetting.getDoubleValue() >= 8000.0)
        testRate = rateSetting.getDoubleValue();
    auto check = [&] (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
        if (! ok) ++failures;
    };

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, juce::String::fromUTF8 (argv[1]));
    check (! found.isEmpty(), "plugin is recognised as a VST3");

    if (found.isEmpty())
        return 1;

    std::cout << "        " << found[0]->name << " by " << found[0]->manufacturerName << ", category " << found[0]->category
              << std::endl;

    juce::String error;
    auto instance = format.createInstanceFromDescription (*found[0], testRate, 512, error);
    check (instance != nullptr, "plugin instantiates" + (error.isNotEmpty() ? " (" + error + ")" : juce::String()));

    if (instance == nullptr)
        return 1;

    instance->prepareToPlay (testRate, 512);
    check (instance->getTotalNumInputChannels() == 2 && instance->getTotalNumOutputChannels() == 2,
           "stereo in, stereo out: " + juce::String (instance->getTotalNumInputChannels()) + " in / "
               + juce::String (instance->getTotalNumOutputChannels()) + " out");

    // Audio must pass through bit for bit.
    juce::AudioBuffer<float> buffer (juce::jmax (2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()), 512);
    juce::AudioBuffer<float> reference;
    juce::MidiBuffer midi;
    bool identical = true;

    for (int block = 0; block < 200; ++block)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.5f * std::sin ((float) (block * 512 + i) * (0.01f + 0.003f * (float) ch)));

        reference.makeCopyOf (buffer);
        instance->processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                identical = identical && buffer.getSample (ch, i) == reference.getSample (ch, i);
    }

    check (identical, "audio passes through unchanged (200 blocks)");

    const auto& params = instance->getParameters();
    check (params.size() >= 17, "parameters exposed for automation: " + juce::String (params.size()));

    // The effect switches: one for all of them and one each. A host shows a
    // two-step parameter as an on/off switch in its automation.
    {
        int switches = 0, onByDefault = 0;
        juce::AudioProcessorParameter* allOn = nullptr;

        for (auto* p : params)
        {
            const auto name = p->getName (64);

            if (! name.startsWith ("FX ") || ! name.endsWith (" On") || p->getNumSteps() != 2 || ! p->isAutomatable())
                continue;

            ++switches;
            onByDefault += p->getDefaultValue() >= 0.5f ? 1 : 0;

            if (name == "FX On")
                allOn = p;
        }

        check (switches == 11 && allOn != nullptr, "effect on/off switches exposed for automation: " + juce::String (switches));
        check (onByDefault == switches, "effect switches are on by default");

        if (allOn != nullptr)
        {
            allOn->setValueNotifyingHost (0.0f);
            const auto offText = allOn->getCurrentValueAsText();
            const bool wentOff = allOn->getValue() < 0.5f;
            allOn->setValueNotifyingHost (1.0f);
            check (wentOff && allOn->getValue() >= 0.5f, "FX On switches off and on again (reads '" + offText + "' when off)");
        }
    }

    // State round trip
    juce::MemoryBlock state;
    params[4]->setValueNotifyingHost (0.123f);
    instance->getStateInformation (state);
    params[4]->setValueNotifyingHost (0.9f);
    instance->setStateInformation (state.getData(), (int) state.getSize());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    check (std::abs (params[4]->getValue() - 0.123f) < 0.01f, "state saves and restores (" + params[4]->getName (64) + ")");

    // Editor
    if (auto* editor = instance->createEditorIfNeeded())
    {
        editor->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
        editor->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (2500);
        check (editor->getWidth() >= 400 && editor->getHeight() >= 300,
               "editor opens: " + juce::String (editor->getWidth()) + " x " + juce::String (editor->getHeight()));

        for (int block = 0; block < 50; ++block)
            instance->processBlock (buffer, midi);

        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        delete editor;
        check (true, "editor closes cleanly");
    }
    else
    {
        check (false, "editor opens");
    }

    //==========================================================================
    if (argc >= 4 && juce::String (argv[2]) == "--record")
    {
        const double seconds = juce::String (argv[3]).getDoubleValue();
        const juce::File folder (juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_REC_DIR", {}));
        const int before = folder.getNumberOfChildFiles (juce::File::findFiles, "*.mov");

        for (int i = 4; i < argc; ++i)
        {
            const auto setting = juce::String (argv[i]);
            const auto key = setting.upToFirstOccurrenceOf ("=", false, false);
            int index = key.containsOnly ("0123456789") ? key.getIntValue() : -1;

            // A parameter can be given by its name too, as the host shows it.
            for (int p = 0; index < 0 && p < params.size(); ++p)
                if (params[p]->getName (64) == key)
                    index = p;

            if (juce::isPositiveAndBelow (index, params.size()))
            {
                params[index]->setValueNotifyingHost (setting.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
                std::cout << "        set " << index << " " << params[index]->getName (64) << " = "
                          << params[index]->getCurrentValueAsText() << std::endl;
            }
            else
            {
                check (false, "no parameter called " + key);
            }
        }

        TestPlayHead head;
        instance->setPlayHead (&head);
        juce::Random random (7);

        // Feeds audio at real speed while the message loop (and so the picture) runs.
        auto run = [&] (double duration)
        {
            const double start = juce::Time::getMillisecondCounterHiRes();
            juce::int64 done = 0;

            for (;;)
            {
                const double elapsed = (juce::Time::getMillisecondCounterHiRes() - start) * 0.001;

                while ((double) done / testRate < juce::jmin (elapsed, duration))
                {
                    fillDrumLoop (buffer, head.samples.load(), random);
                    instance->processBlock (buffer, midi);
                    done += 512;

                    if (head.playing.load())
                        head.samples += 512;
                }

                if (elapsed >= duration)
                    break;

                juce::MessageManager::getInstance()->runDispatchLoopUntil (4);
            }
        };

        if (auto* editor = instance->createEditorIfNeeded())
        {
            editor->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
            editor->setVisible (true);

            run (1.5);      // armed, transport stopped: nothing may be written yet
            check (folder.getNumberOfChildFiles (juce::File::findFiles, "*.mov") == before, "no file while the transport is stopped");

            head.playing = true;
            run (seconds);
            head.playing = false;
            run (2.5);      // transport stopped: the plugin ends and saves the take by itself

            check (folder.getNumberOfChildFiles (juce::File::findFiles, "*.mov") == before + 1,
                   "one video written after play then stop");

            delete editor;
        }

        instance->setPlayHead (nullptr);
    }

    instance->releaseResources();
    instance.reset();
    check (true, "plugin unloads cleanly");

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + juce::String (failures)) << std::endl;
    return failures;
}

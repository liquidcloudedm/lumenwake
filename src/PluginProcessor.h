#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioAnalyzer.h"
#include "ModelLoader.h"
#include "Recorder.h"

enum Scene
{
    sceneImage = 0,
    sceneModel,
    sceneTunnel,
    sceneInk,
    sceneKaleido,
    sceneMetal,
    sceneFractal,
    sceneSpectrum,
    numScenes
};

/** Effects laid over whatever the scene draws. Each has an amount and a rate
    parameter, and runs in time with the host tempo or a tempo of its own. */
enum Fx
{
    fxStrobe = 0,
    fxPulse,
    fxColor,
    fxZoom,
    fxShake,
    fxSwing,
    fxSplit,
    fxGlitch,
    fxGlow,
    fxPixel,
    numFx
};

/** Passes audio through untouched and hands a copy to the visuals. */
class LumenwakeProcessor : public juce::AudioProcessor
{
public:
    LumenwakeProcessor();
    ~LumenwakeProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "Lumenwake"; }
    bool acceptsMidi() const override                        { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    static const juce::StringArray sceneNames, imageModeNames, modelModeNames, fitNames;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String getImageWildcard()                   { return "*.png;*.jpg;*.jpeg;*.gif;*.webp"; }
    static bool isSupportedImage (const juce::File&);

    juce::AudioProcessorValueTreeState apvts;
    AudioFifo fifo;
    std::atomic<double> currentSampleRate { 44100.0 };

    //==========================================================================
    // Assets. The renderer polls the generation counters and re-uploads when
    // they change, so nothing here ever touches OpenGL.
    void loadImage (const juce::File& file);
    void clearImage();
    void loadModel (const juce::File& file);
    void useBuiltinModel();

    int getImageGeneration() const noexcept                  { return imageGeneration.load(); }
    int getMeshGeneration() const noexcept                   { return meshGeneration.load(); }
    juce::Image getImage() const;
    std::shared_ptr<MeshData> getMesh() const;

    juce::String getImageName() const;
    juce::String getModelName() const;
    juce::String getStatus() const;
    bool isModelLoading() const noexcept                     { return modelLoading.load(); }

    //==========================================================================
    // Effects. Parameter ids are fxIds[i] for the amount and fxIds[i] + "Rate".
    static const juce::StringArray fxNames, fxIds, fxRateNames, fxSyncNames, fxColorModeNames;
    static double fxRateBeats (int rateIndex) noexcept;

    /** Where the host transport is, as of the last audio block. */
    struct HostClock
    {
        double bpm = 0.0, ppq = 0.0, timeMs = 0.0;
        bool playing = false, hasPosition = false;
    };

    HostClock getHostClock() const noexcept;

    int panelPage = 0;

    //==========================================================================
    // Recording to a video file with a transparent background.
    static const juce::StringArray recordPresetNames;
    static VideoRecorder::Format getRecordFormat (int preset);
    static juce::File getRecordFolder();

    /** Arms a take: it starts when the host starts playing (straight away if it
        already is, or where there is no transport) and ends when playback stops. */
    bool armRecording();

    VideoRecorder recorder;
    int recordPreset = 0;

    // Remembered editor layout
    int editorWidth = 1060, editorHeight = 700;
    bool panelVisible = true;

private:
    void setStatus (const juce::String&);
    static juce::Image makePlaceholderImage();

    juce::CriticalSection assetLock;
    juce::Image image;
    std::shared_ptr<MeshData> mesh;
    juce::String imagePath, modelPath, imageName, status;
    std::atomic<int> imageGeneration { 0 }, meshGeneration { 0 }, modelLoadToken { 0 };
    std::atomic<bool> modelLoading { false };

    // Written by the audio thread, read by the render thread. The counter is
    // odd while a write is in progress, so a reader can tell a torn copy.
    std::atomic<juce::uint32> clockSequence { 0 };
    std::atomic<double> clockBpm { 0.0 }, clockPpq { 0.0 }, clockTimeMs { 0.0 };
    std::atomic<bool> clockPlaying { false }, clockHasPosition { false };

    juce::ThreadPool loaderPool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenwakeProcessor)
};

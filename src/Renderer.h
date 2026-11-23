#pragma once

#include <juce_opengl/juce_opengl.h>

#include "PluginProcessor.h"

/** Owns the OpenGL context and draws every frame:
    scene -> accumulate (trails) -> grade -> screen. */
class VisualRenderer : public juce::OpenGLRenderer
{
public:
    VisualRenderer (LumenwakeProcessor&, juce::Component& target);
    ~VisualRenderer() override;

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void setViewSize (int width, int height) noexcept   { viewWidth = width; viewHeight = height; }
    float getFps() const noexcept                       { return fps.load(); }
    juce::String getShaderError() const;

private:
    enum ProgramId
    {
        progImage = 0, progTunnel, progInk, progKaleido, progMetal, progFractal, progSpectrum,
        progModelBackground, progMesh, progDust, progAccumulate, progPost, numPrograms
    };

    struct Program
    {
        juce::uint32 id = 0;
        bool attempted = false;
    };

    struct Target
    {
        juce::uint32 fbo = 0, texture = 0, depth = 0;
        int width = 0, height = 0;
    };

    struct Frame;

    Program& useProgram (ProgramId);
    bool buildProgram (Program&, ProgramId);
    void createTarget (Target&, int width, int height, bool withDepth);
    void destroyTarget (Target&);
    void ensureTargets (int width, int height);
    void clearAccumulation();
    void syncAssets();
    void uploadImage (const juce::Image&);
    void uploadMesh (const MeshData&);
    void setCommonUniforms (juce::uint32 program, const Frame&);
    void drawFullscreen();
    void drawModel (const Frame&, bool transparentBackground);
    void ensureCapture (int width, int height);
    void destroyCapture();
    void collectCapture();
    void updateEffects (Frame&, float dt);
    void feedSyntheticAudio (float dt);
    void stepDump (int width, int height, juce::uint32 framebuffer);
    void writeScreenshot (const juce::File&, int width, int height, juce::uint32 framebuffer);

    LumenwakeProcessor& processor;
    juce::Component& target;
    juce::OpenGLContext context;

    std::atomic<float>* pScene = nullptr; std::atomic<float>* pImageMode = nullptr;
    std::atomic<float>* pModelMode = nullptr; std::atomic<float>* pFit = nullptr;
    std::atomic<float>* pSensitivity = nullptr; std::atomic<float>* pSmoothing = nullptr;
    std::atomic<float>* pAmount = nullptr; std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pZoom = nullptr; std::atomic<float>* pTrails = nullptr;
    std::atomic<float>* pGlow = nullptr; std::atomic<float>* pHue = nullptr;
    std::atomic<float>* pSaturation = nullptr; std::atomic<float>* pBrightness = nullptr;
    std::atomic<float>* pTurn = nullptr; std::atomic<float>* pTilt = nullptr;
    std::atomic<float>* pSpin = nullptr;
    std::atomic<float>* pFxSync = nullptr; std::atomic<float>* pFxBpm = nullptr;
    std::atomic<float>* pFxColorMode = nullptr;
    std::atomic<float>* pFx[numFx] = {};
    std::atomic<float>* pFxRate[numFx] = {};
    double fxBeats = 0.0;

    std::atomic<int> viewWidth { 0 }, viewHeight { 0 };
    std::atomic<float> fps { 0.0f };

    AudioAnalyzer analyzer;

    Program programs[numPrograms];
    Target sceneTarget, accum[2];
    int accumIndex = 0;

    juce::uint32 emptyVao = 0, audioTexture = 0, imageTexture = 0;
    juce::uint32 meshVao = 0, meshVbo = 0, pointVao = 0, pointVbo = 0;
    int meshVertexCount = 0, pointCount = 0;
    float imageAspect = 1.0f;

    // Recording
    juce::uint32 captureFbo = 0, captureTexture = 0, capturePbo = 0;
    int captureWidth = 0, captureHeight = 0, captureIndex = -1;

    int imageGeneration = -1, meshGeneration = -1;
    int lastScene = -1, lastMode = -1;
    double lastFrameTime = 0.0;
    float time = 0.0f, sceneAge = 0.0f, spinAngle = 0.0f, fpsSmoothed = 60.0f;

    mutable juce::CriticalSection errorLock;
    juce::String shaderError;

    // Self-test: set LUMENWAKE_DUMP to a folder and every scene and mode is
    // rendered with a synthetic drum loop and written out as a PNG.
    struct DumpItem { int scene, mode, fx = -1; };
    juce::File dumpFolder, shotFile;
    std::vector<DumpItem> dumpItems;
    int dumpIndex = 0, dumpFrame = 0;
    bool dumping = false;
    AudioFifo syntheticFifo;
    double syntheticTime = 0.0;
    juce::Random syntheticRandom { 1234 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisualRenderer)
};

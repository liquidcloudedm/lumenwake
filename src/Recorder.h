#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <deque>
#include <memory>
#include <vector>

//==============================================================================
/** Writes a QuickTime .mov holding lossless video with an alpha channel
    (the "Animation" codec, 32 bits per pixel) and an optional 16-bit stereo
    sound track. Video editors read the alpha channel directly, so the clip can
    be laid over other footage.

    Pixels are four bytes each in the order alpha, red, green, blue, with
    straight (not premultiplied) alpha and the top row first. */
class MovWriter
{
public:
    MovWriter() = default;
    ~MovWriter();

    /** audioSampleRate may be 0 for a silent file. */
    bool open (const juce::File& file, int width, int height, int framesPerSecond, int audioSampleRate);

    bool writeFrame (const juce::uint8* argb);
    bool repeatFrame();
    bool writeAudio (const juce::int16* interleavedStereo, int numFrames);

    /** Writes the index. A file that was never closed cannot be played. */
    bool close();

    bool isOpen() const noexcept                    { return stream != nullptr; }
    bool hasFailed() const noexcept                 { return failed; }
    int getNumFrames() const noexcept               { return (int) videoSamples.size(); }
    juce::int64 getBytesWritten() const noexcept    { return position; }

private:
    struct VideoSample { juce::int64 offset; juce::uint32 size; bool key; };
    struct AudioChunk  { juce::int64 offset; juce::uint32 frames; };

    bool put (const void* data, size_t numBytes);
    bool addVideoSample (const juce::uint8* data, size_t numBytes, bool key);
    void writeIndex (juce::OutputStream&) const;

    std::unique_ptr<juce::FileOutputStream> stream;
    int width = 0, height = 0, fps = 30, audioRate = 0;
    juce::int64 position = 0, mediaStart = 0, audioFrames = 0;
    bool failed = false;

    std::vector<juce::uint8> previous, encoded;
    std::vector<VideoSample> videoSamples;
    std::vector<AudioChunk> audioChunks;

    JUCE_DECLARE_NON_COPYABLE (MovWriter)
};

//==============================================================================
/** Takes frames from the render thread and audio from the audio thread and
    writes them to a MovWriter on a thread of its own.

    A take is armed from the message thread. It starts on the audio thread, on
    the first block where the host transport is running, and ends when the
    transport stops or stop() is called. */
class VideoRecorder : private juce::Thread
{
public:
    enum State { idle = 0, armed, recording, finishing };

    struct Format { int width = 1920, height = 1080, fps = 30; };

    VideoRecorder();
    ~VideoRecorder() override;

    //==========================================================================
    // Message thread
    bool arm (const juce::File& fileToWrite, Format);
    void stop();

    State getState() const noexcept                 { return (State) state.load(); }
    Format getFormat() const noexcept               { return format; }
    double getSecondsRecorded() const noexcept;
    juce::int64 getBytesWritten() const noexcept    { return bytesWritten.load(); }
    int getFramesMissed() const noexcept            { return framesMissed.load(); }
    juce::String getMessage() const;
    juce::File getLastFile() const;

    //==========================================================================
    // Audio thread. followTransport is false where there is no transport to
    // follow (the standalone app); the take then starts straight away.
    void processAudio (const float* left, const float* right, int numSamples, double sampleRate,
                       bool followTransport, bool transportPlaying) noexcept;

    //==========================================================================
    // Render thread. frameDue() returns the number of the frame to capture now,
    // or -1 when the next frame is not due yet.
    int frameDue() noexcept;
    void submitFrame (int index, const juce::uint8* argb);

private:
    struct Slot { int index = 0; std::vector<juce::uint8> pixels; };

    void run() override;
    void pumpAudio (bool flush);
    void pumpFrames();
    void finishTake();
    void setMessage (const juce::String&);

    std::atomic<int> state { idle };
    Format format;
    juce::File file;

    // Audio thread -> writer thread
    juce::AbstractFifo fifo { 1 << 17 };
    std::vector<float> fifoData;
    std::atomic<juce::int64> samplesIn { 0 }, samplesLost { 0 };
    std::atomic<double> audioRate { 48000.0 }, startTime { 0.0 };
    std::atomic<bool> transportLed { false };

    // Render thread -> writer thread
    juce::CriticalSection slotLock;
    std::vector<std::unique_ptr<Slot>> freeSlots;
    std::deque<std::unique_ptr<Slot>> queue;
    std::atomic<int> lastIssued { -1 }, framesMissed { 0 };
    juce::WaitableEvent wake;

    // Writer thread
    MovWriter writer;
    std::vector<juce::int16> pcm;
    std::vector<float> scratch;
    int nextIndex = 0, decimation = 1, decimationCount = 0;
    float decimationLeft = 0.0f, decimationRight = 0.0f;
    bool takeActive = false, openFailed = false, noFrames = false, diskFull = false;
    double lastSpaceCheck = 0.0;
    std::atomic<juce::int64> bytesWritten { 0 };

    mutable juce::CriticalSection messageLock;
    juce::String message;
    juce::File lastFile;

    JUCE_DECLARE_NON_COPYABLE (VideoRecorder)
};

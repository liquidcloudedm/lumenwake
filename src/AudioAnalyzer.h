#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

/** Lock-free single-writer ring buffer. The audio thread pushes a mono mix,
    the render thread reads the most recent block. A torn read costs at most a
    few stale samples, which is invisible in a visualiser. */
class AudioFifo
{
public:
    static constexpr int size = 1 << 15;

    void push (const float* left, const float* right, int numSamples) noexcept
    {
        auto w = writePos.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            const float s = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            buffer[(size_t) w] = s;
            w = (w + 1) & (size - 1);
        }

        writePos.store (w, std::memory_order_release);
    }

    void pushMono (const float* data, int numSamples) noexcept   { push (data, nullptr, numSamples); }

    void readLatest (float* dest, int numSamples) const noexcept
    {
        const auto w = writePos.load (std::memory_order_acquire);
        auto r = (w - numSamples) & (size - 1);

        for (int i = 0; i < numSamples; ++i)
        {
            dest[i] = buffer[(size_t) r];
            r = (r + 1) & (size - 1);
        }
    }

private:
    std::array<float, size> buffer {};
    std::atomic<int> writePos { 0 };
};

//==============================================================================
/** Turns the latest audio into the handful of numbers the shaders react to. */
class AudioAnalyzer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int texWidth = 512;

    struct Features
    {
        float level = 0, bass = 0, mid = 0, high = 0;
        float beat = 0, beatSlow = 0;
        float bassAcc = 0, midAcc = 0, highAcc = 0;
        float beatCount = 0;
    };

    AudioAnalyzer()
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
    }

    const Features& getFeatures() const noexcept   { return f; }

    /** texWidth spectrum values (0..1) followed by texWidth waveform values (-1..1). */
    const float* getTextureData() const noexcept   { return texture.data(); }

    void update (const AudioFifo& fifo, double sampleRate, float dt, float sensitivity, float smoothing)
    {
        dt = juce::jlimit (0.0005f, 0.1f, dt);
        sampleRate = juce::jmax (8000.0, sampleRate);

        fifo.readLatest (block.data(), fftSize);

        double sumSq = 0.0;

        for (int i = 0; i < fftSize; ++i)
        {
            const float x = block[(size_t) i];
            sumSq += (double) x * x;
            fftData[(size_t) i] = x * window[(size_t) i];
        }

        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float rms = (float) std::sqrt (sumSq / fftSize);
        const float binHz = (float) sampleRate / (float) fftSize;
        const float magScale = 4.0f / (float) fftSize;
        const int numBins = fftSize / 2;

        auto bandEnergy = [&] (float lowHz, float highHz)
        {
            const int a = juce::jlimit (1, numBins - 1, (int) std::floor (lowHz / binHz));
            const int b = juce::jlimit (a + 1, numBins, (int) std::ceil (highHz / binHz));
            double e = 0.0;

            for (int i = a; i < b; ++i)
                e += (double) fftData[(size_t) i] * fftData[(size_t) i];

            return (float) std::sqrt (e) * magScale;
        };

        const float rawBass = bandEnergy (30.0f, 150.0f);
        const float rawMid  = bandEnergy (150.0f, 2500.0f);
        const float rawHigh = bandEnergy (2500.0f, 12000.0f);

        // Slow automatic gain so quiet and loud tracks both fill the range.
        const float peakDecay = std::exp (-dt / 7.0f);

        auto normalise = [&] (float value, float& peak, float floorLevel)
        {
            peak = juce::jmax (value, peak * peakDecay);
            const float n = value / juce::jmax (peak, floorLevel);
            return juce::jlimit (0.0f, 1.25f, n * sensitivity);
        };

        const float nLevel = normalise (rms, peakLevel, 0.02f);
        const float nBass  = normalise (rawBass, peakBass, 0.01f);
        const float nMid   = normalise (rawMid, peakMid, 0.006f);
        const float nHigh  = normalise (rawHigh, peakHigh, 0.002f);

        const float attack = 1.0f - std::exp (-dt / 0.012f);
        const float release = 1.0f - std::exp (-dt / (0.05f + 0.55f * smoothing));

        auto follow = [&] (float& env, float target)
        {
            env += (target - env) * (target > env ? attack : release);
        };

        follow (f.level, nLevel);
        follow (f.bass, nBass);
        follow (f.mid, nMid);
        follow (f.high, nHigh);

        // Beat: a bass hit that stands clear of its own recent average.
        sinceBeat += dt;
        bassAverage += (nBass - bassAverage) * (1.0f - std::exp (-dt / 0.45f));

        f.beat *= std::exp (-dt * 5.0f);
        f.beatSlow *= std::exp (-dt * 2.2f);

        if (nBass > bassAverage * 1.3f + 0.1f && nBass > 0.25f && sinceBeat > 0.16f)
        {
            f.beat = 1.0f;
            f.beatSlow = 1.0f;
            f.beatCount += 1.0f;
            sinceBeat = 0.0f;
        }

        f.bassAcc += f.bass * dt;
        f.midAcc  += f.mid * dt;
        f.highAcc += f.high * dt;

        // Keep the accumulators small enough to stay precise in float shaders.
        const float wrap = 2000.0f * juce::MathConstants<float>::pi;
        if (f.bassAcc > wrap) f.bassAcc -= wrap;
        if (f.midAcc  > wrap) f.midAcc  -= wrap;
        if (f.highAcc > wrap) f.highAcc -= wrap;

        //======================================================================
        // Spectrum row: log-spaced, tilted so highs are as visible as lows.
        const float fLow = 30.0f;
        const float fHigh = juce::jmin (16000.0f, (float) sampleRate * 0.45f);
        const float ratio = fHigh / fLow;
        const float sensDb = 20.0f * std::log10 (juce::jmax (0.05f, sensitivity));
        const float specRelease = 1.0f - std::exp (-dt / (0.04f + 0.3f * smoothing));

        for (int i = 0; i < texWidth; ++i)
        {
            const float f0 = fLow * std::pow (ratio, (float) i / (float) texWidth);
            const float f1 = fLow * std::pow (ratio, (float) (i + 1) / (float) texWidth);
            const float p0 = f0 / binHz;
            const float p1 = f1 / binHz;

            float mag;

            if (p1 - p0 < 1.0f)
            {
                const int b = juce::jlimit (0, numBins - 2, (int) p0);
                const float t = p0 - (float) b;
                mag = fftData[(size_t) b] * (1.0f - t) + fftData[(size_t) b + 1] * t;
            }
            else
            {
                mag = 0.0f;
                const int a = juce::jlimit (0, numBins - 1, (int) p0);
                const int b = juce::jlimit (a + 1, numBins, (int) std::ceil (p1));

                for (int k = a; k < b; ++k)
                    mag = juce::jmax (mag, fftData[(size_t) k]);
            }

            float db = 20.0f * std::log10 (mag * magScale + 1.0e-7f);
            db += 4.5f * std::log2 (f0 / 1000.0f) + sensDb;
            const float target = juce::jlimit (0.0f, 1.0f, (db + 66.0f) / 60.0f);

            float& v = texture[(size_t) i];
            v += (target - v) * (target > v ? 0.6f : specRelease);
        }

        //======================================================================
        // Waveform row: about 1000 samples, started on a rising zero crossing
        // so the scope stands still on steady tones.
        const int span = texWidth * 2;
        int start = fftSize - span;

        for (int i = fftSize - span; i > fftSize - span - 480; --i)
        {
            if (block[(size_t) i - 1] <= 0.0f && block[(size_t) i] > 0.0f)
            {
                start = i;
                break;
            }
        }

        const float waveGain = juce::jmin (2.0f, sensitivity) * 0.7f / juce::jmax (peakLevel * 1.6f, 0.03f);

        for (int i = 0; i < texWidth; ++i)
            texture[(size_t) (texWidth + i)] = juce::jlimit (-1.0f, 1.0f, block[(size_t) (start + i * 2)] * waveGain);
    }

private:
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> window {};
    std::array<float, fftSize> block {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, texWidth * 2> texture {};

    Features f;
    float peakLevel = 0, peakBass = 0, peakMid = 0, peakHigh = 0;
    float bassAverage = 0, sinceBeat = 1.0f;
};

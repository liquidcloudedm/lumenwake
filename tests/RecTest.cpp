#include "../src/Recorder.h"

#include <cmath>
#include <iostream>

// Usage: lw_rectest out.mov frames.raw audio.raw [width height frames fps]
//
// Writes a test clip with MovWriter and, beside it, exactly what went in: the
// frames as raw ARGB and the sound as raw 16-bit stereo. Decoding the .mov with
// another program and comparing against those two files proves the writer.
int main (int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cout << "usage: lw_rectest <out.mov> <frames.raw> <audio.raw> [width height frames fps]" << std::endl;
        return 2;
    }

    const int width  = argc > 4 ? juce::String (argv[4]).getIntValue() : 640;
    const int height = argc > 5 ? juce::String (argv[5]).getIntValue() : 360;
    const int frames = argc > 6 ? juce::String (argv[6]).getIntValue() : 75;
    const int fps    = argc > 7 ? juce::String (argv[7]).getIntValue() : 30;
    const int rate = 48000;

    auto openFresh = [] (const char* path) -> std::unique_ptr<juce::FileOutputStream>
    {
        auto s = std::make_unique<juce::FileOutputStream> (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (path)));

        if (s->failedToOpen())
            return nullptr;

        s->setPosition (0);
        s->truncate();
        return s;
    };

    auto rawFrames = openFresh (argv[2]);
    auto rawAudio = openFresh (argv[3]);

    MovWriter writer;

    if (rawFrames == nullptr || rawAudio == nullptr
        || ! writer.open (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[1])), width, height, fps, rate))
    {
        std::cout << "could not open the output files" << std::endl;
        return 1;
    }

    std::vector<juce::uint8> frame ((size_t) width * (size_t) height * 4);
    std::vector<juce::int16> sound;
    juce::Random random (42);
    juce::int64 soundPosition = 0;
    const double start = juce::Time::getMillisecondCounterHiRes();

    for (int n = 0; n < frames; ++n)
    {
        // Frames 10 to 13 repeat frame 9: two through the encoder, two as explicit repeats.
        const bool hold = n >= 10 && n <= 13;

        if (! hold)
        {
            std::fill (frame.begin(), frame.end(), (juce::uint8) 0);

            const float cx = (float) width * (0.2f + 0.6f * (float) n / (float) juce::jmax (1, frames - 1));
            const float cy = (float) height * (0.5f + 0.25f * std::sin ((float) n * 0.3f));
            const float radius = (float) height * 0.22f;

            for (int y = 0; y < height; ++y)
            {
                auto* row = frame.data() + (size_t) y * (size_t) width * 4;

                for (int x = 0; x < width; ++x)
                {
                    auto* p = row + (size_t) x * 4;

                    // A soft disc that moves: gradients, partial alpha.
                    const float d = std::sqrt (((float) x - cx) * ((float) x - cx) + ((float) y - cy) * ((float) y - cy)) / radius;

                    if (d < 1.0f)
                    {
                        p[0] = (juce::uint8) (255.0f * (1.0f - d * d));
                        p[1] = (juce::uint8) (255.0f * (1.0f - d));
                        p[2] = (juce::uint8) (40 + (x * 3) % 200);
                        p[3] = (juce::uint8) (255.0f * d);
                    }

                    // A flat bar that never changes: long repeats, then long skips.
                    if (y >= height - 24 && y < height - 8)
                    {
                        p[0] = 255; p[1] = 20; p[2] = 200; p[3] = 90;
                    }

                    // Noise that changes every frame: literal runs.
                    if (x < width / 5 && y < height / 4)
                    {
                        p[0] = (juce::uint8) random.nextInt (256);
                        p[1] = (juce::uint8) random.nextInt (256);
                        p[2] = (juce::uint8) random.nextInt (256);
                        p[3] = (juce::uint8) random.nextInt (256);
                    }

                    // A pattern that never changes, with pairs and singles mixed.
                    if (x >= width - 96 && y < 40)
                    {
                        const int k = ((x / 2) ^ (y * 7)) % 5;
                        p[0] = 200; p[1] = (juce::uint8) (k * 50); p[2] = (juce::uint8) ((x % 3) * 80); p[3] = 10;
                    }
                }
            }
        }

        if (n == 12 || n == 13)
            writer.repeatFrame();
        else
            writer.writeFrame (frame.data());

        rawFrames->write (frame.data(), frame.size());

        // Sound in uneven pieces, as it would arrive while recording.
        const int piece = (rate / fps) + (n % 3) * 37;
        sound.resize ((size_t) piece * 2);

        for (int i = 0; i < piece; ++i)
        {
            const double t = (double) (soundPosition + i) / rate;
            sound[(size_t) i * 2]     = (juce::int16) (12000.0 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * t));
            sound[(size_t) i * 2 + 1] = (juce::int16) (9000.0 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * t));
        }

        soundPosition += piece;
        writer.writeAudio (sound.data(), piece);
        rawAudio->write (sound.data(), sound.size() * sizeof (juce::int16));
    }

    const double elapsed = juce::Time::getMillisecondCounterHiRes() - start;
    const bool ok = writer.close();

    std::cout << (ok ? "wrote " : "FAILED after ") << frames << " frames, " << width << " x " << height << ", "
              << writer.getBytesWritten() << " bytes, " << juce::String (elapsed / frames, 2) << " ms per frame" << std::endl;
    return ok ? 0 : 1;
}

#include "Recorder.h"

#include <cstring>

//==============================================================================
// QuickTime Animation (RLE) encoder, 32 bits per pixel.
//
// A frame is: 4-byte size, 2-byte header (0 = every line, 8 = a band of lines
// follows as start/0/count/0), then one record per line and a closing 0.
// A line is: a skip byte (pixels to leave untouched, plus one), then codes:
//   0       another skip byte follows
//   1..127  that many literal pixels follow
//   -2..-128 the next pixel repeated that many times
//   -1      end of line
namespace
{
    inline juce::uint32 loadPixel (const juce::uint8* p) noexcept
    {
        juce::uint32 v;
        std::memcpy (&v, p, 4);
        return v;
    }

    juce::uint8* encodeLine (const juce::uint8* cur, const juce::uint8* prev, int width, juce::uint8* out) noexcept
    {
        auto pixel = [cur] (int i) noexcept { return loadPixel (cur + (size_t) i * 4); };
        auto unchanged = [cur, prev] (int i) noexcept
        {
            return prev != nullptr && loadPixel (cur + (size_t) i * 4) == loadPixel (prev + (size_t) i * 4);
        };

        int x = 0;

        while (x < width && unchanged (x))
            ++x;

        if (x == width)
        {
            *out++ = 1;
            *out++ = 0xff;
            return out;
        }

        const int lead = juce::jmin (x, 254);
        *out++ = (juce::uint8) (lead + 1);
        x = lead;

        while (x < width)
        {
            int skip = 0;

            while (x + skip < width && unchanged (x + skip))
                ++skip;

            if (skip > 0)
            {
                if (x + skip == width)
                    break;                      // nothing else on this line changed

                while (skip > 0)
                {
                    const int n = juce::jmin (skip, 254);
                    *out++ = 0;
                    *out++ = (juce::uint8) (n + 1);
                    x += n;
                    skip -= n;
                }

                continue;
            }

            const juce::uint32 value = pixel (x);
            int repeat = 1;

            while (x + repeat < width && repeat < 128 && pixel (x + repeat) == value)
                ++repeat;

            if (repeat >= 2)
            {
                *out++ = (juce::uint8) (juce::int8) -repeat;
                std::memcpy (out, cur + (size_t) x * 4, 4);
                out += 4;
                x += repeat;
                continue;
            }

            // Literal run: stop where skipping or repeating becomes cheaper.
            int n = 1;

            while (x + n < width && n < 127)
            {
                const int i = x + n;

                if (unchanged (i) && (i + 1 >= width || unchanged (i + 1)))
                    break;

                if (i + 2 < width && pixel (i) == pixel (i + 1) && pixel (i) == pixel (i + 2))
                    break;

                ++n;
            }

            *out++ = (juce::uint8) n;
            std::memcpy (out, cur + (size_t) x * 4, (size_t) n * 4);
            out += (size_t) n * 4;
            x += n;
        }

        *out++ = 0xff;
        return out;
    }

    inline juce::uint8* put16 (juce::uint8* p, int v) noexcept
    {
        p[0] = (juce::uint8) (v >> 8);
        p[1] = (juce::uint8) v;
        return p + 2;
    }

    /** prev == nullptr makes a key frame. Returns the number of bytes written. */
    size_t encodeFrame (const juce::uint8* cur, const juce::uint8* prev, int width, int height, juce::uint8* out) noexcept
    {
        const size_t stride = (size_t) width * 4;
        int first = 0, last = height;

        if (prev != nullptr)
        {
            while (first < height && std::memcmp (cur + (size_t) first * stride, prev + (size_t) first * stride, stride) == 0)
                ++first;

            while (last > first && std::memcmp (cur + (size_t) (last - 1) * stride, prev + (size_t) (last - 1) * stride, stride) == 0)
                --last;
        }

        juce::uint8* p = out + 4;

        if (first >= last)
        {
            // Nothing changed: one line that skips nothing and draws nothing.
            p = put16 (p, 8);
            p = put16 (p, 0);
            p = put16 (p, 0);
            p = put16 (p, 1);
            p = put16 (p, 0);
            *p++ = 1;
            *p++ = 0xff;
        }
        else
        {
            if (first == 0 && last == height)
            {
                p = put16 (p, 0);
            }
            else
            {
                p = put16 (p, 8);
                p = put16 (p, first);
                p = put16 (p, 0);
                p = put16 (p, last - first);
                p = put16 (p, 0);
            }

            for (int y = first; y < last; ++y)
                p = encodeLine (cur + (size_t) y * stride, prev != nullptr ? prev + (size_t) y * stride : nullptr, width, p);
        }

        *p++ = 0;

        const auto size = (juce::uint32) (p - out);
        out[0] = (juce::uint8) (size >> 24);
        out[1] = (juce::uint8) (size >> 16);
        out[2] = (juce::uint8) (size >> 8);
        out[3] = (juce::uint8) size;
        return size;
    }

    //==========================================================================
    /** A QuickTime atom: the size is filled in when the object goes out of scope. */
    struct Box
    {
        Box (juce::OutputStream& s, const char* type) : out (s), start (s.getPosition())
        {
            out.writeIntBigEndian (0);
            out.write (type, 4);
        }

        ~Box()
        {
            const auto end = out.getPosition();
            out.setPosition (start);
            out.writeIntBigEndian ((int) (end - start));
            out.setPosition (end);
        }

        juce::OutputStream& out;
        juce::int64 start;
    };

    void be32 (juce::OutputStream& o, juce::uint32 v)   { o.writeIntBigEndian ((int) v); }
    void be16 (juce::OutputStream& o, int v)            { o.writeShortBigEndian ((short) v); }
    void tag (juce::OutputStream& o, const char* t)     { o.write (t, 4); }
    void zeros (juce::OutputStream& o, int n)           { o.writeRepeatedByte (0, (size_t) n); }

    void matrix (juce::OutputStream& o)
    {
        const juce::uint32 m[9] = { 0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000 };
        for (auto v : m) be32 (o, v);
    }

    void handler (juce::OutputStream& o, const char* type, const char* subtype, const char* name)
    {
        Box b (o, "hdlr");
        be32 (o, 0);
        tag (o, type);
        tag (o, subtype);
        zeros (o, 12);
        o.writeByte ((char) std::strlen (name));
        o.write (name, std::strlen (name));
    }

    void dataReference (juce::OutputStream& o)
    {
        handler (o, "dhlr", "alis", "DataHandler");

        Box dinf (o, "dinf");
        Box dref (o, "dref");
        be32 (o, 0);
        be32 (o, 1);
        be32 (o, 12);
        tag (o, "alis");
        be32 (o, 1);            // the media is in this file
    }

    constexpr juce::uint32 movieTimescale = 600;
    constexpr int keyFrameSeconds = 1;
}

//==============================================================================
MovWriter::~MovWriter()
{
    if (isOpen())
        close();
}

bool MovWriter::put (const void* data, size_t numBytes)
{
    if (stream == nullptr || failed)
        return false;

    if (! stream->write (data, numBytes))
    {
        failed = true;
        return false;
    }

    position += (juce::int64) numBytes;
    return true;
}

bool MovWriter::open (const juce::File& file, int w, int h, int framesPerSecond, int audioSampleRate)
{
    if (isOpen() || w < 1 || h < 1 || w > 16384 || h > 16384 || framesPerSecond < 1)
        return false;

    width = w;
    height = h;
    fps = framesPerSecond;
    audioRate = audioSampleRate;
    position = 0;
    audioFrames = 0;
    failed = false;
    videoSamples.clear();
    audioChunks.clear();
    previous.clear();
    encoded.resize ((size_t) h * ((size_t) w * 4 + (size_t) w / 64 + 8) + 64);

    file.getParentDirectory().createDirectory();
    auto opened = std::make_unique<juce::FileOutputStream> (file, (size_t) 1 << 18);

    if (opened->failedToOpen())
        return false;

    opened->setPosition (0);
    opened->truncate();
    stream = std::move (opened);

    juce::MemoryOutputStream head;
    be32 (head, 20);
    tag (head, "ftyp");
    tag (head, "qt  ");
    be32 (head, 0x200);
    tag (head, "qt  ");

    // Media data with a 64-bit size, so takes over 4 GB stay valid.
    be32 (head, 1);
    tag (head, "mdat");
    head.writeInt64BigEndian (0);

    mediaStart = 20;
    return put (head.getData(), head.getDataSize());
}

bool MovWriter::addVideoSample (const juce::uint8* data, size_t numBytes, bool key)
{
    videoSamples.push_back ({ position, (juce::uint32) numBytes, key });
    return put (data, numBytes);
}

bool MovWriter::writeFrame (const juce::uint8* argb)
{
    if (! isOpen() || failed)
        return false;

    const size_t bytes = (size_t) width * (size_t) height * 4;
    const bool key = previous.empty() || (videoSamples.size() % (size_t) (fps * keyFrameSeconds)) == 0;
    const size_t size = encodeFrame (argb, key ? nullptr : previous.data(), width, height, encoded.data());

    previous.resize (bytes);
    std::memcpy (previous.data(), argb, bytes);

    return addVideoSample (encoded.data(), size, key);
}

bool MovWriter::repeatFrame()
{
    if (! isOpen() || failed || previous.empty())
        return false;

    juce::uint8 unchangedFrame[32];
    const size_t size = encodeFrame (previous.data(), previous.data(), width, 1, unchangedFrame);
    return addVideoSample (unchangedFrame, size, false);
}

bool MovWriter::writeAudio (const juce::int16* interleavedStereo, int numFrames)
{
    if (! isOpen() || failed || audioRate <= 0 || numFrames <= 0)
        return false;

    audioChunks.push_back ({ position, (juce::uint32) numFrames });
    audioFrames += numFrames;

   #if JUCE_LITTLE_ENDIAN
    return put (interleavedStereo, (size_t) numFrames * 4);
   #else
    std::vector<juce::int16> swapped (interleavedStereo, interleavedStereo + (size_t) numFrames * 2);
    for (auto& s : swapped) s = (juce::int16) juce::ByteOrder::swap ((juce::uint16) s);
    return put (swapped.data(), (size_t) numFrames * 4);
   #endif
}

bool MovWriter::close()
{
    if (! isOpen())
        return false;

    const juce::int64 mediaSize = position - mediaStart;

    juce::MemoryOutputStream index;
    writeIndex (index);
    put (index.getData(), index.getDataSize());
    stream->flush();

    if (stream->setPosition (mediaStart + 8))
    {
        if (! stream->writeInt64BigEndian (mediaSize))
            failed = true;
    }
    else
    {
        failed = true;
    }

    stream->flush();

    if (stream->getStatus().failed())
        failed = true;

    stream.reset();
    previous.clear();
    previous.shrink_to_fit();
    encoded.clear();
    encoded.shrink_to_fit();
    return ! failed;
}

void MovWriter::writeIndex (juce::OutputStream& o) const
{
    const auto numFrames = (juce::uint32) videoSamples.size();
    const bool hasAudio = audioRate > 0 && audioFrames > 0;
    const auto now = (juce::uint32) (juce::Time::currentTimeMillis() / 1000 + 2082844800LL);    // seconds since 1904

    const juce::uint32 frameTicks = 100;
    const juce::uint32 videoTimescale = (juce::uint32) fps * frameTicks;
    const auto videoDuration = (juce::uint32) ((juce::int64) numFrames * movieTimescale / fps);
    const auto audioDuration = hasAudio ? (juce::uint32) (audioFrames * movieTimescale / audioRate) : 0u;

    Box moov (o, "moov");

    {
        Box b (o, "mvhd");
        be32 (o, 0);
        be32 (o, now);
        be32 (o, now);
        be32 (o, movieTimescale);
        be32 (o, juce::jmax (videoDuration, audioDuration));
        be32 (o, 0x00010000);       // normal speed
        be16 (o, 0x0100);           // full volume
        zeros (o, 10);
        matrix (o);
        zeros (o, 24);
        be32 (o, hasAudio ? 3u : 2u);
    }

    //==========================================================================
    {
        Box trak (o, "trak");

        {
            Box b (o, "tkhd");
            be32 (o, 0x0000000f);
            be32 (o, now);
            be32 (o, now);
            be32 (o, 1);
            be32 (o, 0);
            be32 (o, videoDuration);
            zeros (o, 8);
            be16 (o, 0);
            be16 (o, 0);
            be16 (o, 0);
            be16 (o, 0);
            matrix (o);
            be32 (o, (juce::uint32) width << 16);
            be32 (o, (juce::uint32) height << 16);
        }

        Box mdia (o, "mdia");

        {
            Box b (o, "mdhd");
            be32 (o, 0);
            be32 (o, now);
            be32 (o, now);
            be32 (o, videoTimescale);
            be32 (o, numFrames * frameTicks);
            be16 (o, 0);
            be16 (o, 0);
        }

        handler (o, "mhlr", "vide", "VideoHandler");

        Box minf (o, "minf");

        {
            Box b (o, "vmhd");
            be32 (o, 1);
            be16 (o, 0x40);
            be16 (o, 0x8000);
            be16 (o, 0x8000);
            be16 (o, 0x8000);
        }

        dataReference (o);

        Box stbl (o, "stbl");

        {
            Box stsd (o, "stsd");
            be32 (o, 0);
            be32 (o, 1);

            Box entry (o, "rle ");
            zeros (o, 6);
            be16 (o, 1);                // data reference
            be16 (o, 0);                // version
            be16 (o, 0);                // revision
            tag (o, "lmwk");            // vendor
            be32 (o, 0x200);            // temporal quality
            be32 (o, 0x200);            // spatial quality
            be16 (o, width);
            be16 (o, height);
            be32 (o, 0x00480000);       // 72 dpi
            be32 (o, 0x00480000);
            be32 (o, 0);
            be16 (o, 1);                // one frame per sample

            const char name[] = "Animation";
            o.writeByte ((char) (sizeof (name) - 1));
            o.write (name, sizeof (name) - 1);
            zeros (o, 31 - (int) (sizeof (name) - 1));

            be16 (o, 32);               // depth: millions of colours plus alpha
            be16 (o, 0xffff);           // no colour table
        }

        {
            Box b (o, "stts");
            be32 (o, 0);
            be32 (o, 1);
            be32 (o, numFrames);
            be32 (o, frameTicks);
        }

        {
            Box b (o, "stss");
            be32 (o, 0);

            juce::uint32 keys = 0;
            for (auto& s : videoSamples) if (s.key) ++keys;
            be32 (o, keys);

            for (size_t i = 0; i < videoSamples.size(); ++i)
                if (videoSamples[i].key)
                    be32 (o, (juce::uint32) i + 1);
        }

        {
            Box b (o, "stsc");
            be32 (o, 0);
            be32 (o, 1);
            be32 (o, 1);
            be32 (o, 1);
            be32 (o, 1);
        }

        {
            Box b (o, "stsz");
            be32 (o, 0);
            be32 (o, 0);
            be32 (o, numFrames);
            for (auto& s : videoSamples) be32 (o, s.size);
        }

        {
            Box b (o, "co64");
            be32 (o, 0);
            be32 (o, numFrames);
            for (auto& s : videoSamples) o.writeInt64BigEndian (s.offset);
        }
    }

    //==========================================================================
    if (hasAudio)
    {
        Box trak (o, "trak");

        {
            Box b (o, "tkhd");
            be32 (o, 0x0000000f);
            be32 (o, now);
            be32 (o, now);
            be32 (o, 2);
            be32 (o, 0);
            be32 (o, audioDuration);
            zeros (o, 8);
            be16 (o, 0);
            be16 (o, 0);
            be16 (o, 0x0100);
            be16 (o, 0);
            matrix (o);
            be32 (o, 0);
            be32 (o, 0);
        }

        Box mdia (o, "mdia");

        {
            Box b (o, "mdhd");
            be32 (o, 0);
            be32 (o, now);
            be32 (o, now);
            be32 (o, (juce::uint32) audioRate);
            be32 (o, (juce::uint32) audioFrames);
            be16 (o, 0);
            be16 (o, 0);
        }

        handler (o, "mhlr", "soun", "SoundHandler");

        Box minf (o, "minf");

        {
            Box b (o, "smhd");
            be32 (o, 0);
            be16 (o, 0);
            be16 (o, 0);
        }

        dataReference (o);

        Box stbl (o, "stbl");

        {
            Box stsd (o, "stsd");
            be32 (o, 0);
            be32 (o, 1);

            Box entry (o, "sowt");      // 16-bit little-endian PCM
            zeros (o, 6);
            be16 (o, 1);
            be16 (o, 0);
            be16 (o, 0);
            be32 (o, 0);
            be16 (o, 2);                // channels
            be16 (o, 16);               // bits
            be16 (o, 0);
            be16 (o, 0);
            be16 (o, audioRate);
            be16 (o, 0);
        }

        {
            Box b (o, "stts");
            be32 (o, 0);
            be32 (o, 1);
            be32 (o, (juce::uint32) audioFrames);
            be32 (o, 1);
        }

        {
            // Runs of chunks holding the same number of sample frames.
            Box b (o, "stsc");
            be32 (o, 0);

            std::vector<std::pair<juce::uint32, juce::uint32>> runs;

            for (size_t i = 0; i < audioChunks.size(); ++i)
                if (runs.empty() || runs.back().second != audioChunks[i].frames)
                    runs.push_back ({ (juce::uint32) i + 1, audioChunks[i].frames });

            be32 (o, (juce::uint32) runs.size());

            for (auto& r : runs)
            {
                be32 (o, r.first);
                be32 (o, r.second);
                be32 (o, 1);
            }
        }

        {
            Box b (o, "stsz");
            be32 (o, 0);
            be32 (o, 4);                // every sample frame is four bytes
            be32 (o, (juce::uint32) audioFrames);
        }

        {
            Box b (o, "co64");
            be32 (o, 0);
            be32 (o, (juce::uint32) audioChunks.size());
            for (auto& c : audioChunks) o.writeInt64BigEndian (c.offset);
        }
    }
}

//==============================================================================
VideoRecorder::VideoRecorder() : juce::Thread ("Lumenwake recorder")
{
    fifoData.resize ((size_t) fifo.getTotalSize() * 2);
}

VideoRecorder::~VideoRecorder()
{
    stop();

    for (int i = 0; i < 2000 && isThreadRunning() && state.load() != idle; ++i)
        juce::Thread::sleep (5);

    signalThreadShouldExit();
    wake.signal();
    stopThread (10000);
}

bool VideoRecorder::arm (const juce::File& fileToWrite, Format f)
{
    if (state.load() != idle || f.width < 16 || f.height < 16 || f.fps < 1)
        return false;

    format = f;
    file = fileToWrite;

    {
        const juce::ScopedLock sl (slotLock);
        queue.clear();
        freeSlots.clear();

        for (int i = 0; i < 3; ++i)
        {
            auto slot = std::make_unique<Slot>();
            slot->pixels.resize ((size_t) f.width * (size_t) f.height * 4);
            freeSlots.push_back (std::move (slot));
        }
    }

    fifo.reset();
    samplesIn = 0;
    samplesLost = 0;
    lastIssued = -1;
    framesMissed = 0;
    bytesWritten = 0;
    setMessage ({});

    if (! isThreadRunning())
        startThread();

    state.store (armed);
    return true;
}

void VideoRecorder::stop()
{
    int expected = armed;

    if (state.compare_exchange_strong (expected, idle))
        return;

    expected = recording;
    state.compare_exchange_strong (expected, finishing);
    wake.signal();
}

double VideoRecorder::getSecondsRecorded() const noexcept
{
    return (double) samplesIn.load() / juce::jmax (1.0, audioRate.load());
}

juce::String VideoRecorder::getMessage() const
{
    const juce::ScopedLock sl (messageLock);
    return message;
}

juce::File VideoRecorder::getLastFile() const
{
    const juce::ScopedLock sl (messageLock);
    return lastFile;
}

void VideoRecorder::setMessage (const juce::String& m)
{
    const juce::ScopedLock sl (messageLock);
    message = m;
}

//==============================================================================
void VideoRecorder::processAudio (const float* left, const float* right, int numSamples, double sampleRate,
                                  bool followTransport, bool transportPlaying) noexcept
{
    int s = state.load();

    if (s == armed)
    {
        if (followTransport && ! transportPlaying)
            return;

        audioRate.store (sampleRate);
        transportLed.store (followTransport);
        startTime.store (juce::Time::getMillisecondCounterHiRes());

        int expected = armed;

        if (! state.compare_exchange_strong (expected, recording))
            return;

        s = recording;
    }

    if (s != recording || left == nullptr || numSamples <= 0)
        return;

    if (transportLed.load() && ! transportPlaying)
    {
        int expected = recording;
        state.compare_exchange_strong (expected, finishing);
        return;
    }

    if (right == nullptr)
        right = left;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    float* d = fifoData.data() + (size_t) start1 * 2;

    for (int i = 0; i < size1; ++i)
    {
        *d++ = left[i];
        *d++ = right[i];
    }

    d = fifoData.data() + (size_t) start2 * 2;

    for (int i = 0; i < size2; ++i)
    {
        *d++ = left[size1 + i];
        *d++ = right[size1 + i];
    }

    fifo.finishedWrite (size1 + size2);

    if (size1 + size2 < numSamples)
        samplesLost += numSamples - (size1 + size2);

    samplesIn += numSamples;
}

//==============================================================================
int VideoRecorder::frameDue() noexcept
{
    if (state.load() != recording)
        return -1;

    const double seconds = (juce::Time::getMillisecondCounterHiRes() - startTime.load()) * 0.001;
    const int index = (int) std::floor (seconds * format.fps);

    if (index <= lastIssued.load())
        return -1;

    lastIssued = index;
    return index;
}

void VideoRecorder::submitFrame (int index, const juce::uint8* argb)
{
    {
        const juce::ScopedLock sl (slotLock);

        if (state.load() != recording)
            return;

        if (freeSlots.empty())
        {
            ++framesMissed;         // the disk is not keeping up; the last frame is held instead
            return;
        }

        auto slot = std::move (freeSlots.back());
        freeSlots.pop_back();
        slot->index = index;
        std::memcpy (slot->pixels.data(), argb, slot->pixels.size());
        queue.push_back (std::move (slot));
    }

    wake.signal();
}

//==============================================================================
void VideoRecorder::run()
{
    while (! threadShouldExit())
    {
        wake.wait (4);

        const int s = state.load();

        if (s != recording && s != finishing)
            continue;

        if (! takeActive)
        {
            takeActive = true;
            decimation = audioRate.load() > 65000.0 ? (int) std::ceil (audioRate.load() / 48000.0) : 1;    // the sound track tops out at 48 kHz
            decimationCount = 0;
            decimationLeft = decimationRight = 0.0f;
            nextIndex = 0;
            pcm.clear();
        }

        pumpAudio (false);
        pumpFrames();
        bytesWritten = writer.getBytesWritten();

        if (s == recording)
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            bool end = openFailed || writer.hasFailed();

            if (! writer.isOpen() && ! openFailed && now - startTime.load() > 5000.0)
            {
                noFrames = true;    // nothing is being drawn, so there is nothing to record
                end = true;
            }

            if (writer.isOpen() && now - lastSpaceCheck > 1000.0)
            {
                lastSpaceCheck = now;

                if (file.getParentDirectory().getBytesFreeOnVolume() < (juce::int64) 300 * 1024 * 1024)
                {
                    diskFull = true;
                    end = true;
                }
            }

            if (end)
            {
                int expected = recording;
                state.compare_exchange_strong (expected, finishing);
            }
        }

        if (state.load() == finishing)
        {
            pumpFrames();
            pumpAudio (true);
            finishTake();
            state.store (idle);
        }
    }
}

void VideoRecorder::pumpAudio (bool flush)
{
    for (;;)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (8192, start1, size1, start2, size2);

        if (size1 + size2 == 0)
            break;

        auto convert = [this] (const float* src, int frames)
        {
            for (int i = 0; i < frames; ++i)
            {
                decimationLeft += src[i * 2];
                decimationRight += src[i * 2 + 1];

                if (++decimationCount >= decimation)
                {
                    const float scale = 32767.0f / (float) decimation;
                    pcm.push_back ((juce::int16) juce::jlimit (-32768, 32767, juce::roundToInt (decimationLeft * scale)));
                    pcm.push_back ((juce::int16) juce::jlimit (-32768, 32767, juce::roundToInt (decimationRight * scale)));
                    decimationLeft = decimationRight = 0.0f;
                    decimationCount = 0;
                }
            }
        };

        convert (fifoData.data() + (size_t) start1 * 2, size1);
        convert (fifoData.data() + (size_t) start2 * 2, size2);
        fifo.finishedRead (size1 + size2);
    }

    // Audio that could not be queued becomes silence, so sound and picture stay in step.
    const auto lost = samplesLost.exchange (0);

    if (lost > 0)
        pcm.insert (pcm.end(), (size_t) (lost / decimation) * 2, (juce::int16) 0);

    const size_t frames = pcm.size() / 2;
    const size_t chunk = (size_t) juce::jmax (1.0, audioRate.load() / decimation / 4.0);

    if (writer.isOpen() && frames > 0 && (flush || frames >= chunk))
    {
        writer.writeAudio (pcm.data(), (int) frames);
        pcm.clear();
    }
}

void VideoRecorder::pumpFrames()
{
    for (;;)
    {
        std::unique_ptr<Slot> slot;

        {
            const juce::ScopedLock sl (slotLock);

            if (queue.empty())
                return;

            slot = std::move (queue.front());
            queue.pop_front();
        }

        // The file is only created once there is a picture to put in it.
        if (! writer.isOpen() && ! openFailed)
            if (! writer.open (file, format.width, format.height, format.fps, juce::roundToInt (audioRate.load() / decimation)))
                openFailed = true;

        if (writer.isOpen() && slot->index >= nextIndex)
        {
            // Slots nothing was drawn for hold the picture before them.
            if (writer.getNumFrames() > 0)
            {
                while (nextIndex < slot->index)
                {
                    writer.repeatFrame();
                    ++nextIndex;
                }
            }

            writer.writeFrame (slot->pixels.data());
            ++nextIndex;

            while (nextIndex <= slot->index)    // only when the very first picture arrived late
            {
                writer.repeatFrame();
                ++nextIndex;
            }
        }

        const juce::ScopedLock sl (slotLock);
        freeSlots.push_back (std::move (slot));
    }
}

void VideoRecorder::finishTake()
{
    juce::String text;

    if (writer.isOpen())
    {
        const bool ok = writer.close();
        bytesWritten = writer.getBytesWritten();

        if (ok)
        {
            text = "Saved " + file.getFileName() + " (" + juce::File::descriptionOfSizeInBytes (bytesWritten.load()) + ")";

            if (diskFull)
                text = "Stopped, disk nearly full. " + text;

            const juce::ScopedLock sl (messageLock);
            lastFile = file;
        }
        else
        {
            text = "Recording failed: could not write " + file.getFileName();
        }
    }
    else if (openFailed)
    {
        text = "Could not create a file in " + file.getParentDirectory().getFullPathName();
    }
    else if (noFrames)
    {
        text = "Nothing recorded. Keep this window open and visible.";
    }
    else
    {
        text = "Nothing recorded.";
    }

    setMessage (text);

    {
        const juce::ScopedLock sl (slotLock);

        while (! queue.empty())
        {
            freeSlots.push_back (std::move (queue.front()));
            queue.pop_front();
        }
    }

    pcm.clear();
    takeActive = false;
    openFailed = noFrames = diskFull = false;
    lastSpaceCheck = 0.0;
}

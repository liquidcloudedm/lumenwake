#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray LumenwakeProcessor::sceneNames { "Image", "Model 3D", "Neon Tunnel", "Ink Flow", "Kaleido Bloom", "Liquid Metal", "Fractal Dive", "Spectrum" };
const juce::StringArray LumenwakeProcessor::imageModeNames { "Liquid", "Ripple", "Swirl", "Kaleido", "Tunnel", "Flyover", "Relief", "Glitch", "Mosaic", "Echo", "Wave", "Prism" };
const juce::StringArray LumenwakeProcessor::modelModeNames { "Chrome", "Shatter", "Dust", "Hologram", "Wire", "Iridescent" };
const juce::StringArray LumenwakeProcessor::fitNames { "Fill", "Fit" };

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LumenwakeProcessor::createLayout()
{
    using Choice = juce::AudioParameterChoice;
    using Float = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto id = [] (const char* name) { return juce::ParameterID { name, 1 }; };

    layout.add (std::make_unique<Choice> (id ("scene"), "Scene", sceneNames, (int) sceneModel));
    layout.add (std::make_unique<Choice> (id ("imageMode"), "Image Mode", imageModeNames, 0));
    layout.add (std::make_unique<Choice> (id ("modelMode"), "Model Mode", modelModeNames, 0));
    layout.add (std::make_unique<Choice> (id ("imageFit"), "Image Fit", fitNames, 0));

    Range sensitivity (0.1f, 4.0f, 0.001f);
    sensitivity.setSkewForCentre (1.0f);
    Range zoom (0.25f, 4.0f, 0.001f);
    zoom.setSkewForCentre (1.0f);

    layout.add (std::make_unique<Float> (id ("sensitivity"), "Sensitivity", sensitivity, 1.0f));
    layout.add (std::make_unique<Float> (id ("smoothing"), "Smoothing", Range (0.0f, 1.0f, 0.001f), 0.35f));
    layout.add (std::make_unique<Float> (id ("amount"), "Intensity", Range (0.0f, 1.0f, 0.001f), 0.6f));
    layout.add (std::make_unique<Float> (id ("speed"), "Speed", Range (0.0f, 3.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<Float> (id ("zoom"), "Zoom", zoom, 1.0f));
    layout.add (std::make_unique<Float> (id ("trails"), "Trails", Range (0.0f, 0.98f, 0.001f), 0.0f));
    layout.add (std::make_unique<Float> (id ("glow"), "Glow", Range (0.0f, 1.0f, 0.001f), 0.35f));
    layout.add (std::make_unique<Float> (id ("hue"), "Hue", Range (-180.0f, 180.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<Float> (id ("saturation"), "Saturation", Range (0.0f, 2.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<Float> (id ("brightness"), "Brightness", Range (0.0f, 2.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<Float> (id ("turn"), "Turn", Range (-180.0f, 180.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<Float> (id ("tilt"), "Tilt", Range (-180.0f, 180.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<Float> (id ("spin"), "Spin", Range (-1.0f, 1.0f, 0.001f), 0.0f));

    // Effects: new parameters go after the ones above, so existing automation keeps its place.
    layout.add (std::make_unique<Choice> (id ("fxSync"), "FX Sync", fxSyncNames, 0));
    layout.add (std::make_unique<Float> (id ("fxBpm"), "FX Free BPM", Range (40.0f, 240.0f, 0.1f), 120.0f));

    static const int defaultRate[numFx] = { 6, 5, 1, 5, 5, 3, 5, 6, 5, 5 };

    for (int i = 0; i < numFx; ++i)
    {
        layout.add (std::make_unique<Float> (juce::ParameterID { fxIds[i], 1 }, "FX " + fxNames[i], Range (0.0f, 1.0f, 0.001f), 0.0f));
        layout.add (std::make_unique<Choice> (juce::ParameterID { fxIds[i] + "Rate", 1 }, "FX " + fxNames[i] + " Rate", fxRateNames, defaultRate[i]));
    }

    layout.add (std::make_unique<Choice> (id ("fxColorMode"), "FX Color Mode", fxColorModeNames, 0));

    return layout;
}

//==============================================================================
const juce::StringArray LumenwakeProcessor::fxNames { "Strobe", "Pulse", "Color Shift", "Zoom Punch", "Shake", "Swing", "RGB Split", "Glitch", "Glow Pump", "Pixelate" };
const juce::StringArray LumenwakeProcessor::fxIds { "fxStrobe", "fxPulse", "fxColor", "fxZoom", "fxShake", "fxSwing", "fxSplit", "fxGlitch", "fxGlow", "fxPixel" };
const juce::StringArray LumenwakeProcessor::fxRateNames { "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32" };
const juce::StringArray LumenwakeProcessor::fxSyncNames { "Host tempo", "Free" };
const juce::StringArray LumenwakeProcessor::fxColorModeNames { "Cycle", "Step" };

double LumenwakeProcessor::fxRateBeats (int rateIndex) noexcept
{
    // Length of one cycle in quarter notes, taking a bar as four of them.
    static const double beats[] = { 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    return beats[juce::jlimit (0, 8, rateIndex)];
}

LumenwakeProcessor::HostClock LumenwakeProcessor::getHostClock() const noexcept
{
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        const auto before = clockSequence.load();

        if ((before & 1u) != 0)
            continue;

        HostClock c;
        c.bpm = clockBpm.load();
        c.ppq = clockPpq.load();
        c.timeMs = clockTimeMs.load();
        c.playing = clockPlaying.load();
        c.hasPosition = clockHasPosition.load();

        if (clockSequence.load() == before)
            return c;
    }

    return {};
}

//==============================================================================
LumenwakeProcessor::LumenwakeProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Lumenwake", createLayout())
{
    image = makePlaceholderImage();
    imageName = "Placeholder";
    mesh = ModelLoader::makeBuiltin();
    imageGeneration = 1;
    meshGeneration = 1;
}

LumenwakeProcessor::~LumenwakeProcessor()
{
    // A model may still be loading; let it finish before members go away.
    ++modelLoadToken;
    loaderPool.removeAllJobs (true, -1);
}

void LumenwakeProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
}

bool LumenwakeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void LumenwakeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // The audio itself is left exactly as it arrived.
    if (numIn > 0 && numSamples > 0)
        fifo.push (buffer.getReadPointer (0), numIn > 1 ? buffer.getReadPointer (1) : nullptr, numSamples);

    // Where the host transport is. The effects keep time with it, and a take
    // follows it.
    bool hasTransport = false, playing = false;

    if (wrapperType != wrapperType_Standalone)
    {
        if (auto* head = getPlayHead())
        {
            if (const auto position = head->getPosition())
            {
                hasTransport = true;
                playing = position->getIsPlaying();

                const auto bpm = position->getBpm();
                const auto ppq = position->getPpqPosition();

                ++clockSequence;
                clockBpm.store (bpm.hasValue() ? *bpm : 0.0);
                clockPpq.store (ppq.hasValue() ? *ppq : 0.0);
                clockHasPosition.store (ppq.hasValue());
                clockPlaying.store (playing);
                clockTimeMs.store (juce::Time::getMillisecondCounterHiRes());
                ++clockSequence;
            }
        }
    }

    // A take starts on the first block where the host is playing and ends on
    // the first block where it is not.
    if (recorder.getState() != VideoRecorder::idle && numIn > 0 && numSamples > 0 && ! isNonRealtime())
    {
        recorder.processAudio (buffer.getReadPointer (0), numIn > 1 ? buffer.getReadPointer (1) : nullptr,
                               numSamples, getSampleRate(), hasTransport, playing);
    }
}

//==============================================================================
const juce::StringArray LumenwakeProcessor::recordPresetNames { "1080p 30", "1080p 60", "720p 30", "720p 60", "Vertical 30", "Square 30" };

VideoRecorder::Format LumenwakeProcessor::getRecordFormat (int preset)
{
    switch (preset)
    {
        case 1:  return { 1920, 1080, 60 };
        case 2:  return { 1280, 720, 30 };
        case 3:  return { 1280, 720, 60 };
        case 4:  return { 1080, 1920, 30 };
        case 5:  return { 1080, 1080, 30 };
        default: return { 1920, 1080, 30 };
    }
}

juce::File LumenwakeProcessor::getRecordFolder()
{
    const auto custom = juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_REC_DIR", {});

    if (custom.isNotEmpty())
        return juce::File (custom);

    return juce::File::getSpecialLocation (juce::File::userMoviesDirectory).getChildFile ("Lumenwake");
}

bool LumenwakeProcessor::armRecording()
{
    const auto folder = getRecordFolder();

    if (! folder.createDirectory())
    {
        setStatus ("Could not create " + folder.getFullPathName());
        return false;
    }

    // Named by date and time, and never on top of a file that is already there.
    const auto file = folder.getChildFile ("Lumenwake " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".mov")
                            .getNonexistentSibling();

    return recorder.arm (file, getRecordFormat (recordPreset));
}

juce::AudioProcessorEditor* LumenwakeProcessor::createEditor()
{
    return new LumenwakeEditor (*this);
}

//==============================================================================
void LumenwakeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    {
        const juce::ScopedLock sl (assetLock);
        state.setProperty ("imagePath", imagePath, nullptr);
        state.setProperty ("modelPath", modelPath, nullptr);
    }

    state.setProperty ("editorWidth", editorWidth, nullptr);
    state.setProperty ("editorHeight", editorHeight, nullptr);
    state.setProperty ("panelVisible", panelVisible, nullptr);
    state.setProperty ("recordPreset", recordPreset, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LumenwakeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return;

    apvts.replaceState (state);

    editorWidth = juce::jlimit (480, 8192, (int) state.getProperty ("editorWidth", editorWidth));
    editorHeight = juce::jlimit (320, 8192, (int) state.getProperty ("editorHeight", editorHeight));
    panelVisible = (bool) state.getProperty ("panelVisible", true);
    recordPreset = juce::jlimit (0, recordPresetNames.size() - 1, (int) state.getProperty ("recordPreset", recordPreset));

    const auto savedImage = state.getProperty ("imagePath").toString();
    const auto savedModel = state.getProperty ("modelPath").toString();

    if (savedImage.isNotEmpty())
        loadImage (juce::File (savedImage));
    else
        clearImage();

    if (savedModel.isNotEmpty())
        loadModel (juce::File (savedModel));
    else
        useBuiltinModel();
}

//==============================================================================
bool LumenwakeProcessor::isSupportedImage (const juce::File& file)
{
    return file.hasFileExtension ("png;jpg;jpeg;gif;webp");
}

void LumenwakeProcessor::loadImage (const juce::File& file)
{
    auto loaded = juce::ImageFileFormat::loadFrom (file);

    if (! loaded.isValid())
    {
        setStatus ("Could not open image: " + file.getFileName());
        return;
    }

    // Keep textures to a size any GPU accepts.
    const int maxSide = 4096;

    if (loaded.getWidth() > maxSide || loaded.getHeight() > maxSide)
    {
        const double s = (double) maxSide / juce::jmax (loaded.getWidth(), loaded.getHeight());
        loaded = loaded.rescaled (juce::jmax (1, juce::roundToInt (loaded.getWidth() * s)),
                                  juce::jmax (1, juce::roundToInt (loaded.getHeight() * s)),
                                  juce::Graphics::highResamplingQuality);
    }

    loaded = juce::SoftwareImageType().convert (loaded.convertedToFormat (juce::Image::ARGB));

    {
        const juce::ScopedLock sl (assetLock);
        image = loaded;
        imagePath = file.getFullPathName();
        imageName = file.getFileName();
        status = imageName + "  " + juce::String (loaded.getWidth()) + " x " + juce::String (loaded.getHeight());
    }

    ++imageGeneration;
}

void LumenwakeProcessor::clearImage()
{
    {
        const juce::ScopedLock sl (assetLock);

        if (imagePath.isEmpty())
            return;

        image = makePlaceholderImage();
        imagePath.clear();
        imageName = "Placeholder";
    }

    ++imageGeneration;
}

void LumenwakeProcessor::loadModel (const juce::File& file)
{
    const int token = ++modelLoadToken;
    modelLoading = true;
    setStatus ("Loading " + file.getFileName() + "...");

    loaderPool.addJob ([this, file, token]
    {
        auto loaded = ModelLoader::load (file);

        if (token != modelLoadToken.load())
            return;

        {
            const juce::ScopedLock sl (assetLock);

            if (loaded->isValid())
            {
                mesh = loaded;
                modelPath = file.getFullPathName();
                status = loaded->name + "  " + juce::String (loaded->sourceTriangles) + " triangles";

                if (loaded->numTriangles < loaded->sourceTriangles)
                    status << " (drawing " << loaded->numTriangles << ")";
            }
            else
            {
                status = loaded->error;
            }
        }

        if (loaded->isValid())
            ++meshGeneration;

        modelLoading = false;
    });
}

void LumenwakeProcessor::useBuiltinModel()
{
    ++modelLoadToken;
    modelLoading = false;

    {
        const juce::ScopedLock sl (assetLock);

        if (modelPath.isEmpty() && mesh != nullptr)
            return;

        mesh = ModelLoader::makeBuiltin();
        modelPath.clear();
        status.clear();
    }

    ++meshGeneration;
}

juce::Image LumenwakeProcessor::getImage() const
{
    const juce::ScopedLock sl (assetLock);
    return image;
}

std::shared_ptr<MeshData> LumenwakeProcessor::getMesh() const
{
    const juce::ScopedLock sl (assetLock);
    return mesh;
}

juce::String LumenwakeProcessor::getImageName() const
{
    const juce::ScopedLock sl (assetLock);
    return imageName;
}

juce::String LumenwakeProcessor::getModelName() const
{
    const juce::ScopedLock sl (assetLock);
    return mesh != nullptr ? mesh->name : juce::String();
}

juce::String LumenwakeProcessor::getStatus() const
{
    const juce::ScopedLock sl (assetLock);
    return status;
}

void LumenwakeProcessor::setStatus (const juce::String& s)
{
    const juce::ScopedLock sl (assetLock);
    status = s;
}

//==============================================================================
juce::Image LumenwakeProcessor::makePlaceholderImage()
{
    // Drawn pixel by pixel so it needs no fonts or graphics context: soft
    // overlapping discs over a diagonal wash, with enough detail and contrast
    // that every image mode has something to chew on.
    const int w = 1024, h = 640;
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);

    struct Disc { float x, y, r; float cr, cg, cb; };
    const Disc discs[] = { { 0.30f, 0.42f, 0.30f, 1.00f, 0.42f, 0.25f },
                           { 0.66f, 0.36f, 0.26f, 0.20f, 0.62f, 1.00f },
                           { 0.52f, 0.70f, 0.24f, 1.00f, 0.82f, 0.25f },
                           { 0.82f, 0.74f, 0.16f, 0.85f, 0.25f, 0.75f },
                           { 0.14f, 0.80f, 0.14f, 0.25f, 0.90f, 0.60f } };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float u = (float) x / (float) w, v = (float) y / (float) h;
            const float aspect = (float) w / (float) h;

            float r = 0.06f + 0.10f * u, g = 0.05f + 0.06f * v, b = 0.12f + 0.14f * (1.0f - u) * v;

            for (auto& d : discs)
            {
                const float dx = (u - d.x) * aspect, dy = v - d.y;
                const float dist = std::sqrt (dx * dx + dy * dy);
                const float edge = juce::jlimit (0.0f, 1.0f, (d.r - dist) / 0.004f);
                const float rings = 0.82f + 0.18f * std::sin (dist * 90.0f);
                const float shade = (0.55f + 0.45f * (1.0f - dist / d.r)) * rings;
                r = r + (d.cr * shade - r) * edge * 0.85f;
                g = g + (d.cg * shade - g) * edge * 0.85f;
                b = b + (d.cb * shade - b) * edge * 0.85f;
            }

            const float stripe = (std::fmod (u * 14.0f + v * 6.0f, 1.0f) < 0.06f) ? 0.10f : 0.0f;

            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, r + stripe),
                                                                   juce::jlimit (0.0f, 1.0f, g + stripe),
                                                                   juce::jlimit (0.0f, 1.0f, b + stripe), 1.0f));
        }
    }

    return img;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LumenwakeProcessor();
}

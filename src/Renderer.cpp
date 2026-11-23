#include "Renderer.h"
#include "Shaders.h"

using namespace juce::gl;

//==============================================================================
struct VisualRenderer::Frame
{
    int width = 0, height = 0;
    int scene = 0, mode = 0, fit = 0;
    float amount = 0, zoom = 1, trails = 0, glow = 0, hue = 0, saturation = 1, brightness = 1;
    float turn = 0, tilt = 0;
    AudioAnalyzer::Features audio;

    // Effects, already worked out for this moment in the bar
    float fxZoom = 1, fxAngle = 0, fxShiftX = 0, fxShiftY = 0, fxSplit = 0;
    float fxGlitch = 0, fxGlitchSeed = 0, fxPixel = 0, fxGlow = 0, fxGain = 1;
    float fxTint[3] = { 1, 1, 1 }, fxTintAmount = 0, fxHue = 0;
};

namespace
{
    struct Mat4
    {
        float m[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
    };

    Mat4 operator* (const Mat4& a, const Mat4& b)
    {
        Mat4 r;

        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
            {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
                r.m[c * 4 + row] = s;
            }

        return r;
    }

    Mat4 perspective (float fovY, float aspect, float zNear, float zFar)
    {
        Mat4 r;
        const float t = 1.0f / std::tan (fovY * 0.5f);
        r.m[0] = t / aspect;
        r.m[5] = t;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;
        r.m[14] = 2.0f * zFar * zNear / (zNear - zFar);
        r.m[15] = 0.0f;
        return r;
    }

    Mat4 translation (float x, float y, float z)   { Mat4 r; r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
    Mat4 scaling (float s)                         { Mat4 r; r.m[0] = r.m[5] = r.m[10] = s; return r; }

    Mat4 rotationX (float a)
    {
        Mat4 r;
        const float c = std::cos (a), s = std::sin (a);
        r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
        return r;
    }

    Mat4 rotationY (float a)
    {
        Mat4 r;
        const float c = std::cos (a), s = std::sin (a);
        r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
        return r;
    }

    void setFloat (GLuint program, const char* name, float v)
    {
        const GLint l = glGetUniformLocation (program, name);
        if (l >= 0) glUniform1f (l, v);
    }

    void setInt (GLuint program, const char* name, int v)
    {
        const GLint l = glGetUniformLocation (program, name);
        if (l >= 0) glUniform1i (l, v);
    }

    void setVec2 (GLuint program, const char* name, float x, float y)
    {
        const GLint l = glGetUniformLocation (program, name);
        if (l >= 0) glUniform2f (l, x, y);
    }

    void setVec3 (GLuint program, const char* name, float x, float y, float z)
    {
        const GLint l = glGetUniformLocation (program, name);
        if (l >= 0) glUniform3f (l, x, y, z);
    }

    void setMat4 (GLuint program, const char* name, const Mat4& m)
    {
        const GLint l = glGetUniformLocation (program, name);
        if (l >= 0) glUniformMatrix4fv (l, 1, GL_FALSE, m.m);
    }

    GLuint compileShader (GLenum type, const juce::String& source, juce::String& log)
    {
        const GLuint shader = glCreateShader (type);
        const GLchar* text = source.toRawUTF8();
        glShaderSource (shader, 1, &text, nullptr);
        glCompileShader (shader);

        GLint ok = 0;
        glGetShaderiv (shader, GL_COMPILE_STATUS, &ok);

        if (ok == 0)
        {
            char buffer[4096] = {};
            GLsizei length = 0;
            glGetShaderInfoLog (shader, (GLsizei) sizeof (buffer) - 1, &length, buffer);
            log = juce::String::fromUTF8 (buffer, (int) length);
            glDeleteShader (shader);
            return 0;
        }

        return shader;
    }

    const char* programName (int id)
    {
        static const char* const names[] = { "image", "tunnel", "ink", "kaleido", "metal", "fractal", "spectrum",
                                             "model-background", "mesh", "dust", "accumulate", "post" };
        return names[id];
    }
}

//==============================================================================
VisualRenderer::VisualRenderer (LumenwakeProcessor& p, juce::Component& t)
    : processor (p), target (t)
{
    auto& v = processor.apvts;
    pScene = v.getRawParameterValue ("scene");
    pImageMode = v.getRawParameterValue ("imageMode");
    pModelMode = v.getRawParameterValue ("modelMode");
    pFit = v.getRawParameterValue ("imageFit");
    pSensitivity = v.getRawParameterValue ("sensitivity");
    pSmoothing = v.getRawParameterValue ("smoothing");
    pAmount = v.getRawParameterValue ("amount");
    pSpeed = v.getRawParameterValue ("speed");
    pZoom = v.getRawParameterValue ("zoom");
    pTrails = v.getRawParameterValue ("trails");
    pGlow = v.getRawParameterValue ("glow");
    pHue = v.getRawParameterValue ("hue");
    pSaturation = v.getRawParameterValue ("saturation");
    pBrightness = v.getRawParameterValue ("brightness");
    pTurn = v.getRawParameterValue ("turn");
    pTilt = v.getRawParameterValue ("tilt");
    pSpin = v.getRawParameterValue ("spin");
    pFxSync = v.getRawParameterValue ("fxSync");
    pFxBpm = v.getRawParameterValue ("fxBpm");
    pFxColorMode = v.getRawParameterValue ("fxColorMode");

    for (int i = 0; i < numFx; ++i)
    {
        pFx[i] = v.getRawParameterValue (LumenwakeProcessor::fxIds[i]);
        pFxRate[i] = v.getRawParameterValue (LumenwakeProcessor::fxIds[i] + "Rate");
    }

    const auto dumpPath = juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_DUMP", {});

    if (dumpPath.isNotEmpty())
    {
        dumpFolder = juce::File (dumpPath);
        dumping = dumpFolder.isDirectory();

        const auto only = juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_DUMP_ONLY", {});

        if (only.isEmpty() || only == "image")
            for (int i = 0; i < LumenwakeProcessor::imageModeNames.size(); ++i) dumpItems.push_back ({ sceneImage, i });

        if (only.isEmpty() || only == "model")
            for (int i = 0; i < LumenwakeProcessor::modelModeNames.size(); ++i) dumpItems.push_back ({ sceneModel, i });

        if (only.isEmpty() || only == "scenes")
            for (int i = sceneTunnel; i < numScenes; ++i) dumpItems.push_back ({ i, 0 });

        if (only == "fx")
        {
            for (int i = 0; i < numFx; ++i) dumpItems.push_back ({ sceneImage, 0, i });
            for (int i = 0; i < numFx; ++i) dumpItems.push_back ({ sceneModel, 0, i });
        }

        const auto model = juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_DUMP_MODEL", {});
        const auto picture = juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_DUMP_IMAGE", {});

        if (dumping && model.isNotEmpty())   processor.loadModel (juce::File (model));
        if (dumping && picture.isNotEmpty()) processor.loadImage (juce::File (picture));
    }

    const auto shotPath = juce::SystemStats::getEnvironmentVariable ("LUMENWAKE_SHOT", {});

    if (shotPath.isNotEmpty())
        shotFile = juce::File (shotPath);

    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    context.setRenderer (this);
    context.setComponentPaintingEnabled (false);
    context.setContinuousRepainting (true);
    context.setSwapInterval (1);
    context.attachTo (target);
}

VisualRenderer::~VisualRenderer()
{
    context.detach();
}

juce::String VisualRenderer::getShaderError() const
{
    const juce::ScopedLock sl (errorLock);
    return shaderError;
}

//==============================================================================
void VisualRenderer::newOpenGLContextCreated()
{
    for (auto& p : programs)
        p = {};

    sceneTarget = {};
    accum[0] = {};
    accum[1] = {};
    imageGeneration = meshGeneration = -1;
    lastScene = lastMode = -1;
    meshVertexCount = pointCount = 0;
    lastFrameTime = 0.0;

    glGenVertexArrays (1, &emptyVao);

    glGenTextures (1, &audioTexture);
    glBindTexture (GL_TEXTURE_2D, audioTexture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_R32F, AudioAnalyzer::texWidth, 2, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures (1, &imageTexture);
    glGenVertexArrays (1, &meshVao);
    glGenBuffers (1, &meshVbo);
    glGenVertexArrays (1, &pointVao);
    glGenBuffers (1, &pointVbo);
}

void VisualRenderer::openGLContextClosing()
{
    for (auto& p : programs)
    {
        if (p.id != 0) glDeleteProgram (p.id);
        p = {};
    }

    destroyTarget (sceneTarget);
    destroyTarget (accum[0]);
    destroyTarget (accum[1]);
    destroyCapture();

    glDeleteTextures (1, &audioTexture);
    glDeleteTextures (1, &imageTexture);
    glDeleteVertexArrays (1, &emptyVao);
    glDeleteVertexArrays (1, &meshVao);
    glDeleteVertexArrays (1, &pointVao);
    glDeleteBuffers (1, &meshVbo);
    glDeleteBuffers (1, &pointVbo);
    emptyVao = audioTexture = imageTexture = meshVao = meshVbo = pointVao = pointVbo = 0;
}

//==============================================================================
bool VisualRenderer::buildProgram (Program& program, ProgramId id)
{
    const juce::String header (shaders::common);
    juce::String vertex (shaders::fullscreenVertex), fragment;

    switch (id)
    {
        case progImage:           fragment = header + shaders::image; break;
        case progTunnel:          fragment = header + shaders::neonTunnel; break;
        case progInk:             fragment = header + shaders::inkFlow; break;
        case progKaleido:         fragment = header + shaders::kaleidoBloom; break;
        case progMetal:           fragment = header + shaders::liquidMetal; break;
        case progFractal:         fragment = header + shaders::fractalDive; break;
        case progSpectrum:        fragment = header + shaders::spectrumScene; break;
        case progModelBackground: fragment = header + shaders::modelBackground; break;
        case progMesh:            vertex = shaders::meshVertex; fragment = header + shaders::meshFragment; break;
        case progDust:            vertex = shaders::dustVertex; fragment = shaders::dustFragment; break;
        case progAccumulate:      fragment = header + shaders::accumulate; break;
        case progPost:            fragment = header + shaders::post; break;
        case numPrograms:         return false;
    }

    juce::String log;
    const GLuint vs = compileShader (GL_VERTEX_SHADER, vertex, log);
    const GLuint fs = vs != 0 ? compileShader (GL_FRAGMENT_SHADER, fragment, log) : 0;
    GLuint linked = 0;

    if (vs != 0 && fs != 0)
    {
        linked = glCreateProgram();
        glAttachShader (linked, vs);
        glAttachShader (linked, fs);
        glLinkProgram (linked);

        GLint ok = 0;
        glGetProgramiv (linked, GL_LINK_STATUS, &ok);

        if (ok == 0)
        {
            char buffer[4096] = {};
            GLsizei length = 0;
            glGetProgramInfoLog (linked, (GLsizei) sizeof (buffer) - 1, &length, buffer);
            log = juce::String::fromUTF8 (buffer, (int) length);
            glDeleteProgram (linked);
            linked = 0;
        }
    }

    if (vs != 0) glDeleteShader (vs);
    if (fs != 0) glDeleteShader (fs);

    if (linked == 0)
    {
        const juce::ScopedLock sl (errorLock);
        shaderError = juce::String ("Shader '") + programName (id) + "' failed: " + log.trim();

        if (dumping)
            dumpFolder.getChildFile ("shader_errors.txt").appendText (shaderError + "\n");

        return false;
    }

    program.id = linked;
    return true;
}

VisualRenderer::Program& VisualRenderer::useProgram (ProgramId id)
{
    auto& p = programs[id];

    if (! p.attempted)
    {
        p.attempted = true;
        buildProgram (p, id);
    }

    glUseProgram (p.id);
    return p;
}

//==============================================================================
void VisualRenderer::createTarget (Target& t, int width, int height, bool withDepth)
{
    destroyTarget (t);
    t.width = width;
    t.height = height;

    glGenTextures (1, &t.texture);
    glBindTexture (GL_TEXTURE_2D, t.texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, withDepth ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (! withDepth)
        glGenerateMipmap (GL_TEXTURE_2D);

    glGenFramebuffers (1, &t.fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.texture, 0);

    if (withDepth)
    {
        glGenRenderbuffers (1, &t.depth);
        glBindRenderbuffer (GL_RENDERBUFFER, t.depth);
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depth);
    }

    glViewport (0, 0, width, height);
    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
    glClear (GL_COLOR_BUFFER_BIT);
}

void VisualRenderer::destroyTarget (Target& t)
{
    if (t.fbo != 0)     glDeleteFramebuffers (1, &t.fbo);
    if (t.texture != 0) glDeleteTextures (1, &t.texture);
    if (t.depth != 0)   glDeleteRenderbuffers (1, &t.depth);
    t = {};
}

void VisualRenderer::ensureTargets (int width, int height)
{
    if (sceneTarget.width == width && sceneTarget.height == height && sceneTarget.fbo != 0)
        return;

    createTarget (sceneTarget, width, height, true);
    createTarget (accum[0], width, height, false);
    createTarget (accum[1], width, height, false);
}

void VisualRenderer::clearAccumulation()
{
    for (auto& t : accum)
    {
        if (t.fbo == 0)
            continue;

        glBindFramebuffer (GL_FRAMEBUFFER, t.fbo);
        glViewport (0, 0, t.width, t.height);
        glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT);
    }
}

//==============================================================================
// Recording: the graded picture is drawn once more into an 8-bit target and
// read back through a pixel buffer, so the read never stalls the frame.
void VisualRenderer::ensureCapture (int width, int height)
{
    if (captureFbo != 0 && captureWidth == width && captureHeight == height)
        return;

    destroyCapture();
    captureWidth = width;
    captureHeight = height;

    glGenTextures (1, &captureTexture);
    glBindTexture (GL_TEXTURE_2D, captureTexture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers (1, &captureFbo);
    glBindFramebuffer (GL_FRAMEBUFFER, captureFbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, captureTexture, 0);

    glGenBuffers (1, &capturePbo);
    glBindBuffer (GL_PIXEL_PACK_BUFFER, capturePbo);
    glBufferData (GL_PIXEL_PACK_BUFFER, (GLsizeiptr) ((size_t) width * (size_t) height * 4), nullptr, GL_STREAM_READ);
    glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);
}

void VisualRenderer::destroyCapture()
{
    if (captureFbo != 0)     glDeleteFramebuffers (1, &captureFbo);
    if (captureTexture != 0) glDeleteTextures (1, &captureTexture);
    if (capturePbo != 0)     glDeleteBuffers (1, &capturePbo);
    captureFbo = captureTexture = capturePbo = 0;
    captureWidth = captureHeight = 0;
    captureIndex = -1;
}

void VisualRenderer::collectCapture()
{
    if (captureIndex < 0 || capturePbo == 0)
        return;

    const auto format = processor.recorder.getFormat();

    if (format.width == captureWidth && format.height == captureHeight)
    {
        glBindBuffer (GL_PIXEL_PACK_BUFFER, capturePbo);

        if (auto* data = glMapBufferRange (GL_PIXEL_PACK_BUFFER, 0,
                                           (GLsizeiptr) ((size_t) captureWidth * (size_t) captureHeight * 4), GL_MAP_READ_BIT))
        {
            processor.recorder.submitFrame (captureIndex, static_cast<const juce::uint8*> (data));
            glUnmapBuffer (GL_PIXEL_PACK_BUFFER);
        }

        glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);
    }

    captureIndex = -1;
}

//==============================================================================
void VisualRenderer::syncAssets()
{
    const int ig = processor.getImageGeneration();

    if (ig != imageGeneration)
    {
        imageGeneration = ig;
        uploadImage (processor.getImage());
        sceneAge = 0.0f;
    }

    const int mg = processor.getMeshGeneration();

    if (mg != meshGeneration)
    {
        meshGeneration = mg;

        if (auto mesh = processor.getMesh())
            uploadMesh (*mesh);
    }
}

void VisualRenderer::uploadImage (const juce::Image& source)
{
    if (! source.isValid() || source.getFormat() != juce::Image::ARGB)
        return;

    const int w = source.getWidth(), h = source.getHeight();
    std::vector<juce::uint8> pixels ((size_t) w * (size_t) h * 4);

    {
        juce::Image::BitmapData data (source, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < h; ++y)
            std::memcpy (pixels.data() + (size_t) y * (size_t) w * 4, data.getLinePointer (y), (size_t) w * 4);
    }

    // JUCE stores ARGB as premultiplied BGRA bytes, which suits us: anything
    // transparent simply reads as black.
    glBindTexture (GL_TEXTURE_2D, imageTexture);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap (GL_TEXTURE_2D);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

    imageAspect = (float) w / (float) juce::jmax (1, h);
}

void VisualRenderer::uploadMesh (const MeshData& mesh)
{
    const GLsizei triStride = (GLsizei) (MeshData::triStride * sizeof (float));
    const GLsizei pointStride = (GLsizei) (MeshData::pointStride * sizeof (float));
    auto offset = [] (int floats) { return (const void*) (size_t) (floats * (int) sizeof (float)); };

    glBindVertexArray (meshVao);
    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (mesh.tris.size() * sizeof (float)), mesh.tris.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray (0); glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, triStride, offset (0));
    glEnableVertexAttribArray (1); glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, triStride, offset (3));
    glEnableVertexAttribArray (2); glVertexAttribPointer (2, 3, GL_FLOAT, GL_FALSE, triStride, offset (6));
    glEnableVertexAttribArray (3); glVertexAttribPointer (3, 1, GL_FLOAT, GL_FALSE, triStride, offset (9));
    meshVertexCount = mesh.numTriangles * 3;

    glBindVertexArray (pointVao);
    glBindBuffer (GL_ARRAY_BUFFER, pointVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (mesh.points.size() * sizeof (float)), mesh.points.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray (0); glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, pointStride, offset (0));
    glEnableVertexAttribArray (1); glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, pointStride, offset (3));
    glEnableVertexAttribArray (2); glVertexAttribPointer (2, 1, GL_FLOAT, GL_FALSE, pointStride, offset (6));
    pointCount = (int) (mesh.points.size() / (size_t) MeshData::pointStride);

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

//==============================================================================
void VisualRenderer::setCommonUniforms (juce::uint32 program, const Frame& f)
{
    if (program == 0)
        return;

    setVec2 (program, "uRes", (float) f.width, (float) f.height);
    setFloat (program, "uTime", time);
    setFloat (program, "uSceneAge", sceneAge);
    setFloat (program, "uBass", f.audio.bass);
    setFloat (program, "uMid", f.audio.mid);
    setFloat (program, "uHigh", f.audio.high);
    setFloat (program, "uLevel", f.audio.level);
    setFloat (program, "uBeat", f.audio.beat);
    setFloat (program, "uBeatSlow", f.audio.beatSlow);
    setFloat (program, "uBassAcc", f.audio.bassAcc);
    setFloat (program, "uMidAcc", f.audio.midAcc);
    setFloat (program, "uHighAcc", f.audio.highAcc);
    setFloat (program, "uBeatCount", f.audio.beatCount);
    setFloat (program, "uAmount", f.amount);
    setFloat (program, "uZoom", f.zoom);
    setInt (program, "uMode", f.mode);
    setInt (program, "uAudio", 0);
    setInt (program, "uPrev", 1);
}

void VisualRenderer::drawFullscreen()
{
    glBindVertexArray (emptyVao);
    glDrawArrays (GL_TRIANGLES, 0, 3);
}

void VisualRenderer::drawModel (const Frame& f, bool transparentBackground)
{
    if (! transparentBackground)
    {
        auto& background = useProgram (progModelBackground);
        setCommonUniforms (background.id, f);

        if (background.id != 0)
            drawFullscreen();
    }

    if (meshVertexCount == 0)
        return;

    const float distance = 3.4f;
    const float aspect = (float) f.width / (float) juce::jmax (1, f.height);
    const float fovY = juce::degreesToRadians (35.0f);

    const Mat4 viewProj = perspective (fovY, aspect, 0.1f, 50.0f) * translation (0.0f, 0.0f, -distance);
    const Mat4 model = rotationX (juce::degreesToRadians (f.tilt))
                     * rotationY (juce::degreesToRadians (f.turn) + spinAngle)
                     * scaling (0.85f * f.zoom);

    const bool dust = f.mode == 2;
    auto& program = useProgram (dust ? progDust : progMesh);

    if (program.id == 0)
        return;

    setCommonUniforms (program.id, f);
    setMat4 (program.id, "uModel", model);
    setMat4 (program.id, "uViewProj", viewProj);
    setVec3 (program.id, "uCam", 0.0f, 0.0f, distance);
    setFloat (program.id, "uShatter", f.amount * (0.03f + 0.85f * f.audio.beatSlow + 0.3f * f.audio.bass));
    setFloat (program.id, "uPointScale", (float) f.height * 0.0075f);

    const bool additive = dust || f.mode == 3;

    if (additive)
    {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_ONE, GL_ONE, GL_ZERO, GL_ONE);     // adds light, leaves coverage alone
        glDisable (GL_DEPTH_TEST);
    }
    else
    {
        glDisable (GL_BLEND);
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (GL_LESS);
        glDepthMask (GL_TRUE);
    }

    if (dust)
    {
        glEnable (GL_PROGRAM_POINT_SIZE);
        glBindVertexArray (pointVao);
        glDrawArrays (GL_POINTS, 0, pointCount);
        glDisable (GL_PROGRAM_POINT_SIZE);
    }
    else
    {
        glBindVertexArray (meshVao);
        glDrawArrays (GL_TRIANGLES, 0, meshVertexCount);
    }

    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
}

//==============================================================================
void VisualRenderer::renderOpenGL()
{
    GLint screenFramebuffer = 0;
    glGetIntegerv (GL_FRAMEBUFFER_BINDING, &screenFramebuffer);

    const double scale = context.getRenderingScale();
    const int viewW = juce::roundToInt (scale * viewWidth.load());
    const int viewH = juce::roundToInt (scale * viewHeight.load());

    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glDisable (GL_SCISSOR_TEST);
    glDisable (GL_STENCIL_TEST);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (viewW < 8 || viewH < 8)
    {
        glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT);
        return;
    }

    // Timing
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    float dt = lastFrameTime > 0.0 ? (float) (now - lastFrameTime) : 1.0f / 60.0f;
    lastFrameTime = now;
    dt = juce::jlimit (0.001f, 0.1f, dt);
    fpsSmoothed += (1.0f / dt - fpsSmoothed) * 0.05f;
    fps = fpsSmoothed;

    if (dumping)
        dt = 1.0f / 60.0f;

    // Large windows render at a capped internal size and are scaled up.
    const float quality = juce::jmin (1.0f, 1920.0f / (float) juce::jmax (viewW, viewH));

    // While a recording is armed or running the picture is drawn at the size
    // of the video, whatever the window is, and shown the way it will composite.
    auto& recorder = processor.recorder;
    const int recordState = dumping ? (int) VideoRecorder::idle : (int) recorder.getState();
    const bool keyed = recordState == VideoRecorder::armed || recordState == VideoRecorder::recording;
    const auto recordFormat = recorder.getFormat();

    Frame f;
    f.width = keyed ? recordFormat.width : juce::jmax (16, (int) ((float) viewW * quality));
    f.height = keyed ? recordFormat.height : juce::jmax (16, (int) ((float) viewH * quality));
    f.scene = juce::jlimit (0, (int) numScenes - 1, juce::roundToInt (pScene->load()));
    f.mode = f.scene == sceneImage ? juce::roundToInt (pImageMode->load())
           : f.scene == sceneModel ? juce::roundToInt (pModelMode->load()) : 0;
    f.fit = juce::roundToInt (pFit->load());
    f.amount = pAmount->load();
    f.zoom = pZoom->load();
    f.trails = pTrails->load();
    f.glow = pGlow->load();
    f.hue = juce::degreesToRadians (pHue->load());
    f.saturation = pSaturation->load();
    f.brightness = pBrightness->load();
    f.turn = pTurn->load();
    f.tilt = pTilt->load();

    float sensitivity = pSensitivity->load();
    float smoothing = pSmoothing->load();
    float speed = pSpeed->load();

    if (dumping && dumpIndex < (int) dumpItems.size())
    {
        f.scene = dumpItems[(size_t) dumpIndex].scene;
        f.mode = dumpItems[(size_t) dumpIndex].mode;
        f.amount = 0.7f; f.zoom = 1.0f; f.trails = 0.0f; f.glow = 0.35f; f.hue = 0.0f;
        f.saturation = 1.0f; f.brightness = 1.0f; f.turn = 25.0f; f.tilt = 12.0f; f.fit = 0;
        sensitivity = 1.0f; smoothing = 0.35f; speed = 1.0f;

        // Effects are shown on a picture that is otherwise holding still.
        if (dumpItems[(size_t) dumpIndex].fx >= 0 && f.scene == sceneImage)
        {
            f.amount = 0.0f;
            f.fit = 1;
        }

        feedSyntheticAudio (dt);
        analyzer.update (syntheticFifo, 48000.0, dt, sensitivity, smoothing);
    }
    else
    {
        analyzer.update (processor.fifo, processor.currentSampleRate.load(), dt, sensitivity, smoothing);
    }

    f.audio = analyzer.getFeatures();
    updateEffects (f, dt);

    time += dt * speed;
    if (time > 3600.0f) time -= 3600.0f;
    sceneAge += dt;
    spinAngle = std::fmod (spinAngle + dt * pSpin->load() * juce::MathConstants<float>::twoPi * 0.25f,
                           juce::MathConstants<float>::twoPi);

    ensureTargets (f.width, f.height);
    syncAssets();

    if (f.scene != lastScene || f.mode != lastMode)
    {
        lastScene = f.scene;
        lastMode = f.mode;
        sceneAge = 0.0f;
        clearAccumulation();
    }

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, audioTexture);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, AudioAnalyzer::texWidth, 2, GL_RED, GL_FLOAT, analyzer.getTextureData());

    Target& previous = accum[accumIndex];
    Target& current = accum[1 - accumIndex];

    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, previous.texture);
    glActiveTexture (GL_TEXTURE2);
    glBindTexture (GL_TEXTURE_2D, imageTexture);

    //==========================================================================
    // 1. Scene
    glBindFramebuffer (GL_FRAMEBUFFER, sceneTarget.fbo);
    glViewport (0, 0, f.width, f.height);
    glDepthMask (GL_TRUE);
    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (f.scene == sceneModel)
    {
        drawModel (f, keyed);
    }
    else
    {
        static const ProgramId sceneProgram[numScenes] = { progImage, progMesh, progTunnel, progInk, progKaleido,
                                                           progMetal, progFractal, progSpectrum };
        auto& program = useProgram (sceneProgram[f.scene]);

        if (program.id != 0)
        {
            setCommonUniforms (program.id, f);
            setInt (program.id, "uImage", 2);
            setFloat (program.id, "uImageAspect", imageAspect);
            setInt (program.id, "uFit", f.fit);
            drawFullscreen();
        }
    }

    //==========================================================================
    // 2. Accumulate (trails)
    glBindFramebuffer (GL_FRAMEBUFFER, current.fbo);
    glViewport (0, 0, f.width, f.height);

    {
        auto& program = useProgram (progAccumulate);

        if (program.id != 0)
        {
            setCommonUniforms (program.id, f);
            glActiveTexture (GL_TEXTURE2);
            glBindTexture (GL_TEXTURE_2D, sceneTarget.texture);
            setInt (program.id, "uScene", 2);
            setFloat (program.id, "uTrails", f.trails);
            setFloat (program.id, "uSolid", f.scene == sceneModel ? 1.0f : 0.0f);
            drawFullscreen();
        }
    }

    glActiveTexture (GL_TEXTURE2);
    glBindTexture (GL_TEXTURE_2D, current.texture);
    glGenerateMipmap (GL_TEXTURE_2D);

    //==========================================================================
    // 3. Grade to the screen
    glBindFramebuffer (GL_FRAMEBUFFER, (GLuint) screenFramebuffer);
    glViewport (0, 0, viewW, viewH);

    if (keyed)
    {
        // Keep the shape of the video; the rest of the window stays dark.
        glClearColor (0.016f, 0.015f, 0.020f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT);

        const float fit = juce::jmin ((float) viewW / (float) f.width, (float) viewH / (float) f.height);
        const int w = juce::jlimit (1, viewW, juce::roundToInt ((float) f.width * fit));
        const int h = juce::jlimit (1, viewH, juce::roundToInt ((float) f.height * fit));
        glViewport ((viewW - w) / 2, (viewH - h) / 2, w, h);
    }

    {
        auto& program = useProgram (progPost);

        if (program.id != 0)
        {
            setCommonUniforms (program.id, f);
            setInt (program.id, "uTex", 2);
            setFloat (program.id, "uGlow", f.glow);
            setFloat (program.id, "uHue", f.hue);
            setFloat (program.id, "uSat", f.saturation);
            setFloat (program.id, "uBright", f.brightness);
            setInt (program.id, "uOutput", keyed ? 1 : 0);
            setFloat (program.id, "uFxZoom", f.fxZoom);
            setFloat (program.id, "uFxAngle", f.fxAngle);
            setVec2 (program.id, "uFxShift", f.fxShiftX, f.fxShiftY);
            setFloat (program.id, "uFxSplit", f.fxSplit);
            setVec2 (program.id, "uFxGlitch", f.fxGlitch, f.fxGlitchSeed);
            setFloat (program.id, "uFxPixel", f.fxPixel);
            setFloat (program.id, "uFxGlow", f.fxGlow);
            setFloat (program.id, "uFxGain", f.fxGain);
            setFloat (program.id, "uFxHue", f.fxHue);

            if (const GLint tint = glGetUniformLocation (program.id, "uFxTint"); tint >= 0)
                glUniform4f (tint, f.fxTint[0], f.fxTint[1], f.fxTint[2], f.fxTintAmount);
            drawFullscreen();
        }
        else
        {
            glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
            glClear (GL_COLOR_BUFFER_BIT);
        }

        //======================================================================
        // 4. Recording: hand over the frame read last time, then start the next.
        if (recordState == VideoRecorder::recording && program.id != 0)
        {
            collectCapture();

            const int due = recorder.frameDue();

            if (due >= 0)
            {
                ensureCapture (f.width, f.height);
                glBindFramebuffer (GL_FRAMEBUFFER, captureFbo);
                glViewport (0, 0, f.width, f.height);
                glActiveTexture (GL_TEXTURE2);
                glBindTexture (GL_TEXTURE_2D, current.texture);
                setInt (program.id, "uOutput", 2);
                drawFullscreen();

                glBindBuffer (GL_PIXEL_PACK_BUFFER, capturePbo);
                glPixelStorei (GL_PACK_ALIGNMENT, 1);
                glReadPixels (0, 0, f.width, f.height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);
                captureIndex = due;

                glBindFramebuffer (GL_FRAMEBUFFER, (GLuint) screenFramebuffer);
            }
        }
        else
        {
            captureIndex = -1;
        }
    }

    accumIndex = 1 - accumIndex;

    glBindVertexArray (0);
    glUseProgram (0);
    glActiveTexture (GL_TEXTURE0);

    if (dumping)
        stepDump (viewW, viewH, (GLuint) screenFramebuffer);

    // Self-test: one picture of the window as it looks two seconds into a take.
    if (shotFile != juce::File() && recordState == VideoRecorder::recording && recorder.getSecondsRecorded() > 2.0)
    {
        writeScreenshot (shotFile, viewW, viewH, (GLuint) screenFramebuffer);
        shotFile = juce::File();
    }
}

//==============================================================================
// Effects. Each one has a cycle whose length is its rate; "phase" is how far
// through the current cycle we are (0 to 1) and "step" counts the cycles.
void VisualRenderer::updateEffects (Frame& f, float dt)
{
    float amount[numFx];
    int rate[numFx];

    for (int i = 0; i < numFx; ++i)
    {
        amount[i] = dumping ? 0.0f : pFx[i]->load();
        rate[i] = juce::roundToInt (pFxRate[i]->load());
    }

    const int testing = dumping && dumpIndex < (int) dumpItems.size() ? dumpItems[(size_t) dumpIndex].fx : -1;

    if (testing >= 0)
        amount[testing] = testing == fxStrobe ? 0.7f : 1.0f;

    // Where we are in the music, in quarter notes. While the host is playing
    // its own position is used, carried forward from the last audio block;
    // otherwise the effects keep running at the host's tempo, or at the free one.
    const auto clock = processor.getHostClock();
    const double now = juce::Time::getMillisecondCounterHiRes();
    const bool followHost = juce::roundToInt (pFxSync->load()) == 0 && clock.bpm > 0.0 && now - clock.timeMs < 500.0;

    if (followHost && clock.playing && clock.hasPosition)
        fxBeats = clock.ppq + juce::jlimit (0.0, 0.25, (now - clock.timeMs) * 0.001) * clock.bpm / 60.0;
    else
        fxBeats += (double) dt * (followHost ? clock.bpm : (double) pFxBpm->load()) / 60.0;

    auto cycle = [&] (int fx, double& step)
    {
        if (testing >= 0)
        {
            step = 3.0 + dumpIndex;
            return fx == fxStrobe ? 0.6f : 0.12f;
        }

        const double x = fxBeats / LumenwakeProcessor::fxRateBeats (rate[fx]);
        step = std::floor (x);
        return (float) (x - step);
    };

    auto random = [] (double step, juce::uint32 salt)
    {
        auto h = (juce::uint32) (juce::int64) step * 2654435761u + salt * 40503u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return (float) (h & 0xffffffu) / 16777216.0f;
    };

    const float twoPi = juce::MathConstants<float>::twoPi;
    double step = 0.0;

    if (amount[fxStrobe] > 0.0f)
        f.fxGain *= cycle (fxStrobe, step) < 0.5f ? 1.0f : 1.0f - amount[fxStrobe];

    if (amount[fxPulse] > 0.0f)
        f.fxGain *= 1.0f - amount[fxPulse] * (1.0f - std::exp (-4.5f * cycle (fxPulse, step)));

    if (amount[fxColor] > 0.0f)
    {
        const float phase = cycle (fxColor, step);
        float hue = phase;

        if (juce::roundToInt (pFxColorMode->load()) == 1)
        {
            const double jump = step * 0.381966 + 0.07;    // a new colour every cycle, never the one before
            hue = (float) (jump - std::floor (jump));
        }

        auto channel = [hue] (float n)
        {
            const float k = std::fmod (n + hue * 6.0f, 6.0f);
            return 1.0f - juce::jlimit (0.0f, 1.0f, juce::jmin (k, 4.0f - k));
        };

        f.fxTint[0] = channel (5.0f);
        f.fxTint[1] = channel (3.0f);
        f.fxTint[2] = channel (1.0f);
        f.fxTintAmount = amount[fxColor];
        f.fxHue = hue * twoPi;
    }

    if (amount[fxZoom] > 0.0f)
        f.fxZoom = 1.0f + 0.35f * amount[fxZoom] * std::exp (-5.0f * cycle (fxZoom, step));

    if (amount[fxShake] > 0.0f)
    {
        const float phase = cycle (fxShake, step);
        const float size = 0.05f * amount[fxShake] * std::exp (-4.0f * phase);
        f.fxShiftX = size * std::sin (phase * 38.0f + random (step, 1) * twoPi);
        f.fxShiftY = size * std::cos (phase * 31.0f + random (step, 2) * twoPi);
    }

    if (amount[fxSwing] > 0.0f)
        f.fxAngle = 0.45f * amount[fxSwing] * std::sin (twoPi * cycle (fxSwing, step));

    if (amount[fxSplit] > 0.0f)
        f.fxSplit = 0.035f * amount[fxSplit] * (0.3f + 0.7f * std::exp (-4.0f * cycle (fxSplit, step)));

    if (amount[fxGlitch] > 0.0f)
    {
        // Most cycles glitch for their first part; the slices are redrawn eight times a cycle.
        const float phase = cycle (fxGlitch, step);

        if (phase < 0.6f && (testing >= 0 || random (step, 3) < 0.75f))
        {
            f.fxGlitch = 0.35f * amount[fxGlitch];
            f.fxGlitchSeed = (float) std::fmod (std::abs (step), 997.0) + 13.0f * std::floor (phase * 8.0f);
            f.fxSplit += 0.02f * amount[fxGlitch];
        }
    }

    if (amount[fxGlow] > 0.0f)
        f.fxGlow = 1.6f * amount[fxGlow] * std::exp (-4.0f * cycle (fxGlow, step));

    if (amount[fxPixel] > 0.0f)
    {
        const float cell = 0.08f * amount[fxPixel] * std::exp (-3.5f * cycle (fxPixel, step));
        f.fxPixel = cell > 0.004f ? cell : 0.0f;
    }
}

//==============================================================================
void VisualRenderer::feedSyntheticAudio (float dt)
{
    // A 120 BPM kick, off-beat hats and a held chord.
    const double sampleRate = 48000.0;
    const int n = juce::jlimit (1, 4096, (int) (dt * sampleRate));
    float block[4096];
    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < n; ++i)
    {
        const double t = syntheticTime;
        const double tb = std::fmod (t, 0.5);
        const double th = std::fmod (t + 0.25, 0.25);

        double s = std::sin (twoPi * (50.0 * tb + 3.0 * (1.0 - std::exp (-tb * 30.0)))) * std::exp (-tb * 9.0) * 0.8;
        s += (syntheticRandom.nextDouble() * 2.0 - 1.0) * std::exp (-th * 40.0) * 0.18;
        s += 0.10 * std::sin (twoPi * 330.0 * t) + 0.07 * std::sin (twoPi * 495.0 * t) + 0.04 * std::sin (twoPi * 1320.0 * t);

        block[i] = (float) s;
        syntheticTime += 1.0 / sampleRate;
    }

    syntheticFifo.pushMono (block, n);
}

void VisualRenderer::writeScreenshot (const juce::File& file, int width, int height, juce::uint32 framebuffer)
{
    std::vector<juce::uint8> pixels ((size_t) width * (size_t) height * 4);
    glBindFramebuffer (GL_READ_FRAMEBUFFER, framebuffer);
    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    juce::Image shot (juce::Image::RGB, width, height, false, juce::SoftwareImageType());

    {
        juce::Image::BitmapData data (shot, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            const auto* src = pixels.data() + (size_t) (height - 1 - y) * (size_t) width * 4;

            for (int x = 0; x < width; ++x)
                data.setPixelColour (x, y, juce::Colour (src[x * 4], src[x * 4 + 1], src[x * 4 + 2]));
        }
    }

    const int outW = juce::jmin (720, width);
    shot = shot.rescaled (outW, juce::jmax (1, height * outW / width), juce::Graphics::mediumResamplingQuality);

    if (auto stream = file.createOutputStream())
    {
        stream->setPosition (0);
        stream->truncate();
        juce::PNGImageFormat png;
        png.writeImageToStream (shot, *stream);
    }
}

void VisualRenderer::stepDump (int width, int height, juce::uint32 framebuffer)
{
    if (dumpIndex >= (int) dumpItems.size())
        return;

    if (++dumpFrame < 70)
        return;

    dumpFrame = 0;

    const auto item = dumpItems[(size_t) dumpIndex];
    juce::String name = LumenwakeProcessor::sceneNames[item.scene];

    if (item.scene == sceneImage) name << "_" << LumenwakeProcessor::imageModeNames[item.mode];
    if (item.scene == sceneModel) name << "_" << LumenwakeProcessor::modelModeNames[item.mode];
    if (item.fx >= 0)             name << "_fx-" << LumenwakeProcessor::fxNames[item.fx];

    writeScreenshot (dumpFolder.getChildFile (juce::String (dumpIndex).paddedLeft ('0', 2) + "_" + name.replaceCharacter (' ', '-') + ".png"),
                     width, height, framebuffer);

    if (++dumpIndex >= (int) dumpItems.size())
    {
        dumping = false;
        dumpFolder.getChildFile ("done.txt").replaceWithText ("fps " + juce::String (fpsSmoothed, 1) + "\n" + getShaderError());

        juce::MessageManager::callAsync ([]
        {
            if (auto* app = juce::JUCEApplicationBase::getInstance())
                app->systemRequestedQuit();
        });
    }
}

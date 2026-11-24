#pragma once

// All GLSL lives here. Every shader is original to this project.
// Each literal stays well under MSVC's 16 KB string-literal limit; the common
// header is prepended at runtime.

namespace shaders
{

//==============================================================================
static const char* const fullscreenVertex = R"GLSL(#version 330 core
out vec2 vUv;
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

//==============================================================================
static const char* const common = R"GLSL(#version 330 core
#define PI 3.14159265
#define TAU 6.28318531
uniform vec2 uRes;
uniform float uTime, uSceneAge;
uniform float uBass, uMid, uHigh, uLevel, uBeat, uBeatSlow;
uniform float uBassAcc, uMidAcc, uHighAcc, uBeatCount;
uniform float uAmount, uZoom;
uniform int uMode;
uniform sampler2D uAudio;
uniform sampler2D uPrev;

float spec(float x) { return texture(uAudio, vec2(clamp(x, 0.002, 0.998), 0.25)).r; }
float wave(float x) { return texture(uAudio, vec2(clamp(x, 0.002, 0.998), 0.75)).r; }

mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }
float hash11(float p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p); }
float hash21(vec2 p) { vec3 q = fract(vec3(p.xyx) * 0.1031); q += dot(q, q.yzx + 33.33); return fract((q.x + q.y) * q.z); }
vec2 hash22(vec2 p) { vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); q += dot(q, q.yzx + 33.33); return fract((q.xx + q.yz) * q.zy); }
float vnoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
               mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}
float fbm(vec2 p)
{
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = rot(0.5) * p * 2.02 + 17.0; a *= 0.5; }
    return s;
}
vec3 pal(float t, vec3 a, vec3 b, vec3 c, vec3 d) { return a + b * cos(TAU * (c * t + d)); }
vec3 rainbow(float t) { return pal(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.33, 0.67)); }
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
)GLSL";

//==============================================================================
// Accumulation (trails) and final grade.

static const char* const accumulate = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uScene;
uniform float uTrails, uSolid;
void main()
{
    // Alpha carries coverage: 1 where something solid was drawn (a 3D model),
    // 0 where the picture is only light. It is used when recording.
    vec4 s = texture(uScene, vUv);
    s.a *= uSolid;
    vec2 q = (vUv - 0.5) * (1.0 - 0.006 * uTrails) + 0.5;
    vec4 p = texture(uPrev, q) * uTrails;
    fragColor = max(s, p);
}
)GLSL";

static const char* const post = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uTex;
uniform float uGlow, uHue, uSat, uBright;
uniform int uOutput;    // 0 screen, 1 screen with transparency shown, 2 frame for the video file

// Effects. The renderer works out where each one is in its cycle and sends the
// result; with every effect off these leave the picture exactly as it was.
uniform float uFxZoom;      // picture scale, 1 = none
uniform float uFxAngle;     // rotation, radians
uniform vec2 uFxShift;      // offset, in picture heights
uniform float uFxSplit;     // how far red and blue are pulled apart, in picture heights
uniform vec2 uFxGlitch;     // x = how far slices jump, y = which slices
uniform float uFxPixel;     // block size, in picture heights
uniform float uFxGlow;      // added glow
uniform float uFxGain;      // strobe and pulse: 1 = untouched, 0 = gone
uniform vec4 uFxTint;       // colour shift: rgb = colour for white areas, a = amount
uniform float uFxHue;       // colour shift: turn for coloured areas, radians

vec3 hueRotate(vec3 c, float a)
{
    const vec3 k = vec3(0.57735);
    float ca = cos(a), sa = sin(a);
    return c * ca + cross(k, c) * sa + k * dot(k, c) * (1.0 - ca);
}
vec2 gradX, gradY;
vec4 pick(vec2 q)
{
    // Outside the frame there is nothing, rather than a smear of the edge.
    // The level of detail comes from the untouched picture: slices and blocks
    // jump from one pixel to the next, which would otherwise draw seams.
    vec2 inside = step(vec2(0.0), q) * step(q, vec2(1.0));
    return textureGrad(uTex, q, gradX, gradY) * inside.x * inside.y;
}
void main()
{
    // The video file wants its top row first.
    vec2 uv = uOutput == 2 ? vec2(vUv.x, 1.0 - vUv.y) : vUv;
    gradX = dFdx(uv) / uFxZoom;
    gradY = dFdy(uv) / uFxZoom;

    // Effects that move the picture.
    vec2 shape = vec2(uRes.x / uRes.y, 1.0);
    vec2 p = (uv - 0.5) * shape;
    p = rot(uFxAngle) * p / uFxZoom + uFxShift;

    if (uFxGlitch.x > 0.0)
    {
        float band = floor(uv.y * (5.0 + 20.0 * hash11(uFxGlitch.y + 3.0)));
        float jump = step(0.55, hash21(vec2(uFxGlitch.y, band)));
        p.x += (hash21(vec2(band, uFxGlitch.y)) - 0.5) * 2.0 * uFxGlitch.x * jump;
    }

    if (uFxPixel > 0.0)
        p = (floor(p / uFxPixel) + 0.5) * uFxPixel;

    vec2 q = p / shape + 0.5;
    vec4 t = pick(q);

    if (uFxSplit > 0.0)
    {
        vec2 o = vec2(uFxSplit / shape.x, 0.0);
        vec4 tr = pick(q + o), tb = pick(q - o);
        t = vec4(tr.r, t.g, tb.b, max(t.a, max(tr.a, tb.a)));
    }

    vec3 c = t.rgb;
    vec3 bloom = textureLod(uTex, q, 2.5).rgb * 0.25
               + textureLod(uTex, q, 4.0).rgb * 0.35
               + textureLod(uTex, q, 5.5).rgb * 0.40;
    float glow = uGlow + uFxGlow;
    c += bloom * bloom * glow * 1.6 + bloom * glow * 0.25;
    c = hueRotate(c, uHue);
    c = mix(vec3(luma(c)), c, uSat);
    c = max(c, 0.0) * uBright;
    // Linear up to 0.8, soft shoulder above, so pictures keep their colours.
    c = mix(c, 0.8 + 0.2 * tanh((c - 0.8) * 3.0), step(0.8, c));

    // Colour shift. Turning the hue does nothing to white or grey, so those
    // areas take the colour itself instead, keeping their brightness.
    if (uFxTint.a > 0.0)
    {
        float hi = max(c.r, max(c.g, c.b)), lo = min(c.r, min(c.g, c.b));
        float colourful = hi > 0.0001 ? (hi - lo) / hi : 0.0;
        vec3 turned = max(hueRotate(c, uFxHue), 0.0);
        c = mix(c, mix(hi * uFxTint.rgb, turned, smoothstep(0.08, 0.6, colourful)), uFxTint.a);
    }

    // Strobe and pulse take the picture away altogether, solid parts included.
    c *= uFxGain;
    t.a *= uFxGain;

    if (uOutput != 0)
    {
        // Light is as opaque as it is bright, so black is see-through; anything
        // solid stays solid whatever its colour.
        float a = clamp(max(t.a, max(c.r, max(c.g, c.b))), 0.0, 1.0);

        if (uOutput == 2)
        {
            // The colour goes in as it looks over black (premultiplied alpha),
            // so the file also reads correctly where the alpha channel is
            // ignored. Dividing the colour by the alpha instead (straight
            // alpha) leaves a glow as a flat block of colour for any player
            // or editor that does not apply the alpha in exactly that way.
            // The bytes land as alpha, red, green, blue.
            fragColor = vec4(a, clamp(c, 0.0, 1.0));
            return;
        }

        vec2 g = floor(gl_FragCoord.xy / 12.0);
        c += mix(vec3(0.050), vec3(0.085), mod(g.x + g.y, 2.0)) * (1.0 - a);
    }

    c += (hash21(gl_FragCoord.xy) - 0.5) / 255.0;
    fragColor = vec4(c, 1.0);
}
)GLSL";

//==============================================================================
// IMAGE: the user's picture, moved by the music. Twelve modes.

static const char* const image = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uImage;
uniform float uImageAspect;
uniform int uFit;

vec3 img(vec2 p)
{
    float sa = uRes.x / uRes.y;
    float s = 1.0;
    if (uFit == 0) { if (uImageAspect < sa) s = sa / uImageAspect; }
    else           { if (uImageAspect > sa) s = sa / uImageAspect; }
    vec2 u = vec2(p.x / (uImageAspect * s), p.y / s) * 0.5 + 0.5;
    vec3 c = texture(uImage, vec2(u.x, 1.0 - u.y)).rgb;
    if (uFit == 1)
    {
        vec2 e = smoothstep(vec2(0.0), vec2(0.003), u) * smoothstep(vec2(0.0), vec2(0.003), 1.0 - u);
        c *= e.x * e.y;
    }
    return c;
}
vec3 imgTile(vec2 p)
{
    vec2 u = vec2(p.x / uImageAspect, p.y) * 0.5 + 0.5;
    return texture(uImage, vec2(u.x, 1.0 - u.y)).rgb;
}

void main()
{
    vec2 sp = (gl_FragCoord.xy - 0.5 * uRes) / (0.5 * uRes.y);
    vec2 p = sp / uZoom;
    float sa = uRes.x / uRes.y;
    float t = uTime;
    float A = uAmount;
    vec3 col = vec3(0.0);

    if (uMode == 0)
    {
        // Liquid: the last frame is pushed along a flow field and the picture
        // is fed back in a little at a time.
        vec2 q = p * 1.6;
        vec2 flow = vec2(fbm(q + vec2(0.0, t * 0.15) + uMidAcc * 0.1),
                         fbm(q + vec2(5.2, -t * 0.13) - uMidAcc * 0.1)) - 0.47;
        flow += 0.22 * vec2(sin(p.y * 4.0 + uBassAcc * 2.0), cos(p.x * 4.0 - uBassAcc * 1.7));
        float push = (0.004 + 0.045 * uBass + 0.015 * uLevel) * A * 2.0;
        vec2 suv = gl_FragCoord.xy / uRes - flow * push * vec2(uRes.y / uRes.x, 1.0);
        vec3 prev = texture(uPrev, suv).rgb;
        vec3 src = img(p + flow * 0.03 * A);
        float k = 0.012 + 0.03 * (1.0 - A) + 0.10 * uBeat * (1.0 - 0.6 * A);
        k = max(k, 1.0 - smoothstep(0.0, 0.15, uSceneAge));
        col = mix(prev, src, clamp(k, 0.0, 1.0));
    }
    else if (uMode == 1)
    {
        // Ripple
        float r = length(p);
        float w = sin(r * 14.0 - uBassAcc * 6.0 - t * 1.5);
        float w2 = sin(r * 31.0 - uMidAcc * 9.0);
        vec2 d = normalize(p + 1e-5) * (w * (0.02 + 0.07 * uBass) + w2 * 0.012 * uMid) * A * 1.6;
        col = img(p + d);
        col *= 1.0 + w * 0.5 * (0.1 + 0.5 * uBass) * A;
    }
    else if (uMode == 2)
    {
        // Swirl
        float r = length(p);
        float a = 1.5 * A * (sin(t * 0.3) * 0.6 + uBass * 1.4 + 0.3 * sin(uMidAcc)) * exp(-r * 1.2);
        vec2 q = rot(a * 2.0) * p;
        q += 0.04 * A * uMid * vec2(sin(q.y * 9.0 + t), cos(q.x * 9.0 - t));
        col = img(q);
    }
    else if (uMode == 3)
    {
        // Kaleido
        float n = floor(4.0 + A * 8.0);
        float a = atan(p.y, p.x) + t * 0.1 + uMidAcc * 0.15;
        float r = length(p);
        float seg = TAU / n;
        a = mod(a, seg);
        a = abs(a - seg * 0.5);
        vec2 q = r * vec2(cos(a), sin(a));
        q = rot(-t * 0.07) * q;
        q *= 1.0 - 0.2 * uBass;
        q += vec2(0.3 * sin(uBassAcc * 0.3 + t * 0.05), 0.2 * cos(uMidAcc * 0.2));
        col = imgTile(q);
    }
    else if (uMode == 4)
    {
        // Tunnel: an endless fall into the picture.
        float T = t * 0.25 + uBassAcc * 0.35 * A;
        float z = fract(T);
        const float R = 2.6;
        for (int i = 0; i < 5; i++)
        {
            float k = z - float(i);
            float sc = 1.3 * pow(R, k);
            vec2 q = rot(k * 0.35 * A) * p / sc;
            vec2 e = vec2(sa, 1.0) - abs(q);
            float inside = min(e.x, e.y);
            float fade = (i == 4) ? z : 1.0;
            float m = (i == 0) ? 1.0 : smoothstep(0.0, 0.05, inside) * fade;
            float shadow = (i == 0) ? 0.0 : smoothstep(-0.35, 0.0, inside) * fade;
            col *= 1.0 - 0.45 * shadow;
            col = mix(col, img(q) * (1.0 + 0.25 * uBeat * A), m);
        }
    }
    else if (uMode == 5)
    {
        // Flyover: the picture becomes the ground under a low camera.
        vec2 uv = sp;
        float horizon = 0.12 + 0.05 * sin(t * 0.2);
        float cy = uv.y - horizon;
        float camH = (0.55 / uZoom) * (1.0 + 0.25 * uBass * A);
        vec3 haze = textureLod(uImage, vec2(0.5 + uv.x * 0.08, 0.35), 6.0).rgb;
        vec3 sky = haze * (0.25 + 0.2 * uLevel) * (1.0 - 0.5 * smoothstep(0.0, 1.0, cy));
        if (cy < 0.0)
        {
            float dist = camH / (-cy);
            vec2 g = vec2(uv.x * dist, dist);
            g = rot(0.25 * sin(t * 0.1)) * g;
            g.y += t * 0.6 + uBassAcc * 0.8 * A;
            g.x += 0.3 * sin(t * 0.13);
            vec3 c = imgTile(g * 0.8);
            col = mix(sky, c, exp(-dist * 0.13));
        }
        else col = sky;
        col += textureLod(uImage, vec2(0.5), 8.0).rgb * 0.7 * exp(-abs(cy) * 16.0) * (0.4 + uBass * A);
    }
    else if (uMode == 6)
    {
        // Relief: brightness becomes height, lit by a light that circles.
        vec2 e = vec2(0.006, 0.0);
        float h0 = luma(img(p)), hx = luma(img(p + e.xy)), hy = luma(img(p + e.yx));
        vec2 g = clamp(vec2(hx - h0, hy - h0) / e.x, -20.0, 20.0);
        float depth = (0.03 + 0.10 * uBass) * A;
        float la = t * 0.5 + uMidAcc;
        vec3 l = normalize(vec3(cos(la), sin(la), 0.7));
        vec3 n = normalize(vec3(-g * depth * 3.0, 1.0));
        vec2 q = p + (h0 - 0.5) * depth * 0.25 * l.xy;
        float diff = dot(n, l) / 0.573;
        float sh = pow(max(dot(reflect(-l, n), vec3(0.0, 0.0, 1.0)), 0.0), 24.0);
        col = img(q) * (0.25 + 0.75 * diff) * (1.0 + sh * (0.3 + 0.9 * uHigh));
    }
    else if (uMode == 7)
    {
        // Glitch
        vec2 q = p;
        float tt = floor(t * 12.0);
        float trig = clamp(max(uBeat, uHigh * 0.8) * A * 1.4, 0.0, 1.0);
        float rows = mix(8.0, 40.0, hash11(tt));
        float by = floor((sp.y * 0.5 + 0.5) * rows);
        float off = (hash21(vec2(by, tt)) - 0.5) * 2.0;
        float on = step(1.0 - trig * 0.9, hash21(vec2(by * 1.7, tt + 3.0)));
        q.x += off * on * 0.35 * trig;
        vec2 blk = floor(sp * vec2(6.0, 4.0));
        float bon = step(1.0 - trig * 0.35, hash21(blk + tt));
        q += (hash22(blk + tt) - 0.5) * 0.5 * bon * trig;
        float ca = 0.003 + 0.03 * trig;
        col = vec3(img(q + vec2(ca, 0.0)).r, img(q).g, img(q - vec2(ca, 0.0)).b);
        col *= 1.0 - 0.1 * A * (0.5 + 0.5 * sin(gl_FragCoord.y * 2.2));
    }
    else if (uMode == 8)
    {
        // Mosaic: dots that swell with the spectrum.
        float cells = mix(90.0, 16.0, A) / (1.0 + 1.2 * uBass * A);
        vec2 g = p * cells * 0.5;
        vec2 id = floor(g);
        vec2 f = fract(g) - 0.5;
        vec3 c = img((id + 0.5) / (cells * 0.5));
        float s = spec(fract(hash21(id) * 0.9));
        float rad = 0.28 + 0.26 * clamp(luma(c) * 0.5 + s, 0.0, 1.0);
        float aa = cells / (uRes.y * uZoom);
        float m = smoothstep(rad + aa, rad - aa, length(f));
        col = mix(c * 0.12, c, m);
    }
    else if (uMode == 9)
    {
        // Echo: zoom streaks that lengthen with the bass.
        vec3 acc = vec3(0.0);
        float wsum = 0.0;
        float str = (0.04 + 0.45 * uBass + 0.15 * uLevel) * A;
        float j = hash21(gl_FragCoord.xy + fract(t) * 91.7);
        for (int i = 0; i < 24; i++)
        {
            float f = (float(i) + j) / 24.0;
            float w = 1.0 - f * 0.7;
            acc += img(p * (1.0 - str * f)) * w;
            wsum += w;
        }
        col = acc / wsum * (1.0 + 0.35 * uBeat * A);
    }
    else if (uMode == 10)
    {
        // Wave: rows slide with the waveform.
        vec2 q = p;
        float y01 = sp.y * 0.5 + 0.5;
        q.x += wave(y01) * 0.3 * A;
        float rows = floor(y01 * 48.0);
        q.x += (spec(rows / 48.0) - 0.2) * 0.12 * A * sin(t + rows);
        q.y += 0.03 * A * sin(p.x * 6.0 + uMidAcc * 3.0) * uMid;
        col = img(q);
    }
    else
    {
        // Prism: six offset copies, each weighted to one slice of the rainbow.
        vec3 acc = vec3(0.0);
        float a0 = t * 0.2 + uMidAcc * 0.2;
        float d = (0.015 + 0.12 * uBass + 0.04 * uHigh) * A;
        for (int i = 0; i < 6; i++)
        {
            float a = a0 + float(i) * TAU / 6.0;
            vec2 o = vec2(cos(a), sin(a)) * d;
            acc += img(p * (1.0 - 0.03 * float(i) * A) + o) * rainbow(float(i) / 6.0);
        }
        col = acc / 3.0;
    }

    fragColor = vec4(max(col, 0.0), 1.0);
}
)GLSL";

//==============================================================================
// MODEL: triangle modes.

static const char* const meshVertex = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec3 aCen;
layout(location = 3) in float aRnd;
uniform mat4 uModel, uViewProj;
uniform float uTime, uBass, uBeat, uBassAcc, uAmount, uShatter;
uniform int uMode;
out vec3 vWorld, vNrm, vBary, vObj;
out float vRnd, vChunk;

float h1(float p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p); }
mat3 rotAxis(vec3 a, float ang)
{
    float c = cos(ang), s = sin(ang), o = 1.0 - c;
    return mat3(o * a.x * a.x + c,       o * a.x * a.y + a.z * s, o * a.z * a.x - a.y * s,
                o * a.x * a.y - a.z * s, o * a.y * a.y + c,       o * a.y * a.z + a.x * s,
                o * a.z * a.x + a.y * s, o * a.y * a.z - a.x * s, o * a.z * a.z + c);
}
void main()
{
    vec3 p = aPos, n = aNrm;
    vChunk = 0.0;
    int corner = gl_VertexID % 3;
    vBary = vec3(float(corner == 0), float(corner == 1), float(corner == 2));

    if (uMode == 1)
    {
        // Triangles are grouped into chunks by a warped grid over their
        // centroids, so the model breaks along curved seams and each chunk
        // tumbles as one piece. A little per-triangle drift crumbles the edges.
        vec3 warped = aCen + 0.11 * sin(aCen.yzx * 6.0 + 1.7) + 0.05 * sin(aCen.zxy * 13.0);
        vec3 cell = floor(warped * 3.2);
        float cr = h1(dot(cell, vec3(1.0, 57.0, 113.0)) + 7.0);
        vec3 pivot = (cell + 0.5) / 3.2;
        float e = uShatter * (0.4 + 1.2 * cr);
        vec3 axis = normalize(vec3(h1(cr * 13.0 + 3.0), h1(cr * 71.0 + 4.0), h1(cr * 29.0 + 5.0)) - 0.5 + 1e-4);
        mat3 R = rotAxis(axis, e * 1.6 * (cr - 0.4));
        vec3 dir = normalize(pivot + (vec3(h1(cr * 91.0), h1(cr * 57.0 + 1.0), h1(cr * 33.0 + 2.0)) - 0.5) * 0.5 + 1e-4);
        p = pivot + R * (aPos - pivot) + dir * e * 0.45;
        p += (aCen - pivot) * e * 0.06 * aRnd;
        n = R * n;
        vChunk = cr;
    }
    else
    {
        float w = sin(aPos.y * 7.0 + uBassAcc * 5.0 + uTime) * 0.5 + 0.5;
        p += n * (uBass * 0.05 * w + uBeat * 0.02) * uAmount;
    }

    vec4 wp = uModel * vec4(p, 1.0);
    vWorld = wp.xyz;
    vNrm = mat3(uModel) * n;
    vObj = aPos;
    vRnd = aRnd;
    gl_Position = uViewProj * wp;
}
)GLSL";

static const char* const meshFragment = R"GLSL(
in vec3 vWorld, vNrm, vBary, vObj;
in float vRnd, vChunk;
out vec4 fragColor;
uniform vec3 uCam;
uniform float uShatter;

// A studio that exists only as a function: soft boxes, a top light, two
// coloured kickers that follow the bass and highs, and a ring of spectrum bars
// so reflections dance with the music.
vec3 env(vec3 d)
{
    d.xz = rot(uTime * 0.1 + uMidAcc * 0.15) * d.xz;
    float az = atan(d.z, d.x);
    vec3 c = mix(vec3(0.012, 0.016, 0.03), vec3(0.09, 0.12, 0.19), smoothstep(-0.3, 0.9, d.y));
    float strip = smoothstep(0.80, 0.95, cos(az * 3.0)) * smoothstep(-0.12, 0.1, d.y) * smoothstep(0.95, 0.6, d.y);
    c += vec3(2.6, 2.5, 2.4) * strip * (0.55 + 0.8 * uLevel);
    c += vec3(1.5) * smoothstep(0.86, 0.98, d.y);
    vec3 k1 = pal(uTime * 0.03, vec3(0.6, 0.4, 0.5), vec3(0.4), vec3(1.0), vec3(0.0, 0.15, 0.35));
    vec3 k2 = pal(uTime * 0.03 + 0.5, vec3(0.6, 0.4, 0.5), vec3(0.4), vec3(1.0), vec3(0.0, 0.15, 0.35));
    c += k1 * 2.2 * smoothstep(0.72, 1.0, dot(d, normalize(vec3(1.0, 0.2, 0.3)))) * (0.25 + 1.5 * uBass * uAmount);
    c += k2 * 2.2 * smoothstep(0.72, 1.0, dot(d, normalize(vec3(-1.0, -0.1, -0.4)))) * (0.25 + 1.5 * uHigh * uAmount);
    float bar = spec(abs(fract(az / TAU * 2.0) - 0.5) * 1.7);
    float inBar = step(abs(d.y + 0.42), bar * 0.28 * (0.4 + uAmount)) * step(0.2, fract(az / TAU * 96.0));
    c += rainbow(az / TAU + uTime * 0.02) * inBar * 1.3;
    return c;
}
float wire(vec3 b, float width)
{
    vec3 d = fwidth(b);
    vec3 a = smoothstep(vec3(0.0), d * width * 1.5, b);
    return 1.0 - min(a.x, min(a.y, a.z));
}
void main()
{
    vec3 n = normalize(vNrm);
    vec3 v = normalize(uCam - vWorld);
    if (dot(n, v) < 0.0) n = -n;
    float nv = max(dot(n, v), 0.0);
    float fres = pow(1.0 - nv, 4.0);
    vec3 r = reflect(-v, n);
    vec3 col;

    if (uMode == 0)
    {
        col = env(r) * mix(vec3(0.80, 0.82, 0.86), vec3(1.0), fres);
    }
    else if (uMode == 1)
    {
        col = env(r) * mix(vec3(0.80, 0.82, 0.86), vec3(1.0), fres) * (0.75 + 0.4 * vChunk);
        col += rainbow(vChunk + uTime * 0.05) * fres * clamp(uShatter * 2.5, 0.0, 1.0) * 0.9;
    }
    else if (uMode == 3)
    {
        float scan = 0.5 + 0.5 * sin(vWorld.y * 90.0 - uTime * 6.0);
        float band = smoothstep(0.92, 1.0, sin(vWorld.y * 3.0 - uTime * 1.3 - uBassAcc * 2.0) * 0.5 + 0.5);
        float edge = wire(vBary, 1.0);
        vec3 base = vec3(0.12, 0.7, 1.0);
        float a = fres * 1.2 + 0.07 + edge * 0.3 * (0.4 + uHigh) + band * 0.6;
        a *= 0.55 + 0.45 * scan;
        a *= 0.55 + 0.8 * uLevel * uAmount + 0.5 * uBeat * uAmount;
        col = base * a + vec3(1.0) * band * fres;
    }
    else if (uMode == 4)
    {
        float edge = wire(vBary, 1.2);
        float s = spec(clamp(vObj.y * 0.5 + 0.5, 0.0, 1.0));
        vec3 c = pal(vObj.y * 0.35 + uTime * 0.05 + uBassAcc * 0.05, vec3(0.55, 0.45, 0.5), vec3(0.45), vec3(1.0), vec3(0.0, 0.2, 0.45));
        col = c * edge * (0.35 + 2.4 * s * uAmount + 0.6 * uBeat * uAmount) + c * 0.015;
    }
    else
    {
        vec3 film = rainbow(nv * 1.4 + dot(n, vec3(0.3, 0.5, 0.2)) * 0.6 + uTime * 0.05 + uMidAcc * 0.08);
        col = env(r) * 0.45 + film * (0.3 + 0.7 * fres) * (0.7 + 0.7 * uLevel * uAmount);
        col *= 0.35 + 0.65 * smoothstep(-0.4, 0.9, dot(n, normalize(vec3(0.4, 0.8, 0.5))));
    }

    // Hologram and Wire are made of light; the other surfaces are solid.
    fragColor = vec4(max(col, 0.0), (uMode == 3 || uMode == 4) ? 0.0 : 1.0);
}
)GLSL";

//==============================================================================
// MODEL: dust mode (surface samples drawn as points).

static const char* const dustVertex = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in float aRnd;
uniform mat4 uModel, uViewProj;
uniform float uTime, uBass, uHigh, uLevel, uBeat, uAmount, uPointScale;
out vec3 vCol;

vec3 swirl(vec3 p) { return vec3(sin(p.y * 3.1 + p.z * 1.7), sin(p.z * 2.9 + p.x * 2.3), sin(p.x * 3.3 + p.y * 1.9)); }
void main()
{
    float t = uTime * 0.6;
    vec3 p = aPos;
    float e = uAmount * (0.03 + 0.45 * uBass + 0.25 * uBeat) * (0.3 + aRnd);
    vec3 sw = swirl(p * 2.0 + t + aRnd * 6.0) + 0.5 * swirl(p * 5.0 - t * 1.3);
    p += aNrm * e * 0.6 + sw * e * 0.5;
    p += step(0.93, aRnd) * sw * 0.25 * uAmount;

    vec4 wp = uModel * vec4(p, 1.0);
    vec4 cp = uViewProj * wp;
    gl_Position = cp;
    gl_PointSize = max(1.0, uPointScale * (0.6 + 1.4 * fract(aRnd * 17.0)) * (1.0 + 1.5 * uHigh * fract(aRnd * 31.0)) / cp.w);

    float lit = 0.35 + 0.65 * max(dot(normalize(mat3(uModel) * aNrm), normalize(vec3(0.4, 0.7, 0.6))), 0.0);
    vec3 c = vec3(0.6, 0.5, 0.45) + vec3(0.4, 0.4, 0.45) * cos(6.28318 * (aRnd * 0.25 + aPos.y * 0.2 + uTime * 0.03 + vec3(0.05, 0.15, 0.3)));
    vCol = c * lit * (0.5 + 0.5 * fract(aRnd * 7.0)) * (0.7 + 0.8 * uLevel);
}
)GLSL";

static const char* const dustFragment = R"GLSL(#version 330 core
in vec3 vCol;
out vec4 fragColor;
void main()
{
    vec2 c = gl_PointCoord - 0.5;
    float d = dot(c, c) * 4.0;
    if (d > 1.0) discard;
    fragColor = vec4(vCol * (1.0 - d) * 0.5, 1.0);
}
)GLSL";

static const char* const modelBackground = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
void main()
{
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y;
    float r = length(p);
    vec3 c = vec3(0.010, 0.012, 0.018) * (1.0 - r * 0.8);
    c += vec3(0.045, 0.055, 0.085) * exp(-r * r * 5.0) * (0.3 + 0.9 * uLevel * uAmount);
    fragColor = vec4(max(c, 0.0), 0.0);
}
)GLSL";

//==============================================================================
// Generative scenes.

static const char* const neonTunnel = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
void main()
{
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y / uZoom;
    p *= rot(0.2 * sin(uTime * 0.2) + uMidAcc * 0.05);
    float r = length(p) + 1e-4;
    float a = atan(p.y, p.x);
    float t = uTime * 0.6 + uBassAcc * 1.5;
    vec3 col = vec3(0.0);

    // Light beams: each one listens to its own slice of the spectrum.
    const float beams = 28.0;
    float ai = (a / TAU + 0.5) * beams;
    float id = floor(ai);
    float af = fract(ai) - 0.5;
    float s = spec(hash11(id + 3.0) * 0.85);
    float width = 0.03 + 0.25 * s * uAmount;
    float beam = exp(-af * af / (width * width));
    vec3 bc = rainbow(id / beams + uTime * 0.03);
    col += bc * beam * (0.12 + 1.6 * s) * smoothstep(0.02, 0.35, r);

    // Rings rushing past.
    float d = 0.35 / r + t;
    float rf = fract(d) - 0.5;
    float ring = exp(-rf * rf * 90.0);
    float rs = spec(hash11(floor(d)) * 0.6);
    col += rainbow(floor(d) * 0.13 + 0.4) * ring * (0.15 + 1.2 * rs * uAmount + 0.6 * uBeat) * smoothstep(0.0, 0.25, r);

    // The far end glows with the kick.
    col += vec3(0.6, 0.8, 1.0) * 0.012 / (r * r + 0.012) * (0.25 + 1.2 * uBass);
    col *= 1.0 - 0.4 * smoothstep(0.5, 1.2, r);
    fragColor = vec4(col, 1.0);
}
)GLSL";

static const char* const inkFlow = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
void main()
{
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y * 2.2 / uZoom;
    float t = uTime * 0.12 + uMidAcc * 0.25;
    vec2 q = vec2(fbm(p + vec2(0.0, t)), fbm(p + vec2(5.2, 1.3 - t)));
    vec2 r = vec2(fbm(p + 4.0 * q + vec2(1.7, 9.2) + 0.5 * t), fbm(p + 4.0 * q + vec2(8.3, 2.8) - 0.4 * t));
    r += uAmount * uBass * 0.3 * vec2(sin(p.y * 3.0 + t * 4.0), cos(p.x * 3.0 - t * 3.0));
    float f = fbm(p + 4.0 * r);

    vec3 col = mix(vec3(0.02, 0.03, 0.08), vec3(0.08, 0.42, 0.72), clamp(f * f * 4.0, 0.0, 1.0));
    col = mix(col, vec3(0.92, 0.22, 0.48), clamp(length(q), 0.0, 1.0) * 0.6);
    col = mix(col, vec3(1.0, 0.85, 0.6), clamp(r.x * r.x * 1.3, 0.0, 1.0) * (0.2 + 0.5 * uHigh));
    float vein = abs(sin(f * 18.0 + uBassAcc * 2.0));
    col += vec3(1.0, 0.95, 0.9) * pow(1.0 - vein, 14.0) * (0.06 + 0.3 * uMid * uAmount);
    col *= 0.22 + 0.75 * f * f * 2.0 + 0.12 * uBeat * uAmount;
    fragColor = vec4(col, 1.0);
}
)GLSL";

static const char* const kaleidoBloom = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
void main()
{
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y * 2.0 / uZoom;
    float t = uTime * 0.2;
    float n = 6.0 + 2.0 * floor(mod(uBeatCount / 8.0, 3.0));
    float r0 = length(p);
    float a = atan(p.y, p.x) + t * 0.3 + uMidAcc * 0.1;
    float seg = TAU / n;
    a = mod(a, seg);
    a = abs(a - seg * 0.5);
    p = r0 * vec2(cos(a), sin(a));

    vec3 col = vec3(0.0);
    float sc = 1.0;
    for (int i = 0; i < 6; i++)
    {
        float fi = float(i);
        p = abs(p) - vec2(0.55 + 0.15 * sin(t + fi), 0.3 + 0.2 * uBass * uAmount);
        p *= rot(t * 0.4 + fi * 0.6 + uHighAcc * 0.05);
        p *= 1.35;
        sc *= 1.35;
        float d = min(abs(p.x), abs(p.y)) / sc;
        vec3 c = pal(fi * 0.13 + r0 * 0.4 - t + uBassAcc * 0.1, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.2, 0.45));
        col += c * (0.0035 / (d + 0.0025)) * (0.3 + 1.6 * spec(fi / 6.0 + 0.05) * uAmount);
    }
    col *= smoothstep(2.4, 0.3, r0);
    col += vec3(0.08, 0.07, 0.1) * uBeat * uAmount;
    fragColor = vec4(col, 1.0);
}
)GLSL";

static const char* const liquidMetal = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
float smin(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0); return mix(b, a, h) - k * h * (1.0 - h); }
float map(vec3 p)
{
    float t = uTime * 0.5;
    float d = length(p) - 1.0;
    float disp = sin(p.x * 3.0 + t * 2.0 + uBassAcc * 3.0) * sin(p.y * 3.4 - t * 1.7) * sin(p.z * 2.8 + t * 1.3);
    d += disp * (0.10 + 0.32 * uBass * uAmount);
    d += 0.035 * sin(p.y * 12.0 + uHighAcc * 8.0) * uHigh * uAmount;
    for (int i = 0; i < 3; i++)
    {
        float fi = float(i);
        vec3 c = 1.55 * vec3(sin(t * (1.0 + fi * 0.3) + fi * 2.1), cos(t * (0.8 + fi * 0.2) + fi), sin(t * 0.6 + fi * 4.0));
        d = smin(d, length(p - c) - (0.3 + 0.28 * spec(0.1 + 0.25 * fi) * uAmount), 0.6);
    }
    return d * 0.65;
}
vec3 env(vec3 d)
{
    d.xz = rot(uTime * 0.07) * d.xz;
    float az = atan(d.z, d.x);
    vec3 c = mix(vec3(0.015, 0.02, 0.04), vec3(0.10, 0.14, 0.22), smoothstep(-0.3, 0.9, d.y));
    float strip = smoothstep(0.80, 0.95, cos(az * 3.0)) * smoothstep(-0.1, 0.1, d.y) * smoothstep(0.95, 0.6, d.y);
    c += vec3(2.6, 2.5, 2.4) * strip;
    c += vec3(1.4) * smoothstep(0.86, 0.98, d.y);
    c += pal(uTime * 0.04, vec3(0.6, 0.4, 0.5), vec3(0.4), vec3(1.0), vec3(0.0, 0.15, 0.35)) * 2.0
         * smoothstep(0.7, 1.0, dot(d, normalize(vec3(1.0, -0.3, 0.2)))) * (0.3 + 1.5 * uBass);
    c += pal(uTime * 0.04 + 0.5, vec3(0.6, 0.4, 0.5), vec3(0.4), vec3(1.0), vec3(0.0, 0.15, 0.35)) * 2.0
         * smoothstep(0.7, 1.0, dot(d, normalize(vec3(-1.0, -0.2, -0.3)))) * (0.3 + 1.5 * uHigh);
    return c;
}
void main()
{
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y;
    vec3 ro = vec3(0.0, 0.0, 4.6 / uZoom);
    vec3 rd = normalize(vec3(p, -1.5));
    float ca = uTime * 0.15;
    ro.xz = rot(ca) * ro.xz;
    rd.xz = rot(ca) * rd.xz;

    float tt = 0.0;
    bool hit = false;
    for (int i = 0; i < 80; i++)
    {
        float d = map(ro + rd * tt);
        if (d < 0.002) { hit = true; break; }
        tt += d;
        if (tt > 14.0) break;
    }

    vec3 col = env(rd) * 0.04;
    if (hit)
    {
        vec3 pos = ro + rd * tt;
        vec2 e = vec2(0.003, 0.0);
        vec3 n = normalize(vec3(map(pos + e.xyy) - map(pos - e.xyy), map(pos + e.yxy) - map(pos - e.yxy), map(pos + e.yyx) - map(pos - e.yyx)));
        float fres = pow(1.0 - max(dot(n, -rd), 0.0), 4.0);
        col = env(reflect(rd, n)) * mix(vec3(0.78, 0.80, 0.85), vec3(1.0), fres);
    }
    fragColor = vec4(col, 1.0);
}
)GLSL";

static const char* const fractalDive = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
vec4 trap;
float map(vec3 p)
{
    float s = 1.0;
    trap = vec4(1000.0);
    float k0 = 1.22 + 0.10 * sin(uTime * 0.07) + 0.10 * uBass * uAmount;
    for (int i = 0; i < 7; i++)
    {
        p = -1.0 + 2.0 * fract(0.5 * p + 0.5);
        float r2 = dot(p, p);
        trap = min(trap, vec4(abs(p), r2));
        float k = k0 / r2;
        p *= k;
        s *= k;
    }
    return 0.25 * abs(p.y) / s;
}
void main()
{
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y;
    float t = uTime * 0.12 + uBassAcc * 0.08 * uAmount;
    vec3 ro = vec3(1.0 + 0.35 * sin(t * 0.7), 0.5 + 0.3 * cos(t * 0.53), t * 1.2);
    vec3 fw = normalize(vec3(0.35 * cos(t * 0.7), -0.25 * sin(t * 0.53), 1.0));
    vec3 rt = normalize(cross(vec3(0.0, 1.0, 0.0), fw));
    vec3 up = cross(fw, rt);
    float roll = 0.3 * sin(t * 0.4);
    vec2 pr = rot(roll) * p;
    vec3 rd = normalize(fw * (1.2 * uZoom) + rt * pr.x + up * pr.y);

    float tt = 0.01, glow = 0.0;
    vec4 tr = vec4(0.0);
    bool hit = false;
    for (int i = 0; i < 90; i++)
    {
        float d = map(ro + rd * tt);
        glow += exp(-d * 40.0);
        if (d < 0.0006 * tt + 0.0002) { hit = true; tr = trap; break; }
        tt += d;
        if (tt > 6.0) break;
    }

    vec3 col = vec3(0.0);
    if (hit)
    {
        vec3 pos = ro + rd * tt;
        vec2 e = vec2(0.0008 * tt + 0.0003, 0.0);
        vec3 n = normalize(vec3(map(pos + e.xyy) - map(pos - e.xyy), map(pos + e.yxy) - map(pos - e.yxy), map(pos + e.yyx) - map(pos - e.yyx)));
        vec3 base = pal(tr.w * 0.6 + tr.x * 0.5 + uTime * 0.02, vec3(0.5, 0.45, 0.4), vec3(0.45, 0.4, 0.45), vec3(1.0), vec3(0.0, 0.12, 0.3));
        float dif = clamp(dot(n, normalize(vec3(0.5, 0.8, -0.4))), 0.0, 1.0);
        float rim = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
        col = base * (0.14 + 0.8 * dif) + base.zxy * rim * (0.25 + 0.7 * uHigh * uAmount);
        col += vec3(1.0, 0.8, 0.5) * smoothstep(0.03, 0.0, tr.y) * (0.1 + 0.7 * uMid * uAmount);
    }
    col *= exp(-tt * 0.3);
    col += vec3(0.25, 0.45, 0.9) * min(glow, 40.0) * 0.0022 * (0.4 + 1.6 * uBass * uAmount + 0.8 * uBeat);
    fragColor = vec4(col, 1.0);
}
)GLSL";

static const char* const spectrumScene = R"GLSL(
in vec2 vUv;
out vec4 fragColor;
void main()
{
    vec2 uv = gl_FragCoord.xy / uRes;
    vec3 col = vec3(0.008, 0.010, 0.016);

    // Faint grid
    vec2 g = abs(fract(uv * vec2(16.0, 9.0)) - 0.5);
    col += vec3(0.02, 0.025, 0.035) * smoothstep(0.48, 0.5, max(g.x, g.y));

    // Bars with a reflection underneath
    const float n = 72.0;
    float margin = 0.04;
    float x = (uv.x - margin) / (1.0 - 2.0 * margin);
    if (x > 0.0 && x < 1.0)
    {
        float id = floor(x * n);
        float fx = fract(x * n);
        float s = spec((id + 0.5) / n);
        float base = 0.36;
        float h = s * 0.42 * (0.5 + uAmount);
        float y = uv.y - base;
        float bar = step(0.14, fx) * step(fx, 0.86);
        vec3 bc = pal(id / n * 0.75 + uTime * 0.02, vec3(0.55, 0.5, 0.5), vec3(0.45), vec3(1.0), vec3(0.0, 0.2, 0.5));
        if (y >= 0.0) col += bc * bar * step(y, h) * (0.55 + 0.7 * y / max(h, 1e-3));
        else          col += bc * bar * step(-y * 4.0, h) * 0.22 * (1.0 + y / base);
        col += bc * bar * 0.9 * step(abs(y - h - 0.006), 0.003);
    }

    // Scope
    float w = wave(uv.x) * 0.09 * (0.5 + uAmount);
    float d = abs(uv.y - 0.88 - w);
    col += vec3(0.5, 0.9, 1.0) * (0.0018 / (d + 0.0012)) * (0.6 + 0.6 * uLevel);
    fragColor = vec4(col, 1.0);
}
)GLSL";

} // namespace shaders

#include "ModelLoader.h"

#include "cgltf.h"
#include "ufbx.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <unordered_map>

namespace
{
//==============================================================================
struct V3
{
    float x = 0, y = 0, z = 0;
};

inline V3 operator+ (V3 a, V3 b) noexcept     { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator- (V3 a, V3 b) noexcept     { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator* (V3 a, float s) noexcept  { return { a.x * s, a.y * s, a.z * s }; }
inline float dot (V3 a, V3 b) noexcept        { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross (V3 a, V3 b) noexcept         { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float length (V3 a) noexcept           { return std::sqrt (dot (a, a)); }
inline bool finite (V3 a) noexcept            { return std::isfinite (a.x) && std::isfinite (a.y) && std::isfinite (a.z); }

inline V3 normalised (V3 a, V3 fallback = { 0, 1, 0 }) noexcept
{
    const float l = length (a);
    return l > 1.0e-20f && std::isfinite (l) ? a * (1.0f / l) : fallback;
}

/** Unindexed triangles, three corners each. A zero normal means "not supplied". */
struct Soup
{
    std::vector<V3> pos, nrm;

    void add (V3 a, V3 b, V3 c)
    {
        pos.push_back (a); pos.push_back (b); pos.push_back (c);
        nrm.resize (pos.size());
    }

    void add (V3 a, V3 b, V3 c, V3 na, V3 nb, V3 nc)
    {
        pos.push_back (a); pos.push_back (b); pos.push_back (c);
        nrm.push_back (na); nrm.push_back (nb); nrm.push_back (nc);
    }

    size_t numTriangles() const noexcept   { return pos.size() / 3; }
};

//==============================================================================
struct TextCursor
{
    const char* p;
    const char* end;

    static bool isBlank (char c) noexcept    { return c == ' ' || c == '\t' || c == '\r'; }
    static bool isSpace (char c) noexcept    { return isBlank (c) || c == '\n'; }

    void skipBlanks() noexcept               { while (p < end && isBlank (*p)) ++p; }
    void skipSpaces() noexcept               { while (p < end && isSpace (*p)) ++p; }
    void skipLine() noexcept                 { while (p < end && *p != '\n') ++p; if (p < end) ++p; }
    bool atLineEnd() const noexcept          { return p >= end || *p == '\n'; }

    template <typename Number>
    bool read (Number& out) noexcept
    {
        if (p < end && *p == '+')
            ++p;

        const auto r = std::from_chars (p, end, out);

        if (r.ec != std::errc())
            return false;

        p = r.ptr;
        return true;
    }

    /** Decimal numbers are read by hand: the Mac standard library only has
        std::from_chars for floating point from macOS 26 on, and strtod needs
        a terminated string and follows the locale. */
    bool read (double& out) noexcept
    {
        const auto isDigit = [] (char c) noexcept { return c >= '0' && c <= '9'; };

        const char* s = p;
        bool negative = false;

        if (s < end && (*s == '+' || *s == '-'))
            negative = (*s++ == '-');

        uint64_t mantissa = 0;
        int digits = 0, scale = 0;
        bool any = false;

        for (; s < end && isDigit (*s); ++s)
        {
            any = true;

            if (digits < 19)
            {
                mantissa = mantissa * 10 + (uint64_t) (*s - '0');
                digits += mantissa != 0 ? 1 : 0;
            }
            else
            {
                ++scale;
            }
        }

        if (s < end && *s == '.')
        {
            for (++s; s < end && isDigit (*s); ++s)
            {
                any = true;

                if (digits < 19)
                {
                    mantissa = mantissa * 10 + (uint64_t) (*s - '0');
                    digits += mantissa != 0 ? 1 : 0;
                    --scale;
                }
            }
        }

        if (! any)
            return false;

        if (s < end && (*s == 'e' || *s == 'E'))
        {
            const char* e = s + 1;
            bool negativeExponent = false;

            if (e < end && (*e == '+' || *e == '-'))
                negativeExponent = (*e++ == '-');

            if (e < end && isDigit (*e))
            {
                int exponent = 0;

                for (; e < end && isDigit (*e); ++e)
                    exponent = std::min (exponent * 10 + (*e - '0'), 100000);

                scale += negativeExponent ? -exponent : exponent;
                s = e;
            }
        }

        double value = (double) mantissa;

        if (mantissa != 0 && scale != 0)
            value *= std::pow (10.0, (double) scale);

        if (! std::isfinite (value))
            return false;

        out = negative ? -value : value;
        p = s;
        return true;
    }

    bool read (float& out) noexcept
    {
        const char* start = p;
        double value = 0.0;

        if (! read (value))
            return false;

        if (std::abs (value) > (double) std::numeric_limits<float>::max())
        {
            p = start;
            return false;
        }

        out = (float) value;
        return true;
    }

    template <typename Number>
    bool readOnLine (Number& out) noexcept   { skipBlanks(); return read (out); }

    template <typename Number>
    bool readAnywhere (Number& out) noexcept { skipSpaces(); return read (out); }

    std::string readToken()
    {
        skipSpaces();
        const char* start = p;
        while (p < end && ! isSpace (*p)) ++p;
        return std::string (start, p);
    }
};

//==============================================================================
bool loadObj (const juce::MemoryBlock& data, Soup& soup, juce::String& error)
{
    std::vector<V3> v, vn;
    std::vector<int> fv, fn;

    TextCursor c { (const char*) data.getData(), (const char*) data.getData() + data.getSize() };

    auto startsWith = [&] (const char* word, size_t len)
    {
        return (size_t) (c.end - c.p) > len && std::memcmp (c.p, word, len) == 0 && TextCursor::isBlank (c.p[len]);
    };

    while (c.p < c.end)
    {
        c.skipBlanks();

        if (startsWith ("v", 1))
        {
            c.p += 2;
            V3 a;

            if (c.readOnLine (a.x) && c.readOnLine (a.y) && c.readOnLine (a.z))
                v.push_back (a);
            else
                v.push_back ({});
        }
        else if (startsWith ("vn", 2))
        {
            c.p += 3;
            V3 a;
            c.readOnLine (a.x); c.readOnLine (a.y); c.readOnLine (a.z);
            vn.push_back (a);
        }
        else if (startsWith ("f", 1))
        {
            c.p += 2;
            fv.clear();
            fn.clear();

            for (;;)
            {
                c.skipBlanks();

                if (c.atLineEnd())
                    break;

                int vi = 0, ti = 0, ni = 0;

                if (! c.read (vi))
                    break;

                if (c.p < c.end && *c.p == '/')
                {
                    ++c.p;

                    if (c.p < c.end && *c.p != '/')
                        c.read (ti);

                    if (c.p < c.end && *c.p == '/')
                    {
                        ++c.p;
                        c.read (ni);
                    }
                }

                vi = vi > 0 ? vi - 1 : (int) v.size() + vi;
                ni = ni > 0 ? ni - 1 : (ni < 0 ? (int) vn.size() + ni : -1);

                if (vi < 0 || vi >= (int) v.size())
                {
                    fv.clear();
                    break;
                }

                fv.push_back (vi);
                fn.push_back (ni >= 0 && ni < (int) vn.size() ? ni : -1);
            }

            for (size_t i = 1; i + 1 < fv.size(); ++i)
            {
                const size_t k[3] = { 0, i, i + 1 };

                if (fn[k[0]] >= 0 && fn[k[1]] >= 0 && fn[k[2]] >= 0)
                    soup.add (v[(size_t) fv[k[0]]], v[(size_t) fv[k[1]]], v[(size_t) fv[k[2]]],
                              vn[(size_t) fn[k[0]]], vn[(size_t) fn[k[1]]], vn[(size_t) fn[k[2]]]);
                else
                    soup.add (v[(size_t) fv[k[0]]], v[(size_t) fv[k[1]]], v[(size_t) fv[k[2]]]);
            }
        }

        c.skipLine();
    }

    if (soup.numTriangles() == 0)
    {
        error = "No faces found in OBJ file";
        return false;
    }

    return true;
}

//==============================================================================
bool loadStl (const juce::MemoryBlock& data, Soup& soup, juce::String& error)
{
    const auto* bytes = (const uint8_t*) data.getData();
    const size_t size = data.getSize();

    auto readBinary = [&] (uint32_t count)
    {
        soup.pos.reserve ((size_t) count * 3);

        for (uint32_t i = 0; i < count; ++i)
        {
            float f[9];
            std::memcpy (f, bytes + 84 + (size_t) i * 50 + 12, sizeof (f));
            soup.add ({ f[0], f[1], f[2] }, { f[3], f[4], f[5] }, { f[6], f[7], f[8] });
        }
    };

    uint32_t count = 0;

    if (size >= 84)
    {
        std::memcpy (&count, bytes + 80, 4);

        if (84 + (uint64_t) count * 50 == size && count > 0)
        {
            readBinary (count);
            return true;
        }
    }

    // ASCII: every "vertex" keyword is followed by three numbers.
    {
        TextCursor c { (const char*) bytes, (const char*) bytes + size };
        static const char word[] = "vertex";
        V3 tri[3];
        int n = 0;

        for (;;)
        {
            const char* hit = std::search (c.p, c.end, word, word + 6);

            if (hit == c.end)
                break;

            c.p = hit + 6;
            V3 a;

            if (c.readOnLine (a.x) && c.readOnLine (a.y) && c.readOnLine (a.z))
            {
                tri[n++] = a;

                if (n == 3)
                {
                    soup.add (tri[0], tri[1], tri[2]);
                    n = 0;
                }
            }
        }
    }

    if (soup.numTriangles() > 0)
        return true;

    // A binary file whose header count disagrees with its length.
    if (size > 84 + 50)
    {
        readBinary ((uint32_t) juce::jmin ((uint64_t) count, (uint64_t) ((size - 84) / 50)));

        if (soup.numTriangles() > 0)
            return true;
    }

    error = "No triangles found in STL file";
    return false;
}

//==============================================================================
namespace ply
{
    enum Type { i8, u8, i16, u16, i32, u32, f32, f64, invalid };

    Type parseType (const std::string& s)
    {
        if (s == "char"   || s == "int8")    return i8;
        if (s == "uchar"  || s == "uint8")   return u8;
        if (s == "short"  || s == "int16")   return i16;
        if (s == "ushort" || s == "uint16")  return u16;
        if (s == "int"    || s == "int32")   return i32;
        if (s == "uint"   || s == "uint32")  return u32;
        if (s == "float"  || s == "float32") return f32;
        if (s == "double" || s == "float64") return f64;
        return invalid;
    }

    int sizeOf (Type t)
    {
        switch (t)
        {
            case i8: case u8:    return 1;
            case i16: case u16:  return 2;
            case i32: case u32: case f32: return 4;
            case f64:            return 8;
            case invalid:        break;
        }

        return 0;
    }

    struct Property
    {
        std::string name;
        Type type = invalid;
        bool isList = false;
        Type countType = invalid;
    };

    struct Element
    {
        std::string name;
        size_t count = 0;
        std::vector<Property> props;
    };

    struct Reader
    {
        TextCursor c;
        bool ascii = true, bigEndian = false, failed = false;

        double read (Type t)
        {
            if (ascii)
            {
                double d = 0.0;

                if (! c.readAnywhere (d))
                    failed = true;

                return d;
            }

            const int n = sizeOf (t);

            if (n == 0 || c.end - c.p < n)
            {
                failed = true;
                return 0.0;
            }

            uint8_t b[8];
            std::memcpy (b, c.p, (size_t) n);
            c.p += n;

            if (bigEndian)
                std::reverse (b, b + n);

            switch (t)
            {
                case i8:   { int8_t v;   std::memcpy (&v, b, 1); return v; }
                case u8:   { uint8_t v;  std::memcpy (&v, b, 1); return v; }
                case i16:  { int16_t v;  std::memcpy (&v, b, 2); return v; }
                case u16:  { uint16_t v; std::memcpy (&v, b, 2); return v; }
                case i32:  { int32_t v;  std::memcpy (&v, b, 4); return v; }
                case u32:  { uint32_t v; std::memcpy (&v, b, 4); return v; }
                case f32:  { float v;    std::memcpy (&v, b, 4); return v; }
                case f64:  { double v;   std::memcpy (&v, b, 8); return v; }
                case invalid: break;
            }

            return 0.0;
        }
    };
}

bool loadPly (const juce::MemoryBlock& data, Soup& soup, juce::String& error)
{
    using namespace ply;

    const char* begin = (const char*) data.getData();
    const char* end = begin + data.getSize();

    if (data.getSize() < 4 || std::memcmp (begin, "ply", 3) != 0)
    {
        error = "Not a PLY file";
        return false;
    }

    Reader reader;
    std::vector<Element> elements;
    TextCursor c { begin, end };
    bool headerDone = false;

    while (c.p < c.end && ! headerDone)
    {
        const char* lineEnd = std::find (c.p, c.end, '\n');
        TextCursor line { c.p, lineEnd };
        const auto word = line.readToken();

        if (word == "format")
        {
            const auto fmt = line.readToken();
            reader.ascii = fmt == "ascii";
            reader.bigEndian = fmt == "binary_big_endian";
        }
        else if (word == "element")
        {
            Element e;
            e.name = line.readToken();
            unsigned long long n = 0;
            line.readAnywhere (n);
            e.count = (size_t) n;
            elements.push_back (e);
        }
        else if (word == "property" && ! elements.empty())
        {
            Property p;
            const auto t = line.readToken();

            if (t == "list")
            {
                p.isList = true;
                p.countType = parseType (line.readToken());
                p.type = parseType (line.readToken());
            }
            else
            {
                p.type = parseType (t);
            }

            p.name = line.readToken();
            elements.back().props.push_back (p);
        }
        else if (word == "end_header")
        {
            headerDone = true;
        }

        c.p = lineEnd < c.end ? lineEnd + 1 : c.end;
    }

    if (! headerDone)
    {
        error = "PLY header is incomplete";
        return false;
    }

    reader.c = { c.p, end };

    std::vector<V3> verts, normals;
    std::vector<uint32_t> face;
    bool sawFaces = false;

    for (auto& e : elements)
    {
        if (e.name == "vertex")
        {
            int ix = -1, iy = -1, iz = -1, inx = -1, iny = -1, inz = -1;

            for (int i = 0; i < (int) e.props.size(); ++i)
            {
                const auto& n = e.props[(size_t) i].name;
                if (n == "x") ix = i; else if (n == "y") iy = i; else if (n == "z") iz = i;
                else if (n == "nx") inx = i; else if (n == "ny") iny = i; else if (n == "nz") inz = i;
            }

            if (ix < 0 || iy < 0 || iz < 0)
            {
                error = "PLY vertices have no x/y/z";
                return false;
            }

            const bool hasNormals = inx >= 0 && iny >= 0 && inz >= 0;
            verts.reserve (juce::jmin (e.count, (size_t) 50'000'000));
            std::vector<double> row (e.props.size());

            for (size_t i = 0; i < e.count && ! reader.failed; ++i)
            {
                for (size_t k = 0; k < e.props.size(); ++k)
                {
                    const auto& p = e.props[k];

                    if (p.isList)
                    {
                        const auto n = (int) reader.read (p.countType);
                        for (int j = 0; j < n && ! reader.failed; ++j) reader.read (p.type);
                        row[k] = 0.0;
                    }
                    else
                    {
                        row[k] = reader.read (p.type);
                    }
                }

                if (reader.failed)
                    break;

                verts.push_back ({ (float) row[(size_t) ix], (float) row[(size_t) iy], (float) row[(size_t) iz] });

                if (hasNormals)
                    normals.push_back ({ (float) row[(size_t) inx], (float) row[(size_t) iny], (float) row[(size_t) inz] });
            }
        }
        else
        {
            const bool isFace = e.name == "face";

            for (size_t i = 0; i < e.count && ! reader.failed; ++i)
            {
                for (auto& p : e.props)
                {
                    if (! p.isList)
                    {
                        reader.read (p.type);
                        continue;
                    }

                    const auto n = (int) reader.read (p.countType);
                    const bool isIndices = isFace && (p.name == "vertex_indices" || p.name == "vertex_index");
                    face.clear();

                    for (int j = 0; j < n && ! reader.failed; ++j)
                    {
                        const double d = reader.read (p.type);

                        if (isIndices)
                            face.push_back ((uint32_t) d);
                    }

                    if (! isIndices || reader.failed)
                        continue;

                    sawFaces = true;

                    for (size_t j = 1; j + 1 < face.size(); ++j)
                    {
                        const uint32_t a = face[0], b = face[j], d = face[j + 1];

                        if (a >= verts.size() || b >= verts.size() || d >= verts.size())
                            continue;

                        if (normals.size() == verts.size())
                            soup.add (verts[a], verts[b], verts[d], normals[a], normals[b], normals[d]);
                        else
                            soup.add (verts[a], verts[b], verts[d]);
                    }
                }
            }
        }

        if (reader.failed)
            break;
    }

    if (soup.numTriangles() == 0 && ! sawFaces && ! verts.empty())
    {
        // A point cloud: give every point a tiny triangle so each mode has
        // something to draw. The dust mode is the natural fit for these.
        V3 lo = verts[0], hi = verts[0];

        for (auto& v : verts)
        {
            if (! finite (v)) continue;
            lo = { juce::jmin (lo.x, v.x), juce::jmin (lo.y, v.y), juce::jmin (lo.z, v.z) };
            hi = { juce::jmax (hi.x, v.x), juce::jmax (hi.y, v.y), juce::jmax (hi.z, v.z) };
        }

        const float s = length (hi - lo) * 0.004f;
        const size_t step = juce::jmax ((size_t) 1, verts.size() / 500'000);
        const bool hasNormals = normals.size() == verts.size();

        for (size_t i = 0; i < verts.size(); i += step)
        {
            const V3 n = hasNormals ? normalised (normals[i]) : normalised (V3 { std::sin ((float) i * 12.9898f), std::cos ((float) i * 78.233f), std::sin ((float) i * 37.719f) });
            const V3 t = normalised (cross (n, std::abs (n.y) < 0.9f ? V3 { 0, 1, 0 } : V3 { 1, 0, 0 }));
            const V3 b = cross (n, t);
            const V3 p = verts[i];
            soup.add (p + t * s, p + (t * -0.5f + b * 0.866f) * s, p + (t * -0.5f + b * -0.866f) * s, n, n, n);
        }
    }

    if (soup.numTriangles() == 0)
    {
        error = reader.failed ? "PLY data ended unexpectedly" : "No geometry found in PLY file";
        return false;
    }

    return true;
}

//==============================================================================
bool loadGltf (const juce::File& file, const juce::MemoryBlock& data, Soup& soup, juce::String& error)
{
    cgltf_options options {};
    cgltf_data* gltf = nullptr;

    if (cgltf_parse (&options, data.getData(), data.getSize(), &gltf) != cgltf_result_success || gltf == nullptr)
    {
        error = "Could not parse glTF file";
        return false;
    }

    if (cgltf_load_buffers (&options, gltf, file.getFullPathName().toRawUTF8()) != cgltf_result_success)
    {
        cgltf_free (gltf);
        error = "Could not load glTF buffers (is the .bin file next to the .gltf?)";
        return false;
    }

    bool sawCompressed = false;
    std::vector<float> positions, normals;

    for (cgltf_size ni = 0; ni < gltf->nodes_count; ++ni)
    {
        cgltf_node* node = &gltf->nodes[ni];

        if (node->mesh == nullptr)
            continue;

        float m[16];
        cgltf_node_transform_world (node, m);

        auto transformPoint = [&] (const float* p)
        {
            return V3 { m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
                        m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
                        m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14] };
        };

        auto transformDirection = [&] (const float* p)
        {
            return V3 { m[0] * p[0] + m[4] * p[1] + m[8] * p[2],
                        m[1] * p[0] + m[5] * p[1] + m[9] * p[2],
                        m[2] * p[0] + m[6] * p[1] + m[10] * p[2] };
        };

        for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi)
        {
            const cgltf_primitive& prim = node->mesh->primitives[pi];

            if (prim.type != cgltf_primitive_type_triangles)
                continue;

            const cgltf_accessor* posAcc = nullptr;
            const cgltf_accessor* nrmAcc = nullptr;

            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
            {
                if (prim.attributes[ai].type == cgltf_attribute_type_position) posAcc = prim.attributes[ai].data;
                if (prim.attributes[ai].type == cgltf_attribute_type_normal)   nrmAcc = prim.attributes[ai].data;
            }

            if (posAcc == nullptr || posAcc->count == 0)
                continue;

            positions.assign (posAcc->count * 3, 0.0f);

            if (cgltf_accessor_unpack_floats (posAcc, positions.data(), positions.size()) == 0)
            {
                sawCompressed = true;
                continue;
            }

            bool hasNormals = nrmAcc != nullptr && nrmAcc->count == posAcc->count;

            if (hasNormals)
            {
                normals.assign (nrmAcc->count * 3, 0.0f);
                hasNormals = cgltf_accessor_unpack_floats (nrmAcc, normals.data(), normals.size()) != 0;
            }

            const cgltf_size numIndices = prim.indices != nullptr ? prim.indices->count : posAcc->count;

            for (cgltf_size i = 0; i + 2 < numIndices; i += 3)
            {
                cgltf_size ix[3];
                bool ok = true;

                for (int k = 0; k < 3; ++k)
                {
                    ix[k] = prim.indices != nullptr ? cgltf_accessor_read_index (prim.indices, i + (cgltf_size) k) : i + (cgltf_size) k;
                    ok = ok && ix[k] < posAcc->count;
                }

                if (! ok)
                    continue;

                const V3 a = transformPoint (&positions[ix[0] * 3]);
                const V3 b = transformPoint (&positions[ix[1] * 3]);
                const V3 c = transformPoint (&positions[ix[2] * 3]);

                if (hasNormals)
                    soup.add (a, b, c, transformDirection (&normals[ix[0] * 3]),
                                       transformDirection (&normals[ix[1] * 3]),
                                       transformDirection (&normals[ix[2] * 3]));
                else
                    soup.add (a, b, c);
            }
        }
    }

    cgltf_free (gltf);

    if (soup.numTriangles() == 0)
    {
        error = sawCompressed ? "This glTF uses mesh compression (Draco/meshopt), which is not supported"
                              : "No triangle meshes found in glTF file";
        return false;
    }

    return true;
}

//==============================================================================
bool loadFbx (const juce::File& file, Soup& soup, juce::String& error)
{
    ufbx_load_opts opts {};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.generate_missing_normals = true;

    ufbx_error fbxError;
    ufbx_scene* scene = ufbx_load_file (file.getFullPathName().toRawUTF8(), &opts, &fbxError);

    if (scene == nullptr)
    {
        error = "Could not read FBX: " + juce::String::fromUTF8 (fbxError.description.data, (int) fbxError.description.length);
        return false;
    }

    std::vector<uint32_t> tri;

    for (size_t ni = 0; ni < scene->nodes.count; ++ni)
    {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;

        if (mesh == nullptr)
            continue;

        const ufbx_matrix toWorld = node->geometry_to_world;
        const ufbx_matrix normalToWorld = ufbx_matrix_for_normals (&toWorld);
        const bool hasNormals = mesh->vertex_normal.exists;

        tri.resize (mesh->max_face_triangles * 3);

        for (size_t fi = 0; fi < mesh->faces.count; ++fi)
        {
            const uint32_t numTris = ufbx_triangulate_face (tri.data(), tri.size(), mesh, mesh->faces.data[fi]);

            for (uint32_t t = 0; t < numTris; ++t)
            {
                V3 p[3], n[3];

                for (int k = 0; k < 3; ++k)
                {
                    const uint32_t ix = tri[(size_t) t * 3 + (size_t) k];
                    const ufbx_vec3 wp = ufbx_transform_position (&toWorld, ufbx_get_vertex_vec3 (&mesh->vertex_position, ix));
                    p[k] = { (float) wp.x, (float) wp.y, (float) wp.z };

                    if (hasNormals)
                    {
                        const ufbx_vec3 wn = ufbx_transform_direction (&normalToWorld, ufbx_get_vertex_vec3 (&mesh->vertex_normal, ix));
                        n[k] = { (float) wn.x, (float) wn.y, (float) wn.z };
                    }
                }

                if (hasNormals)
                    soup.add (p[0], p[1], p[2], n[0], n[1], n[2]);
                else
                    soup.add (p[0], p[1], p[2]);
            }
        }
    }

    ufbx_free_scene (scene);

    if (soup.numTriangles() == 0)
    {
        error = "No meshes found in FBX file";
        return false;
    }

    return true;
}

//==============================================================================
std::shared_ptr<MeshData> finalise (Soup& soup, const juce::String& name)
{
    auto out = std::make_shared<MeshData>();
    out->name = name;

    // Drop triangles that are degenerate or contain NaNs.
    size_t kept = 0;

    for (size_t t = 0; t < soup.numTriangles(); ++t)
    {
        const V3 a = soup.pos[t * 3], b = soup.pos[t * 3 + 1], c = soup.pos[t * 3 + 2];

        if (! (finite (a) && finite (b) && finite (c)))
            continue;

        const float area = length (cross (b - a, c - a));

        if (! (area > 0.0f) || ! std::isfinite (area))
            continue;

        for (size_t k = 0; k < 3; ++k)
        {
            soup.pos[kept * 3 + k] = soup.pos[t * 3 + k];
            soup.nrm[kept * 3 + k] = soup.nrm[t * 3 + k];
        }

        ++kept;
    }

    if (kept == 0)
    {
        out->error = "Model contains no usable triangles";
        return out;
    }

    out->sourceTriangles = (int) juce::jmin (kept, (size_t) 0x7fffffff);

    // Very dense models are thinned so they stay smooth to draw.
    const size_t maxTriangles = 700'000;

    if (kept > maxTriangles)
    {
        const size_t step = (kept + maxTriangles - 1) / maxTriangles;
        size_t n = 0;

        for (size_t t = 0; t < kept; t += step, ++n)
            for (size_t k = 0; k < 3; ++k)
            {
                soup.pos[n * 3 + k] = soup.pos[t * 3 + k];
                soup.nrm[n * 3 + k] = soup.nrm[t * 3 + k];
            }

        kept = n;
    }

    soup.pos.resize (kept * 3);
    soup.nrm.resize (kept * 3);

    // Centre on the bounding box and scale to a unit sphere.
    V3 lo = soup.pos[0], hi = soup.pos[0];

    for (auto& p : soup.pos)
    {
        lo = { juce::jmin (lo.x, p.x), juce::jmin (lo.y, p.y), juce::jmin (lo.z, p.z) };
        hi = { juce::jmax (hi.x, p.x), juce::jmax (hi.y, p.y), juce::jmax (hi.z, p.z) };
    }

    const V3 centre = (lo + hi) * 0.5f;
    double radiusSq = 0.0;

    for (auto& p : soup.pos)
    {
        const double dx = (double) p.x - centre.x, dy = (double) p.y - centre.y, dz = (double) p.z - centre.z;
        radiusSq = juce::jmax (radiusSq, dx * dx + dy * dy + dz * dz);
    }

    const double radius = std::sqrt (radiusSq);

    if (! (radius > 0.0) || ! std::isfinite (radius))
    {
        out->error = "Model has no size";
        return out;
    }

    for (auto& p : soup.pos)
        p = { (float) ((p.x - (double) centre.x) / radius), (float) ((p.y - (double) centre.y) / radius), (float) ((p.z - (double) centre.z) / radius) };

    // Face normals and areas (after scaling, so areas are comparable).
    std::vector<V3> faceNormal (kept);
    std::vector<double> cdf (kept);
    double totalArea = 0.0;
    bool needSmooth = false;

    for (size_t t = 0; t < kept; ++t)
    {
        const V3 n = cross (soup.pos[t * 3 + 1] - soup.pos[t * 3], soup.pos[t * 3 + 2] - soup.pos[t * 3]);
        faceNormal[t] = n;
        totalArea += (double) length (n) * 0.5 + 1.0e-12;
        cdf[t] = totalArea;

        for (size_t k = 0; k < 3; ++k)
        {
            V3& vn = soup.nrm[t * 3 + k];

            if (! finite (vn) || dot (vn, vn) < 1.0e-12f)
            {
                vn = {};
                needSmooth = true;
            }
            else
            {
                vn = normalised (vn);
            }
        }
    }

    if (needSmooth)
    {
        // Weld by position, average area-weighted face normals, and keep hard
        // edges hard by falling back to the face normal across sharp creases.
        auto keyOf = [] (V3 p)
        {
            const auto q = [] (float v) { return (uint64_t) juce::jlimit (0, (1 << 21) - 1, (int) std::lround ((v + 1.0f) * 500000.0f * 2.0f)); };
            return q (p.x) | (q (p.y) << 21) | (q (p.z) << 42);
        };

        std::unordered_map<uint64_t, V3> welded;
        welded.reserve (kept * 2);

        for (size_t t = 0; t < kept; ++t)
            for (size_t k = 0; k < 3; ++k)
            {
                V3& acc = welded[keyOf (soup.pos[t * 3 + k])];
                acc = acc + faceNormal[t];
            }

        for (size_t t = 0; t < kept; ++t)
        {
            const V3 fn = normalised (faceNormal[t]);

            for (size_t k = 0; k < 3; ++k)
            {
                V3& vn = soup.nrm[t * 3 + k];

                if (dot (vn, vn) > 0.5f)
                    continue;

                const V3 sn = normalised (welded[keyOf (soup.pos[t * 3 + k])], fn);
                vn = dot (sn, fn) > 0.5f ? sn : fn;
            }
        }
    }

    std::mt19937 rng (0x1234abcd);
    std::uniform_real_distribution<float> uniform (0.0f, 1.0f);

    out->numTriangles = (int) kept;
    out->tris.resize (kept * 3 * (size_t) MeshData::triStride);
    float* dst = out->tris.data();

    for (size_t t = 0; t < kept; ++t)
    {
        const V3 centroid = (soup.pos[t * 3] + soup.pos[t * 3 + 1] + soup.pos[t * 3 + 2]) * (1.0f / 3.0f);
        const float rnd = uniform (rng);

        for (size_t k = 0; k < 3; ++k)
        {
            const V3 p = soup.pos[t * 3 + k], n = soup.nrm[t * 3 + k];
            *dst++ = p.x; *dst++ = p.y; *dst++ = p.z;
            *dst++ = n.x; *dst++ = n.y; *dst++ = n.z;
            *dst++ = centroid.x; *dst++ = centroid.y; *dst++ = centroid.z;
            *dst++ = rnd;
        }
    }

    // Area-weighted surface samples for the dust mode.
    const size_t numPoints = 220'000;
    out->points.resize (numPoints * (size_t) MeshData::pointStride);
    dst = out->points.data();

    for (size_t i = 0; i < numPoints; ++i)
    {
        const double pick = (double) uniform (rng) * totalArea;
        const size_t t = juce::jmin (kept - 1, (size_t) (std::lower_bound (cdf.begin(), cdf.end(), pick) - cdf.begin()));

        const float su = std::sqrt (uniform (rng));
        const float r2 = uniform (rng);
        const float b0 = 1.0f - su, b1 = su * (1.0f - r2), b2 = su * r2;

        const V3 p = soup.pos[t * 3] * b0 + soup.pos[t * 3 + 1] * b1 + soup.pos[t * 3 + 2] * b2;
        const V3 n = normalised (soup.nrm[t * 3] * b0 + soup.nrm[t * 3 + 1] * b1 + soup.nrm[t * 3 + 2] * b2);

        *dst++ = p.x; *dst++ = p.y; *dst++ = p.z;
        *dst++ = n.x; *dst++ = n.y; *dst++ = n.z;
        *dst++ = uniform (rng);
    }

    return out;
}

const char* const supportedExtensions[] = { "glb", "gltf", "fbx", "obj", "stl", "ply" };

} // namespace

//==============================================================================
bool ModelLoader::isSupported (const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase().trimCharactersAtStart (".");

    for (auto* e : supportedExtensions)
        if (ext == e)
            return true;

    return false;
}

juce::String ModelLoader::getWildcard()
{
    return "*.glb;*.gltf;*.fbx;*.obj;*.stl;*.ply";
}

std::shared_ptr<MeshData> ModelLoader::load (const juce::File& file)
{
    auto fail = [&] (const juce::String& message)
    {
        auto m = std::make_shared<MeshData>();
        m->name = file.getFileName();
        m->error = message;
        return m;
    };

    if (! file.existsAsFile())
        return fail ("File not found");

    if (file.getSize() > (juce::int64) 1500 * 1024 * 1024)
        return fail ("File is too large (over 1.5 GB)");

    const auto ext = file.getFileExtension().toLowerCase().trimCharactersAtStart (".");

    try
    {
        Soup soup;
        juce::String error;
        bool ok = false;

        if (ext == "fbx")
        {
            ok = loadFbx (file, soup, error);
        }
        else
        {
            juce::MemoryBlock data;

            if (! file.loadFileAsData (data) || data.getSize() == 0)
                return fail ("Could not read file");

            if (ext == "obj")                       ok = loadObj (data, soup, error);
            else if (ext == "stl")                  ok = loadStl (data, soup, error);
            else if (ext == "ply")                  ok = loadPly (data, soup, error);
            else if (ext == "glb" || ext == "gltf") ok = loadGltf (file, data, soup, error);
            else                                    return fail ("Unsupported model format");
        }

        if (! ok)
            return fail (error.isNotEmpty() ? error : "Could not load model");

        return finalise (soup, file.getFileName());
    }
    catch (const std::bad_alloc&)
    {
        return fail ("Not enough memory to load this model");
    }
    catch (...)
    {
        return fail ("Unexpected error while loading model");
    }
}

std::shared_ptr<MeshData> ModelLoader::makeBuiltin()
{
    // A (2,3) torus knot swept into a tube.
    const int segments = 420, sides = 22;
    const float knotP = 2.0f, knotQ = 3.0f, major = 1.0f, minor = 0.45f, tube = 0.2f;
    const float twoPi = juce::MathConstants<float>::twoPi;

    auto curve = [&] (float t)
    {
        const float r = major + minor * std::cos (knotQ * t);
        return V3 { r * std::cos (knotP * t), r * std::sin (knotP * t), minor * std::sin (knotQ * t) };
    };

    std::vector<V3> ringPos ((size_t) (segments * sides)), ringNrm ((size_t) (segments * sides));

    for (int i = 0; i < segments; ++i)
    {
        const float t = twoPi * (float) i / (float) segments;
        const V3 c = curve (t);
        const V3 tangent = normalised (curve (t + 0.001f) - curve (t - 0.001f));
        const V3 spine { major * std::cos (knotP * t), major * std::sin (knotP * t), 0.0f };
        const V3 binormal = normalised (cross (tangent, normalised (c - spine)));
        const V3 normal = cross (binormal, tangent);

        for (int j = 0; j < sides; ++j)
        {
            const float a = twoPi * (float) j / (float) sides;
            const V3 n = normal * std::cos (a) + binormal * std::sin (a);
            ringPos[(size_t) (i * sides + j)] = c + n * tube;
            ringNrm[(size_t) (i * sides + j)] = n;
        }
    }

    Soup soup;

    for (int i = 0; i < segments; ++i)
        for (int j = 0; j < sides; ++j)
        {
            const auto a = (size_t) (i * sides + j);
            const auto b = (size_t) (((i + 1) % segments) * sides + j);
            const auto c = (size_t) (((i + 1) % segments) * sides + (j + 1) % sides);
            const auto d = (size_t) (i * sides + (j + 1) % sides);

            soup.add (ringPos[a], ringPos[b], ringPos[c], ringNrm[a], ringNrm[b], ringNrm[c]);
            soup.add (ringPos[a], ringPos[c], ringPos[d], ringNrm[a], ringNrm[c], ringNrm[d]);
        }

    return finalise (soup, "Built-in knot");
}

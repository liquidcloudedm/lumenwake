#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

/** A model prepared for the renderer: centred, scaled to a unit sphere, with a
    triangle soup for the surface modes and a point cloud for the dust mode. */
struct MeshData
{
    static constexpr int triStride = 10;    // position 3, normal 3, triangle centroid 3, random 1
    static constexpr int pointStride = 7;   // position 3, normal 3, random 1

    std::vector<float> tris;
    std::vector<float> points;

    int numTriangles = 0;
    int sourceTriangles = 0;
    juce::String name;
    juce::String error;

    bool isValid() const noexcept   { return error.isEmpty() && numTriangles > 0; }
};

namespace ModelLoader
{
    /** Extensions without the dot: glb, gltf, fbx, obj, stl, ply. */
    bool isSupported (const juce::File& file);
    juce::String getWildcard();

    /** Never returns null. On failure the result carries an error message. */
    std::shared_ptr<MeshData> load (const juce::File& file);

    /** The sculpture shown before the user loads a model of their own. */
    std::shared_ptr<MeshData> makeBuiltin();
}

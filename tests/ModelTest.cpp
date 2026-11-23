#include "ModelLoader.h"
#include <iostream>

// Usage: lw_modeltest file [file ...]
// Prints the triangle count, bounds and normal sanity for each model.
int main (int argc, char* argv[])
{
    int failures = 0;

    for (int i = 1; i < argc; ++i)
    {
        const juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[i])));
        const auto mesh = ModelLoader::load (file);

        std::cout << file.getFileName() << ": ";

        if (! mesh->isValid())
        {
            std::cout << "FAILED - " << mesh->error << std::endl;
            ++failures;
            continue;
        }

        float maxRadius = 0.0f, minNormal = 10.0f, maxNormal = 0.0f;

        for (size_t v = 0; v < mesh->tris.size(); v += (size_t) MeshData::triStride)
        {
            const float* p = &mesh->tris[v];
            maxRadius = juce::jmax (maxRadius, std::sqrt (p[0] * p[0] + p[1] * p[1] + p[2] * p[2]));
            const float n = std::sqrt (p[3] * p[3] + p[4] * p[4] + p[5] * p[5]);
            minNormal = juce::jmin (minNormal, n);
            maxNormal = juce::jmax (maxNormal, n);
        }

        std::cout << mesh->numTriangles << " triangles (source " << mesh->sourceTriangles << "), radius " << maxRadius
                  << ", normal length " << minNormal << ".." << maxNormal
                  << ", " << mesh->points.size() / (size_t) MeshData::pointStride << " points" << std::endl;
    }

    return failures;
}

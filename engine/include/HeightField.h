#pragma once
#include <glm/glm.hpp>
#include <vector>

// Ground height over a regular grid, sampled from a mesh. Walking on terrain only ever needs
// "how high is the ground here", which is far cheaper than testing against its triangles.
class HeightField {
public:
    // Rasterizes triangles (three vertices each, in object space) moved by transform.
    void build(const std::vector<glm::vec3>& triangles, const glm::mat4& transform, float cellSize = 1.0f);

    bool isValid() const { return !heights.empty(); }

    float heightAt(float x, float z) const;
    glm::vec3 clampInside(glm::vec3 position, float margin) const;

private:
    std::vector<float> heights;
    int columns = 0;
    int rows = 0;
    float cell = 1.0f;
    glm::vec2 origin = glm::vec2(0.0f);

    float sample(int column, int row) const;
};

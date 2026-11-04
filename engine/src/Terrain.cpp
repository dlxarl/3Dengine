#include "Terrain.h"

#include <algorithm>
#include <cmath>

namespace {
    float smoothstep01(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // How hard a brush acts at a distance: full inside the hard core, fading to nothing at the edge.
    float brushWeight(float distance, float radius, float falloff) {
        if (distance >= radius) return 0.0f;
        const float core = radius * (1.0f - std::clamp(falloff, 0.0f, 1.0f));
        if (distance <= core) return 1.0f;
        return 1.0f - smoothstep01((distance - core) / std::max(radius - core, 1e-4f));
    }

    float hash(int x, int z, uint32_t seed) {
        uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u + seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return float(h ^ (h >> 16)) / 4294967295.0f;
    }

    float valueNoise(float x, float z, uint32_t seed) {
        const int ix = (int)std::floor(x), iz = (int)std::floor(z);
        const float fx = smoothstep01(x - ix), fz = smoothstep01(z - iz);
        const float a = hash(ix, iz, seed), b = hash(ix + 1, iz, seed);
        const float c = hash(ix, iz + 1, seed), d = hash(ix + 1, iz + 1, seed);
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fz;
    }
}

TerrainObject::TerrainObject() {
    reset(cells, cellSize);
    gameRole = GameRole::Ground;
    isStatic = true;
}

void TerrainObject::fitArrays() {
    heights.resize((size_t)(cells + 1) * (cells + 1), 0.0f);
    tiles.resize((size_t)cells * cells, 0);
}

void TerrainObject::reset(int newCells, float newCellSize) {
    cells = std::clamp(newCells, 2, 256);
    cellSize = std::max(0.1f, newCellSize);
    heights.assign((size_t)(cells + 1) * (cells + 1), 0.0f);
    tiles.assign((size_t)cells * cells, 0);
    dirty = true;
}

bool TerrainObject::contains(float x, float z) const {
    const float half = size() * 0.5f;
    return x >= -half && x <= half && z >= -half && z <= half;
}

float TerrainObject::heightAt(float x, float z) const {
    if (!contains(x, z)) return 0.0f;
    const float half = size() * 0.5f;
    const float gx = (x + half) / cellSize, gz = (z + half) / cellSize;
    const int ix = std::min((int)gx, cells - 1), iz = std::min((int)gz, cells - 1);
    const float fx = gx - ix, fz = gz - iz;
    const float h00 = heightAtVertex(ix, iz), h10 = heightAtVertex(ix + 1, iz);
    const float h01 = heightAtVertex(ix, iz + 1), h11 = heightAtVertex(ix + 1, iz + 1);
    return (h00 + (h10 - h00) * fx) + ((h01 + (h11 - h01) * fx) - (h00 + (h10 - h00) * fx)) * fz;
}

bool TerrainObject::raycast(glm::vec3 origin, glm::vec3 direction, glm::vec3& hit) const {
    const float length = glm::length(direction);
    if (length < 1e-6f) return false;
    direction /= length;

    // Marched in steps of half a cell, then the crossing is narrowed down by halving.
    const float reach = size() * 2.0f + 2000.0f;
    const float step = std::max(0.05f, cellSize * 0.5f);
    float previous = 0.0f;
    bool wasAbove = true;
    for (float t = 0.0f; t < reach; t += step) {
        const glm::vec3 p = origin + direction * t;
        const bool inside = contains(p.x, p.z);
        const bool above = !inside || p.y > heightAt(p.x, p.z);
        if (!above && inside && wasAbove) {
            float low = previous, high = t;
            for (int i = 0; i < 20; i++) {
                const float mid = (low + high) * 0.5f;
                const glm::vec3 q = origin + direction * mid;
                if (q.y > heightAt(q.x, q.z)) low = mid; else high = mid;
            }
            hit = origin + direction * high;
            return true;
        }
        if (inside || !above) wasAbove = above;
        previous = t;
    }
    return false;
}

bool TerrainObject::applyBrush(Brush brush, glm::vec2 center, float radius, float strength, float falloff, float deltaTime,
                               float flattenHeight, int tile) {
    fitArrays();
    const float half = size() * 0.5f;
    bool changed = false;

    if (brush == Brush::Paint) {
        const int tileCount = std::max(1, atlasColumns * atlasRows);
        const uint8_t value = (uint8_t)std::clamp(tile, 0, tileCount - 1);
        for (int z = 0; z < cells; z++) {
            for (int x = 0; x < cells; x++) {
                const glm::vec2 middle((x + 0.5f) * cellSize - half, (z + 0.5f) * cellSize - half);
                if (glm::distance(middle, center) <= radius && tiles[(size_t)z * cells + x] != value) {
                    tiles[(size_t)z * cells + x] = value;
                    changed = true;
                }
            }
        }
        dirty |= changed;
        return changed;
    }

    const int x0 = std::max(0, (int)std::floor((center.x - radius + half) / cellSize));
    const int x1 = std::min(cells, (int)std::ceil((center.x + radius + half) / cellSize));
    const int z0 = std::max(0, (int)std::floor((center.y - radius + half) / cellSize));
    const int z1 = std::min(cells, (int)std::ceil((center.y + radius + half) / cellSize));

    // Smoothing reads the heights as they were, so that the result does not depend on the order.
    const std::vector<float> before = heights;
    auto at = [&](int x, int z) { return before[(size_t)std::clamp(z, 0, cells) * (cells + 1) + std::clamp(x, 0, cells)]; };

    for (int z = z0; z <= z1; z++) {
        for (int x = x0; x <= x1; x++) {
            const glm::vec2 corner(x * cellSize - half, z * cellSize - half);
            const float weight = brushWeight(glm::distance(corner, center), radius, falloff);
            if (weight <= 0.0f) continue;

            float& h = heights[(size_t)z * (cells + 1) + x];
            const float old = h;
            switch (brush) {
                case Brush::Raise: h += strength * weight * deltaTime; break;
                case Brush::Lower: h -= strength * weight * deltaTime; break;
                case Brush::Smooth: {
                    const float average = (at(x - 1, z) + at(x + 1, z) + at(x, z - 1) + at(x, z + 1) + old) / 5.0f;
                    h += (average - old) * std::min(1.0f, weight * strength * deltaTime * 6.0f);
                    break;
                }
                case Brush::Flatten:
                    h += (flattenHeight - old) * std::min(1.0f, weight * strength * deltaTime * 6.0f);
                    break;
                case Brush::Paint: break;
            }
            changed |= h != old;
        }
    }
    dirty |= changed;
    return changed;
}

void TerrainObject::generate(uint32_t seed, float amplitude, float wavelength, int octaves) {
    fitArrays();
    octaves = std::clamp(octaves, 1, 8);
    const float half = size() * 0.5f;
    for (int z = 0; z <= cells; z++) {
        for (int x = 0; x <= cells; x++) {
            float height = 0.0f, a = 1.0f, total = 0.0f, frequency = 1.0f / std::max(wavelength, 0.5f);
            for (int octave = 0; octave < octaves; octave++) {
                height += a * valueNoise((x * cellSize - half) * frequency, (z * cellSize - half) * frequency, seed + octave * 101u);
                total += a;
                a *= 0.5f;
                frequency *= 2.0f;
            }
            heights[(size_t)z * (cells + 1) + x] = (height / total - 0.5f) * 2.0f * amplitude;
        }
    }
    dirty = true;
}

void TerrainObject::draw(Shader& shader) {
    if (dirty || !getMesh()) rebuild();
    MeshShape::draw(shader);
}

void TerrainObject::rebuild() {
    fitArrays();
    dirty = false;

    const float half = size() * 0.5f;
    auto vertexNormal = [&](int x, int z) {
        auto h = [&](int ix, int iz) { return heightAtVertex(std::clamp(ix, 0, cells), std::clamp(iz, 0, cells)); };
        return glm::normalize(glm::vec3(h(x - 1, z) - h(x + 1, z), 2.0f * cellSize, h(x, z - 1) - h(x, z + 1)));
    };

    MeshData data;
    data.vertices.reserve((size_t)cells * cells * 6 * 8);
    const int columns = std::max(1, atlasColumns), rows = std::max(1, atlasRows);
    const float margin = (columns * rows > 1) ? 0.04f : 0.0f;   // keeps neighbouring tiles from bleeding in

    auto push = [&](int x, int z, float u, float v) {
        const glm::vec3 n = vertexNormal(x, z);
        const float values[8] = {x * cellSize - half, heightAtVertex(x, z), z * cellSize - half, n.x, n.y, n.z, u, v};
        data.vertices.insert(data.vertices.end(), values, values + 8);
    };

    for (int z = 0; z < cells; z++) {
        for (int x = 0; x < cells; x++) {
            const int tile = std::min((int)tiles[(size_t)z * cells + x], columns * rows - 1);
            const int column = tile % columns, row = tile / columns;
            const float u0 = (column + margin) / columns, u1 = (column + 1 - margin) / columns;
            const float v1 = 1.0f - (row + margin) / rows, v0 = 1.0f - (row + 1 - margin) / rows;

            // Wound so that the surface faces up.
            push(x, z, u0, v1);
            push(x, z + 1, u0, v0);
            push(x + 1, z, u1, v1);
            push(x + 1, z, u1, v1);
            push(x, z + 1, u0, v0);
            push(x + 1, z + 1, u1, v0);
        }
    }
    setMesh(std::make_shared<GpuMesh>(data), std::string());
}

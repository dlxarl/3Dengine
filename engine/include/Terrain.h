#pragma once
#include "MeshShape.h"

#include <cstdint>
#include <vector>

// A heightfield you shape and paint: a square of cells whose corners have heights, drawn as a
// mesh. The mesh is rebuilt whenever the heights or the painting change. Centered on the object,
// in its local space, with a cell of cellSize metres.
class TerrainObject : public MeshShape {
public:
    TerrainObject();

    ObjectType getType() const override { return ObjectType::Terrain; }

    // The mesh is made when it is first needed, and again after every change.
    void draw(Shader& shader) override;

    int cells = 64;                 // along each side
    float cellSize = 2.0f;          // metres
    int atlasColumns = 1;           // the texture is a grid of tiles to paint with; 1 by 1 repeats it per cell
    int atlasRows = 1;
    std::vector<float> heights;     // (cells + 1) squared, row by row; x runs fastest
    std::vector<uint8_t> tiles;     // one per cell: which atlas tile it shows

    enum class Brush { Raise, Lower, Smooth, Flatten, Paint };

    // Flat ground at height 0, painted with tile 0.
    void reset(int newCells, float newCellSize);

    float size() const { return cells * cellSize; }
    float heightAtVertex(int x, int z) const { return heights[(size_t)z * (cells + 1) + x]; }

    // The height of the surface at a point in local space; 0 outside the terrain.
    float heightAt(float x, float z) const;
    bool contains(float x, float z) const;

    // Where a ray, in local space, meets the surface.
    bool raycast(glm::vec3 origin, glm::vec3 direction, glm::vec3& hit) const;

    // One step of a brush centered on a point in local space. strength is metres per second
    // for raising and lowering, and a rate of 0 to 1 for the rest; falloff 0 is a hard edge.
    // Returns true when something changed (the mesh is rebuilt by rebuild()).
    bool applyBrush(Brush brush, glm::vec2 center, float radius, float strength, float falloff, float deltaTime,
                    float flattenHeight = 0.0f, int tile = 0);

    // Rolling hills of layered noise, replacing the heights.
    void generate(uint32_t seed, float amplitude, float wavelength, int octaves);

    // Brings the mesh in line with the heights and the painting.
    void rebuild();
    bool needsRebuild() const { return dirty; }
    void markChanged() { dirty = true; }

private:
    bool dirty = true;
    void fitArrays();
};

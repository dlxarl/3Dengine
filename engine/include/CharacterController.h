#pragma once
#include "Colliders.h"
#include "HeightField.h"
#include "WaterArea.h"
#include "Shape.h"

#include <vector>

// Where a character is, beyond its position and velocity, which live on the body itself.
struct CharacterState {
    bool grounded = false;
    bool swimming = false;
    float jumpGrace = 0.0f;  // time left to jump after leaving the ground
    bool diving = false;     // swimming under the surface rather than floating on it
    bool submerged = false;  // the head is under water
};

struct CharacterSurroundings {
    const ColliderGrid* colliders = nullptr;
    const HeightField* ground = nullptr;
    std::vector<WaterArea> waters;

    // The water over a point, if there is any. Where areas overlap, as where the pieces of a
    // river meet, the one whose middle is nearest.
    const WaterArea* waterAt(glm::vec2 point) const;
};

struct CharacterMove {
    float landingSpeed = 0.0f;  // how fast it hit the ground this frame; 0 when it did not land
};

// Moves a character the way a player expects: it speeds up and slows down instead of starting
// and stopping dead, slides along walls (turned ones too, and round their corners), steps up
// small ledges, cannot climb slopes that are too steep and floats with its head out of deep
// water. Against walls it is an upright cylinder as wide as its box; on top of things, a box.
class CharacterController {
public:
    float acceleration = 45.0f;     // on the ground, per second
    float airAcceleration = 9.0f;   // steering in the air
    float gravity = 19.6f;
    float jumpSpeed = 7.67f;        // about 1.5 units high
    float stepHeight = 0.45f;
    float maxSlope = 1.15f;         // rise per unit of distance, about 49 degrees
    float jumpGraceSeconds = 0.12f;
    float swimDepth = 1.0f;         // water deeper than this at the feet means swimming

    // wish is the direction to go, up to length 1; speed is how fast at full length.
    // swimUp is -1 to dive and 1 to rise, and only means something in water. A diver steers up
    // and down with the y of wish.
    CharacterMove move(Shape& body, CharacterState& state, glm::vec3 wish, float speed, bool jump,
                       const CharacterSurroundings& world, float deltaTime, float swimUp = 0.0f) const;
};

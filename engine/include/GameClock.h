#pragma once
#include "GameDefinition.h"
#include "SceneGraph.h"

// The time of day, and how the sky, the sun and the fog look at it.
class GameClock {
public:
    void begin(const WorldDefinition& world);

    bool isRunning() const { return dayMinutes > 0.0f; }
    void update(float deltaTime);

    float getHour() const { return hour; }
    int getDay() const { return day; }
    bool isNight() const;

    // Moves time forward to the next time it is this hour.
    void sleepUntil(float targetHour);

    // Sets the sky color, the fog and the scene's light for the current hour. The first call
    // remembers the scene's own fog distances, which night then shortens.
    void apply(SceneGraph& scene);

private:
    float dayMinutes = 0.0f;
    float hour = 12.0f;
    int day = 1;

    bool fogRemembered = false;
    float dayFogStart = 0.0f;
    float dayFogEnd = 0.0f;
};

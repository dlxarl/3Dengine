#include "GameActors.h"
#include "Player.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {
    struct KindStats {
        float health;
        float walkSpeed;
        float runSpeed;
        float sight;         // hostile: notices the player; timid: runs from the player
        float reach;
        float damage;
        float attackSeconds;
        float wanderRadius;
        float radius;        // of its body, for being hit and for keeping apart
        float height;
        float shove;         // how hard a hit knocks the player
        bool hostile;
        bool fleesPlayer;
        bool upright;        // falls over backwards when it dies; animals fall on their side
    };

    // Villager, Bandit, Deer, Wolf, Livestock, Boar, Bear, Rabbit, Bird, Dragon
    const KindStats STATS[] = {
        // health walk  run    sight  reach dmg    attack wander radius height shove hostile flees upright
        {50.0f,  1.8f, 5.0f,  0.0f,  0.0f, 0.0f,  0.0f, 9.0f,  0.4f,  1.9f, 0.0f, false, false, true},
        {100.0f, 2.2f, 4.6f,  16.0f, 1.9f, 10.0f, 1.2f, 9.0f,  0.4f,  1.9f, 2.5f, true,  false, true},
        {40.0f,  1.6f, 7.5f,  11.0f, 0.0f, 0.0f,  0.0f, 14.0f, 0.45f, 1.6f, 0.0f, false, true,  false},
        {60.0f,  2.0f, 6.8f,  13.0f, 1.7f, 8.0f,  1.0f, 12.0f, 0.4f,  1.1f, 2.0f, true,  false, false},
        {40.0f,  1.0f, 3.2f,  0.0f,  0.0f, 0.0f,  0.0f, 6.0f,  0.45f, 1.2f, 0.0f, false, false, false},
        {90.0f,  1.3f, 7.2f,  7.0f,  1.6f, 13.0f, 1.3f, 10.0f, 0.5f,  1.0f, 7.0f, true,  false, false},
        {260.0f, 1.4f, 6.2f,  11.0f, 2.3f, 22.0f, 1.6f, 12.0f, 0.85f, 1.9f, 7.0f, true,  false, false},
        {12.0f,  1.0f, 8.0f,  7.0f,  0.0f, 0.0f,  0.0f, 6.0f,  0.25f, 0.5f, 0.0f, false, true,  false},
        {8.0f,   0.7f, 9.0f,  6.0f,  0.0f, 0.0f,  0.0f, 4.0f,  0.2f,  0.4f, 0.0f, false, true,  false},
        {1300.0f, 4.5f, 17.0f, 42.0f, 4.6f, 28.0f, 1.8f, 0.0f, 2.2f,  4.0f, 9.0f, true,  false, false},
    };

    const float WOLF_NIGHT_SIGHT = 1.7f;   // how much further wolves see in the dark
    const float LOSE_SIGHT = 1.6f;         // times sight, where a chase is given up
    const float PACK_RANGE = 15.0f;
    const float FEAR_RANGE = 8.0f;         // villagers run from a chase this close
    const float PANIC_SECONDS = 5.0f;
    const float KNOCKBACK = 5.0f;
    const float KNOCK_FADE = 7.0f;
    const float PROP_PUSH = 4.0f;
    const float ARROW_GRAVITY = 7.0f;
    const float ARROW_STUCK_SECONDS = 6.0f;

    const float MAX_STEP_RISE = 1.3f;      // per unit walked; steeper ground is a wall
    const float WADE_DEPTH = 0.6f;         // deeper water is a wall to anyone walking into it
    const float CLIMBABLE = 0.35f;         // solids this low are walked over
    const float DETOUR_SECONDS = 0.9f;
    const float IDLE_DISTANCE = 170.0f;    // further from the player, only those already busy move
    const float PLAYER_RADIUS = 0.4f;
    const float FALL_RATE = 2.2f;          // of a body falling over, in falls per second

    const float FLIGHT_ACCELERATION = 16.0f;
    const float BIRD_CIRCLE = 9.0f;
    const float BIRD_HEIGHT = 9.0f;
    const float BIRD_FLIGHT_SECONDS = 9.0f;

    const float DRAGON_ORBIT = 24.0f;
    const float DRAGON_ALTITUDE = 15.0f;
    const float DRAGON_LANDS_BELOW = 0.4f;  // of its health
    const float DRAGON_GIVES_UP = 2.4f;     // times its sight
    const float FIRE_SPEED = 26.0f;
    const float FIRE_GRAVITY = 3.0f;
    const float FIRE_DAMAGE = 12.0f;
    const float FIRE_SPLASH = 3.2f;
    const float FIRE_SECONDS = 4.0f;
    const float FIRE_BURST_SECONDS = 0.35f;
    const float FIRE_LEAD_SECONDS = 0.5f;

    glm::vec3 flat(glm::vec3 v) {
        v.y = 0.0f;
        const float length = glm::length(v);
        return length > 0.0001f ? v / length : glm::vec3(0.0f);
    }

    float flatDistance(glm::vec3 a, glm::vec3 b) {
        return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
    }

    glm::vec3 forwardOf(float yaw) {
        return glm::vec3(-std::sin(yaw), 0.0f, -std::cos(yaw));
    }

    float wrapAngle(float angle) {
        while (angle > 3.14159265f) angle -= 6.2831853f;
        while (angle < -3.14159265f) angle += 6.2831853f;
        return angle;
    }

    void tint(SceneObject& object, glm::vec3 color) {
        if (auto* shape = dynamic_cast<Shape*>(&object)) shape->setColor(color);
        for (SceneObject* child : object.getChildren()) tint(*child, color);
    }

    GameRole roleOf(int kind) {
        static const GameRole ROLES[] = {GameRole::Villager, GameRole::Bandit, GameRole::Deer, GameRole::Wolf, GameRole::Livestock,
                                         GameRole::Boar, GameRole::Bear, GameRole::Rabbit, GameRole::Bird, GameRole::Dragon};
        return ROLES[kind];
    }
}

void animateLimbs(SceneObject& root, float stride, float amount, float wingbeat, float wingAmount) {
    for (SceneObject* child : root.getChildren()) {
        auto* limb = dynamic_cast<Shape*>(child);
        if (!limb || limb->gameRole != GameRole::Limb) continue;
        const int axis = std::clamp(limb->limbAxis, 0, 2);
        glm::vec3 rotation = limb->rotation;
        if (axis == 2) {
            rotation.z = limb->limbSwing * wingAmount * std::sin(wingbeat + glm::radians(limb->limbPhase));
        } else {
            rotation[axis] = limb->limbSwing * amount * std::sin(stride + glm::radians(limb->limbPhase));
        }
        limb->setRotation(rotation);
    }
}

float GameActors::randomRange(float low, float high) {
    return std::uniform_real_distribution<float>(low, high)(random);
}

float GameActors::groundAt(const CharacterSurroundings& world, glm::vec3 point) const {
    return world.ground && world.ground->isValid() ? world.ground->heightAt(point.x, point.z) : 0.0f;
}

void GameActors::begin(SceneGraph& scene, const std::unordered_set<std::string>& stationary) {
    npcs.clear();
    arrows.clear();
    fires.clear();
    props.clear();
    events.clear();
    totalBandits = 0;
    playerPush = glm::vec3(0.0f);
    random.seed(1234);
    player = scene.getPlayer();
    lastPlayerFeet = player ? player->position : glm::vec3(0.0f);
    playerVelocity = glm::vec3(0.0f);

    for (const auto& shape : scene.getShapes()) {
        int kind = -1;
        switch (shape->gameRole) {
            case GameRole::Villager:  kind = 0; break;
            case GameRole::Bandit:    kind = 1; break;
            case GameRole::Deer:      kind = 2; break;
            case GameRole::Wolf:      kind = 3; break;
            case GameRole::Livestock: kind = 4; break;
            case GameRole::Boar:      kind = 5; break;
            case GameRole::Bear:      kind = 6; break;
            case GameRole::Rabbit:    kind = 7; break;
            case GameRole::Bird:      kind = 8; break;
            case GameRole::Dragon:    kind = 9; break;
            case GameRole::Arrow:
                shape->visible = false;
                arrows.push_back({shape->id, glm::vec3(0.0f), 0.0f, 0.0f, false});
                continue;
            case GameRole::Fireball:
                shape->visible = false;
                fires.push_back({shape->id, glm::vec3(0.0f), shape->scale, 0.0f, 0.0f});
                continue;
            default:
                if (shape->hasCollision && !shape->isStatic && shape->getType() != ObjectType::Player) props.push_back(shape.get());
                continue;
        }

        Npc npc{};
        npc.id = shape->id;
        npc.kind = (Kind)kind;
        npc.state = State::Idle;
        npc.home = shape->position;
        npc.target = shape->position;
        npc.yaw = glm::radians(shape->rotation.y);
        npc.health = STATS[kind].health;
        npc.wanderRadius = stationary.count(shape->name) ? 0.0f : STATS[kind].wanderRadius;
        npc.timer = randomRange(0.0f, 2.0f);
        npc.stride = randomRange(0.0f, 6.0f);
        npc.wingbeat = randomRange(0.0f, 6.0f);
        npc.breath = 2.0f;
        npc.orbit = randomRange(0.0f, 6.2831853f);
        npc.flight = Flight::Circle;
        npcs.push_back(npc);
        totalBandits += npc.kind == Kind::Bandit ? 1 : 0;
    }
}

int GameActors::banditsAlive() const {
    int alive = 0;
    for (const Npc& npc : npcs) {
        alive += npc.kind == Kind::Bandit && npc.state != State::Dead ? 1 : 0;
    }
    return alive;
}

bool GameActors::isAlive(ObjectId id) const {
    for (const Npc& npc : npcs) {
        if (npc.id == id) return npc.state != State::Dead;
    }
    return true;
}

bool GameActors::hostileNear(const SceneGraph& scene, glm::vec3 position, float range) const {
    for (const Npc& npc : npcs) {
        if (npc.state == State::Dead || !STATS[(int)npc.kind].hostile) continue;
        const Shape* body = scene.findAs<Shape>(npc.id);
        if (npc.state == State::Chase || (body && glm::length(body->position - position) < range)) return true;
    }
    return false;
}

void GameActors::holdStill(ObjectId id, glm::vec3 lookAt, float seconds) {
    for (Npc& npc : npcs) {
        if (npc.id == id && npc.state != State::Dead) {
            npc.hold = seconds;
            npc.lookAt = lookAt;
        }
    }
}

bool GameActors::walk(const CharacterSurroundings& world, Npc& npc, Shape& body, glm::vec3 direction, float speed, float deltaTime,
                      bool turn) {
    if (glm::length(direction) < 0.001f) return false;
    const KindStats& stats = STATS[(int)npc.kind];

    // A way round whatever was in the way last time, for a little while.
    glm::vec3 heading = direction;
    if (npc.detourTime > 0.0f) {
        npc.detourTime -= deltaTime;
        heading = flat(direction * 0.3f + npc.detour);
    }

    const glm::vec3 feet = body.position;
    const glm::vec3 step = heading * speed * deltaTime;
    const bool hasGround = world.ground && world.ground->isValid();

    // Ground too steep to climb, and water too deep to wade into, are walls too.
    auto passable = [&](glm::vec3 to) {
        if (!hasGround) return true;
        const float here = world.ground->heightAt(feet.x, feet.z);
        const float there = world.ground->heightAt(to.x, to.z);
        if (there - here > MAX_STEP_RISE * flatDistance(to, feet) + 0.01f) return false;
        const WaterArea* water = world.waterAt(glm::vec2(to.x, to.z));
        // Getting out of deep water is always allowed, so nobody is trapped in it.
        return !(water && there < water->level - WADE_DEPTH && there < here);
    };

    glm::vec3 next = feet + step;
    if (!passable(next)) {
        const glm::vec3 alongX = feet + glm::vec3(step.x, 0.0f, 0.0f);
        const glm::vec3 alongZ = feet + glm::vec3(0.0f, 0.0f, step.z);
        const bool xFirst = std::abs(step.x) > std::abs(step.z);
        if (passable(xFirst ? alongX : alongZ)) next = xFirst ? alongX : alongZ;
        else if (passable(xFirst ? alongZ : alongX)) next = xFirst ? alongZ : alongX;
        else next = feet;
    }

    if (hasGround) {
        next = world.ground->clampInside(next, 1.0f);
        next.y = world.ground->heightAt(next.x, next.z);
    }
    if (world.colliders) world.colliders->resolve(next, stats.radius, stats.height, CLIMBABLE, nullptr);
    if (hasGround) next.y = world.ground->heightAt(next.x, next.z);

    const float wanted = glm::length(glm::vec2(step.x, step.z));
    const bool blocked = flatDistance(next, feet) < 0.35f * wanted;
    if (blocked) {
        // Turn to one side and keep going that way for a while; if that fails too, the other.
        if (npc.detourTime > 0.0f && npc.detourTime < DETOUR_SECONDS - 0.3f) {
            npc.detour = -npc.detour;
        } else if (npc.detourTime <= 0.0f) {
            const glm::vec3 side(-direction.z, 0.0f, direction.x);
            npc.detour = randomRange(0.0f, 1.0f) < 0.5f ? side : -side;
        }
        npc.detourTime = DETOUR_SECONDS;
        if (npc.state == State::Walk) npc.timer = std::min(npc.timer, 0.6f);
    }

    if (turn) npc.yaw = std::atan2(-heading.x, -heading.z);
    npc.stride += speed * deltaTime * 3.0f * std::min(1.0f, 1.2f / stats.height + 0.4f);

    body.setPosition(next);
    body.setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
    animateLimbs(body, npc.stride, std::min(1.0f, speed / 3.0f));
    return !blocked;
}

void GameActors::wander(Npc& npc, float deltaTime) {
    npc.timer -= deltaTime;
    if (npc.timer > 0.0f) return;

    if (npc.state == State::Walk || npc.wanderRadius <= 0.0f) {
        npc.state = State::Idle;
        npc.timer = randomRange(1.0f, 4.0f);
    } else {
        npc.state = State::Walk;
        npc.timer = randomRange(2.0f, 6.0f);
        const float angle = randomRange(0.0f, 6.2831853f);
        const float distance = randomRange(1.5f, npc.wanderRadius);
        npc.target = npc.home + glm::vec3(std::cos(angle) * distance, 0.0f, std::sin(angle) * distance);
    }
}

void GameActors::flyTowards(const CharacterSurroundings& world, Npc& npc, Shape& body, glm::vec3 point, float speed, float deltaTime) {
    // Steers its velocity rather than turning on the spot, slowing as it arrives.
    const glm::vec3 offset = point - body.position;
    const float distance = glm::length(offset);
    const glm::vec3 wanted = distance > 0.01f ? offset / distance * std::min(speed, distance * 2.0f) : glm::vec3(0.0f);
    glm::vec3 change = wanted - npc.velocity;
    const float most = FLIGHT_ACCELERATION * deltaTime;
    if (glm::length(change) > most) change *= most / glm::length(change);
    npc.velocity += change;

    glm::vec3 position = body.position + npc.velocity * deltaTime;
    const float floor = groundAt(world, position);
    if (position.y < floor) {
        position.y = floor;
        npc.velocity.y = std::max(0.0f, npc.velocity.y);
    }

    // Faces where it flies, nose up when climbing, leaning into its turns.
    const glm::vec2 across(npc.velocity.x, npc.velocity.z);
    float turnRate = 0.0f;
    if (glm::length(across) > 0.3f) {
        const float yaw = std::atan2(-across.x, -across.y);
        turnRate = deltaTime > 0.0f ? wrapAngle(yaw - npc.yaw) / deltaTime : 0.0f;
        npc.yaw = yaw;
    }
    const float pitch = std::clamp(std::atan2(npc.velocity.y, glm::length(across)), -0.6f, 0.6f);
    npc.bank += (std::clamp(turnRate * 0.35f, -0.7f, 0.7f) - npc.bank) * std::min(1.0f, deltaTime * 4.0f);

    glm::mat4 world_ = glm::translate(glm::mat4(1.0f), position);
    world_ = glm::rotate(world_, npc.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    world_ = glm::rotate(world_, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    world_ = glm::rotate(world_, npc.bank, glm::vec3(0.0f, 0.0f, 1.0f));
    world_ = glm::scale(world_, body.scale);
    body.setWorldMatrix(world_);
}

void GameActors::updateBird(const CharacterSurroundings& world, Npc& npc, Shape& body, float distance, float deltaTime) {
    const KindStats& stats = STATS[(int)Kind::Bird];
    const bool disturbed = distance < stats.sight || npc.panic > 0.0f;

    if (!npc.airborne) {
        if (disturbed) {
            npc.airborne = true;
            npc.state = State::Flee;
            npc.flightTime = BIRD_FLIGHT_SECONDS * randomRange(0.7f, 1.3f);
            npc.velocity = glm::vec3(0.0f, 4.0f, 0.0f);
            return;
        }
        wander(npc, deltaTime);
        bool moving = false;
        if (npc.state == State::Walk) {
            const glm::vec3 toTarget = npc.target - body.position;
            if (glm::length(glm::vec2(toTarget.x, toTarget.z)) < 0.3f) npc.timer = 0.0f;
            else moving = walk(world, npc, body, flat(toTarget), stats.walkSpeed, deltaTime);
        }
        if (!moving) animateLimbs(body, npc.stride, 0.0f);
        return;
    }

    // Round and round above home, then down again somewhere near it.
    npc.flightTime -= deltaTime;
    npc.wingbeat += deltaTime * 14.0f;
    if (npc.flightTime > 0.0f || (disturbed && npc.flightTime > -6.0f)) {
        npc.orbit += deltaTime * 0.9f;
        glm::vec3 point = npc.home + glm::vec3(std::cos(npc.orbit), 0.0f, std::sin(npc.orbit)) * BIRD_CIRCLE;
        point.y = groundAt(world, point) + BIRD_HEIGHT + 2.0f * std::sin(npc.orbit * 2.0f);
        flyTowards(world, npc, body, point, stats.runSpeed, deltaTime);
    } else {
        if (npc.state != State::Walk) {
            npc.state = State::Walk;
            const float angle = randomRange(0.0f, 6.2831853f);
            npc.target = npc.home + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * randomRange(0.5f, npc.wanderRadius + 0.5f);
            npc.target.y = groundAt(world, npc.target);
        }
        flyTowards(world, npc, body, npc.target, stats.runSpeed * 0.6f, deltaTime);
        if (glm::length(body.position - npc.target) < 0.5f) {
            npc.airborne = false;
            npc.state = State::Idle;
            npc.timer = 2.0f;
            npc.velocity = glm::vec3(0.0f);
            body.setPosition(npc.target);
            body.setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
        }
    }
    animateLimbs(body, npc.stride, 0.0f, npc.wingbeat, 1.0f);
}

void GameActors::breatheFire(SceneGraph& scene, glm::vec3 origin, glm::vec3 target) {
    for (Fire& fire : fires) {
        if (fire.life > 0.0f) continue;
        Shape* shape = scene.findAs<Shape>(fire.id);
        if (!shape) continue;
        const glm::vec3 toward = target - origin;
        const float distance = glm::length(toward);
        if (distance < 0.01f) return;
        // Aimed a little high, for the drop on the way.
        const float seconds = distance / FIRE_SPEED;
        fire.velocity = toward / seconds + glm::vec3(0.0f, 0.5f * FIRE_GRAVITY * seconds, 0.0f);
        fire.life = FIRE_SECONDS;
        fire.burst = 0.0f;
        shape->visible = true;
        shape->setPosition(origin);
        shape->setScale(fire.scale);
        return;
    }
}

float GameActors::updateDragon(SceneGraph& scene, const CharacterSurroundings& world, Npc& npc, Shape& body, glm::vec3 playerFeet,
                               float deltaTime) {
    const KindStats& stats = STATS[(int)Kind::Dragon];
    const glm::vec3 toPlayer = playerFeet - body.position;
    const float distance = glm::length(glm::vec2(toPlayer.x, toPlayer.z));
    const glm::vec3 chest = playerFeet + glm::vec3(0.0f, 0.9f, 0.0f);
    const float size = std::max(body.scale.x, 0.01f);
    float damage = 0.0f;
    npc.breath -= deltaTime;

    auto mouth = [&]() {
        const glm::vec3 heading = npc.airborne && glm::length(npc.velocity) > 0.5f ? glm::normalize(npc.velocity) : forwardOf(npc.yaw);
        return body.position + heading * stats.radius * 2.0f * size + glm::vec3(0.0f, stats.height * 0.6f * size, 0.0f);
    };
    auto takeOff = [&](Flight flight) {
        npc.airborne = true;
        npc.state = State::Chase;
        npc.flight = flight;
        npc.flightTime = 2.5f;
        npc.velocity = glm::vec3(0.0f, 7.0f, 0.0f);
    };

    // Asleep at its lair until someone comes near, or hurts it.
    if (!npc.airborne && npc.state == State::Idle) {
        if (distance < stats.sight || npc.panic > 0.0f) {
            takeOff(Flight::Climb);
        } else {
            animateLimbs(body, npc.stride, 0.0f);
            return 0.0f;
        }
    }

    // Too far away: back to the lair, to sleep and heal.
    if (distance > stats.sight * DRAGON_GIVES_UP && npc.flight != Flight::Return) {
        if (!npc.airborne) takeOff(Flight::Return);
        npc.flight = Flight::Return;
        npc.state = State::Walk;
    }

    if (npc.airborne) {
        npc.wingbeat += deltaTime * (npc.flight == Flight::Swoop ? 3.0f : 5.0f);
        switch (npc.flight) {
            case Flight::Circle: {
                npc.orbit += deltaTime * stats.runSpeed * 0.8f / DRAGON_ORBIT;
                glm::vec3 point = playerFeet + glm::vec3(std::cos(npc.orbit), 0.0f, std::sin(npc.orbit)) * DRAGON_ORBIT;
                point.y = std::max(groundAt(world, point), playerFeet.y) + DRAGON_ALTITUDE;
                flyTowards(world, npc, body, point, stats.runSpeed, deltaTime);
                if (npc.breath <= 0.0f && distance < stats.sight * 1.3f) {
                    breatheFire(scene, mouth(), chest + playerVelocity * FIRE_LEAD_SECONDS);
                    npc.breath = randomRange(2.4f, 3.4f);
                }
                npc.flightTime -= deltaTime;
                if (npc.health < stats.health * DRAGON_LANDS_BELOW) {
                    npc.flight = Flight::Land;
                    npc.target = playerFeet + flat(body.position - playerFeet) * 9.0f;
                    npc.target.y = groundAt(world, npc.target);
                } else if (npc.flightTime <= 0.0f) {
                    npc.flight = Flight::Swoop;
                    npc.flightTime = 4.5f;
                }
                break;
            }
            case Flight::Swoop: {
                flyTowards(world, npc, body, chest + glm::vec3(0.0f, 0.4f, 0.0f), stats.runSpeed * 1.25f, deltaTime);
                npc.flightTime -= deltaTime;
                const bool close = glm::length(body.position + glm::vec3(0.0f, size, 0.0f) - chest) < stats.reach * size;
                if (close && npc.attackTimer <= 0.0f) {
                    damage += stats.damage;
                    npc.attackTimer = stats.attackSeconds;
                    sound(ActorSound::Kind::Attack, npc, body);
                    playerPush += flat(toPlayer) * stats.shove + glm::vec3(0.0f, 4.0f, 0.0f);
                }
                if (close || npc.flightTime <= 0.0f) {
                    npc.flight = Flight::Climb;
                    npc.flightTime = 2.2f;
                }
                break;
            }
            case Flight::Climb: {
                glm::vec3 ahead = body.position + flat(npc.velocity) * 12.0f;
                ahead.y = groundAt(world, ahead) + DRAGON_ALTITUDE + 4.0f;
                flyTowards(world, npc, body, ahead, stats.runSpeed, deltaTime);
                npc.flightTime -= deltaTime;
                if (npc.flightTime <= 0.0f) {
                    npc.flight = Flight::Circle;
                    npc.flightTime = randomRange(7.0f, 10.0f);
                    npc.orbit = std::atan2(body.position.z - playerFeet.z, body.position.x - playerFeet.x);
                }
                break;
            }
            case Flight::Land: {
                flyTowards(world, npc, body, npc.target, stats.runSpeed * 0.6f, deltaTime);
                if (body.position.y - groundAt(world, body.position) < 0.3f && flatDistance(body.position, npc.target) < 2.5f) {
                    npc.airborne = false;
                    npc.state = State::Chase;
                    npc.velocity = glm::vec3(0.0f);
                    body.setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
                }
                break;
            }
            case Flight::Return: {
                if (distance < stats.sight) {
                    npc.state = State::Chase;
                    npc.flight = Flight::Climb;
                    npc.flightTime = 2.0f;
                    break;
                }
                const bool over = flatDistance(body.position, npc.home) < 10.0f;
                glm::vec3 point = npc.home;
                if (!over) point.y = std::max(groundAt(world, body.position), npc.home.y) + DRAGON_ALTITUDE;
                flyTowards(world, npc, body, point, stats.runSpeed, deltaTime);
                if (over && glm::length(body.position - npc.home) < 1.5f) {
                    npc.airborne = false;
                    npc.state = State::Idle;
                    npc.flight = Flight::Circle;
                    npc.health = stats.health;
                    npc.velocity = glm::vec3(0.0f);
                    body.setPosition(npc.home);
                    body.setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
                }
                break;
            }
        }
        if (npc.airborne) animateLimbs(body, npc.stride, 0.0f, npc.wingbeat, 1.0f);
        return damage;
    }

    // On the ground: it walks at the player and bites, and breathes fire from further off.
    if (distance > stats.reach * 0.8f * size) {
        walk(world, npc, body, flat(toPlayer), stats.walkSpeed, deltaTime);
    } else {
        npc.yaw = std::atan2(-toPlayer.x, -toPlayer.z);
        body.setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
        animateLimbs(body, npc.stride, 0.0f);
    }
    if (distance < stats.reach * size && npc.attackTimer <= 0.0f) {
        damage += stats.damage;
        npc.attackTimer = stats.attackSeconds;
        sound(ActorSound::Kind::Attack, npc, body);
        playerPush += flat(toPlayer) * stats.shove;
    }
    if (npc.breath <= 0.0f && distance > 6.0f && distance < 30.0f) {
        breatheFire(scene, mouth(), chest);
        npc.breath = randomRange(3.0f, 4.5f);
    }
    return damage;
}

void GameActors::updateFallen(const CharacterSurroundings& world, Npc& npc, Shape& body, float deltaTime) {
    const KindStats& stats = STATS[(int)npc.kind];
    glm::vec3 position = body.position;
    const float floor = groundAt(world, position);

    // A flier drops out of the sky first.
    if (npc.airborne) {
        npc.velocity.y -= 20.0f * deltaTime;
        position += npc.velocity * deltaTime;
        if (position.y <= floor) {
            position.y = floor;
            npc.airborne = false;
            npc.velocity = glm::vec3(0.0f);
        }
        body.setPosition(position);
        return;
    }
    if (npc.fallen >= 1.0f) return;

    npc.fallen = std::min(1.0f, npc.fallen + FALL_RATE * deltaTime);
    const float eased = npc.fallen * npc.fallen * (3.0f - 2.0f * npc.fallen);
    const float yaw = glm::degrees(npc.yaw);
    const float size = std::max(body.scale.x, 0.01f);
    if (stats.upright) {
        body.setRotation(glm::vec3(90.0f * eased, yaw, 0.0f));
        position.y = floor + 0.25f * eased;
    } else {
        body.setRotation(glm::vec3(0.0f, yaw, 90.0f * eased));
        position.y = floor + stats.radius * 0.6f * size * eased;
    }
    body.setPosition(position);
}

void GameActors::separate(SceneGraph& scene, const CharacterSurroundings& world, glm::vec3 playerFeet) {
    struct Standing {
        Shape* body;
        float radius;
        float height;
        bool moved;
    };
    std::vector<Standing> standing;
    for (const Npc& npc : npcs) {
        if (npc.state == State::Dead || npc.airborne) continue;
        Shape* body = scene.findAs<Shape>(npc.id);
        if (!body || flatDistance(body->position, playerFeet) > IDLE_DISTANCE) continue;
        const float size = std::max(body->scale.x, 0.01f);
        standing.push_back({body, STATS[(int)npc.kind].radius * size, STATS[(int)npc.kind].height * size, false});
    }

    for (size_t i = 0; i < standing.size(); i++) {
        Standing& a = standing[i];
        glm::vec3 position = a.body->position;

        // Solid collision: NPCs stand firmly on their ground!
        // The PLAYER is blocked and pushed back, instead of sliding the NPC like a hockey puck.
        glm::vec2 offset(position.x - playerFeet.x, position.z - playerFeet.z);
        float distance = glm::length(offset);
        float apart = a.radius + PLAYER_RADIUS;
        if (distance < apart && distance > 1e-4f) {
            glm::vec2 normal = offset / distance;
            float penetration = apart - distance;
            playerPush -= glm::vec3(normal.x, 0.0f, normal.y) * penetration * 30.0f;
        }

        for (size_t j = i + 1; j < standing.size(); j++) {
            Standing& b = standing[j];
            offset = glm::vec2(position.x - b.body->position.x, position.z - b.body->position.z);
            distance = glm::length(offset);
            apart = a.radius + b.radius;
            if (distance >= apart || distance < 1e-4f) continue;
            const glm::vec3 push = glm::vec3(offset.x, 0.0f, offset.y) / distance * (apart - distance) * 0.5f;
            position += push;
            b.body->setPosition(b.body->position - push);
            a.moved = b.moved = true;
        }
        a.body->setPosition(position);
    }

    for (Standing& s : standing) {
        if (!s.moved) continue;
        glm::vec3 feet = s.body->position;
        if (world.colliders) world.colliders->resolve(feet, s.radius, s.height, CLIMBABLE, nullptr);
        if (world.ground && world.ground->isValid()) feet.y = world.ground->heightAt(feet.x, feet.z);
        s.body->setPosition(feet);
    }

    // Heavy horse collision: horses (500kg) block the player solidly and cannot be pushed like pucks.
    for (const auto& shape : scene.getShapes()) {
        if (shape->gameRole == GameRole::Horse && shape->visible) {
            glm::vec2 offset(shape->position.x - playerFeet.x, shape->position.z - playerFeet.z);
            float dist = glm::length(offset);
            const float horseRadius = 1.15f * std::max(shape->scale.x, 0.01f);
            float apart = horseRadius + PLAYER_RADIUS;
            if (dist < apart && dist > 1e-4f) {
                glm::vec2 normal = offset / dist;
                float penetration = apart - dist;
                playerPush -= glm::vec3(normal.x, 0.0f, normal.y) * penetration * 35.0f;
            }
        }
    }
}

float GameActors::update(SceneGraph& scene, const CharacterSurroundings& world, glm::vec3 playerFeet, bool night, float deltaTime) {
    float damage = 0.0f;
    if (deltaTime > 0.0f) {
        playerVelocity = (playerFeet - lastPlayerFeet) / deltaTime;
        if (glm::length(playerVelocity) > 30.0f) playerVelocity = glm::vec3(0.0f);   // a respawn, not a run
    }
    lastPlayerFeet = playerFeet;

    for (Npc& npc : npcs) {
        Shape* body = scene.findAs<Shape>(npc.id);
        if (!body) continue;
        const KindStats& stats = STATS[(int)npc.kind];

        if (npc.flash > 0.0f) {
            npc.flash -= deltaTime;
            tint(*body, npc.flash > 0.0f ? glm::vec3(1.0f, 0.25f, 0.2f) : glm::vec3(1.0f));
        }

        if (npc.state == State::Dead) {
            updateFallen(world, npc, *body, deltaTime);
            continue;
        }

        const glm::vec3 toPlayer = playerFeet - body->position;
        const float distance = glm::length(glm::vec2(toPlayer.x, toPlayer.z));
        if (distance > IDLE_DISTANCE && npc.state != State::Chase && !npc.airborne) continue;

        npc.attackTimer = std::max(0.0f, npc.attackTimer - deltaTime);
        npc.panic = std::max(0.0f, npc.panic - deltaTime);

        if (glm::length(npc.knock) > 0.1f) {
            if (!npc.airborne) walk(world, npc, *body, glm::normalize(npc.knock), glm::length(npc.knock), deltaTime, false);
            npc.knock *= std::exp(-KNOCK_FADE * deltaTime);
        }

        if (npc.hold > 0.0f) {
            npc.hold -= deltaTime;
            npc.state = State::Idle;
            const glm::vec3 facing = flat(npc.lookAt - body->position);
            if (glm::length(facing) > 0.0f) npc.yaw = std::atan2(-facing.x, -facing.z);
            body->setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
            animateLimbs(*body, npc.stride, 0.0f);
            continue;
        }

        if (npc.kind == Kind::Dragon) {
            damage += updateDragon(scene, world, npc, *body, playerFeet, deltaTime);
            continue;
        }
        if (npc.kind == Kind::Bird) {
            updateBird(world, npc, *body, distance, deltaTime);
            continue;
        }

        if (stats.hostile) {
            const float sight = stats.sight * (night && npc.kind == Kind::Wolf ? WOLF_NIGHT_SIGHT : 1.0f);
            if (distance < sight && npc.state != State::Chase) {
                npc.state = State::Chase;
                if (npc.kind == Kind::Wolf) {
                    for (Npc& other : npcs) {
                        const Shape* otherBody = scene.findAs<Shape>(other.id);
                        if (other.kind == Kind::Wolf && other.state != State::Dead && otherBody &&
                            glm::length(otherBody->position - body->position) < PACK_RANGE) {
                            other.state = State::Chase;
                        }
                    }
                }
            } else if (npc.state == State::Chase && distance > sight * LOSE_SIGHT) {
                npc.state = State::Walk;
                npc.target = npc.home;
                npc.timer = 8.0f;
            }

            if (npc.state == State::Chase) {
                if (distance > stats.reach * 0.8f) {
                    walk(world, npc, *body, flat(toPlayer), stats.runSpeed, deltaTime);
                } else {
                    npc.yaw = std::atan2(-toPlayer.x, -toPlayer.z);
                    body->setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
                    animateLimbs(*body, npc.stride, 0.0f);
                }
                if (distance < stats.reach && npc.attackTimer <= 0.0f) {
                    npc.attackTimer = stats.attackSeconds;
                    damage += stats.damage;
                    sound(ActorSound::Kind::Attack, npc, *body);
                    playerPush += flat(toPlayer) * stats.shove;
                }
                continue;
            }
        } else {
            bool scared = npc.panic > 0.0f;
            glm::vec3 danger = playerFeet;

            if (stats.fleesPlayer && distance < stats.sight) {
                scared = true;
            }
            if (npc.kind == Kind::Villager || npc.kind == Kind::Deer || npc.kind == Kind::Rabbit) {
                for (const Npc& other : npcs) {
                    if (other.state != State::Chase) continue;
                    if (const Shape* hunter = scene.findAs<Shape>(other.id)) {
                        if (flatDistance(hunter->position, body->position) < FEAR_RANGE) {
                            danger = hunter->position;
                            scared = true;
                        }
                    }
                }
            }

            if (scared) {
                if (npc.state != State::Flee) sound(ActorSound::Kind::Alarm, npc, *body);
                npc.state = State::Flee;
                walk(world, npc, *body, flat(body->position - danger), stats.runSpeed, deltaTime);
                continue;
            }
            if (npc.state == State::Flee) {
                npc.state = State::Idle;
                npc.timer = 0.5f;
                // Somewhere new to settle, rather than straight back to the danger.
                npc.home = body->position;
            }
        }

        // Villagers spend the night at home.
        if (night && npc.kind == Kind::Villager && npc.wanderRadius > 0.0f) {
            npc.target = npc.home;
            npc.state = State::Walk;
            npc.timer = 1.0f;
        } else {
            wander(npc, deltaTime);
        }

        bool moving = false;
        if (npc.state == State::Walk) {
            const glm::vec3 toTarget = npc.target - body->position;
            if (glm::length(glm::vec2(toTarget.x, toTarget.z)) < 0.4f) {
                npc.timer = night ? 1.0f : 0.0f;
                if (night) npc.state = State::Idle;
            } else {
                moving = walk(world, npc, *body, flat(toTarget), stats.walkSpeed, deltaTime);
            }
        }
        if (!moving && npc.state != State::Walk) {
            // Natural idle actions: breathing, shifting weight from foot to foot, and turning to look at player
            const float animTime = scene.getAnimationTime();
            const float phase = float(npc.id) * 2.399f;
            const float breath = std::sin(animTime * 1.6f + phase);
            animateLimbs(*body, animTime * 1.6f + phase, 0.08f * std::abs(breath));

            // Subtle posture / weight shift
            glm::vec3 rot = body->rotation;
            rot.z = std::sin(animTime * 0.8f + phase) * 1.0f;

            // Player awareness: smoothly face the player when they approach
            const glm::vec3 toPl = playerFeet - body->position;
            const float pDist = glm::length(glm::vec2(toPl.x, toPl.z));
            if (pDist < 5.5f && pDist > 0.5f) {
                float targetYaw = std::atan2(-toPl.x, -toPl.z);
                float diff = targetYaw - npc.yaw;
                while (diff > 3.14159f) diff -= 6.28318f;
                while (diff < -3.14159f) diff += 6.28318f;
                npc.yaw += diff * std::min(1.0f, 2.5f * deltaTime);
                rot.y = glm::degrees(npc.yaw);
            }
            body->setRotation(rot);
        }
    }

    separate(scene, world, playerFeet);
    updateArrows(scene, world, deltaTime);
    damage += updateFires(scene, world, playerFeet, deltaTime);
    return damage;
}

void GameActors::sound(ActorSound::Kind kind, const Npc& npc, const Shape& body) {
    sounds.push_back({kind, roleOf((int)npc.kind), body.position});
}

void GameActors::hurt(SceneGraph& scene, Npc& npc, float damage, Shape& body, glm::vec3 from) {
    const KindStats& stats = STATS[(int)npc.kind];
    npc.health -= damage;
    npc.flash = 0.15f;
    if (npc.health > 0.0f) sound(ActorSound::Kind::Hurt, npc, body);
    if (!npc.airborne) npc.knock = flat(body.position - from) * KNOCKBACK * std::min(1.0f, 0.45f / stats.radius);

    // A fight frightens every villager and animal that can see it.
    for (Npc& other : npcs) {
        if (other.kind == Kind::Villager || other.kind == Kind::Livestock || other.kind == Kind::Deer ||
            other.kind == Kind::Rabbit || other.kind == Kind::Bird) {
            if (const Shape* otherBody = scene.findAs<Shape>(other.id)) {
                if (glm::length(otherBody->position - body.position) < 20.0f) other.panic = PANIC_SECONDS;
            }
        }
    }

    if (npc.health <= 0.0f) {
        npc.state = State::Dead;
        npc.knock = glm::vec3(0.0f);
        npc.fallen = 0.0f;
        npc.velocity *= 0.5f;
        if (npc.airborne) body.setRotation(glm::vec3(0.0f, glm::degrees(npc.yaw), 0.0f));
        events.push_back({roleOf((int)npc.kind), body.name, body.position});
        sound(ActorSound::Kind::Death, npc, body);
    } else if (npc.kind == Kind::Dragon) {
        if (npc.state == State::Idle) npc.panic = 1.0f;   // woken
    } else if (stats.hostile) {
        npc.state = State::Chase;
    }
}

int GameActors::strike(SceneGraph& scene, glm::vec3 origin, glm::vec3 forward, float range, float damage) {
    int hits = 0;
    const glm::vec3 facing = flat(forward);

    auto inCone = [&](glm::vec3 offset, float radius) {
        const float distance = glm::length(glm::vec2(offset.x, offset.z));
        return distance <= range + radius * 0.6f && std::abs(offset.y) < 3.0f &&
               (distance < 0.8f + radius || glm::dot(flat(offset), facing) > 0.45f);
    };

    for (Npc& npc : npcs) {
        if (npc.state == State::Dead) continue;
        Shape* body = scene.findAs<Shape>(npc.id);
        if (body && inCone(body->position - origin, STATS[(int)npc.kind].radius * std::max(body->scale.x, 0.01f))) {
            hurt(scene, npc, damage, *body, origin);
            hits++;
        }
    }

    for (Shape* prop : props) {
        if (inCone(prop->getWorldPosition() - origin, 0.0f)) {
            prop->velocity += facing * PROP_PUSH + glm::vec3(0.0f, 2.0f, 0.0f);
        }
    }
    return hits;
}

bool GameActors::fireArrow(SceneGraph& scene, glm::vec3 origin, glm::vec3 velocity, float damage) {
    Arrow* chosen = nullptr;
    for (Arrow& arrow : arrows) {
        if (arrow.life <= 0.0f) {
            chosen = &arrow;
            break;
        }
    }
    if (!chosen) {
        // All in use: the oldest stuck arrow is the least missed.
        for (Arrow& arrow : arrows) {
            if (!arrow.flying && (!chosen || arrow.life < chosen->life)) chosen = &arrow;
        }
    }
    if (!chosen) return false;

    Shape* shape = scene.findAs<Shape>(chosen->id);
    if (!shape) return false;

    chosen->velocity = velocity;
    chosen->damage = damage;
    chosen->life = ARROW_STUCK_SECONDS;
    chosen->flying = true;
    shape->visible = true;
    shape->setPosition(origin);
    return true;
}

void GameActors::updateArrows(SceneGraph& scene, const CharacterSurroundings& world, float deltaTime) {
    for (Arrow& arrow : arrows) {
        if (arrow.life <= 0.0f) continue;

        Shape* shape = scene.findAs<Shape>(arrow.id);
        if (!shape) continue;

        if (!arrow.flying) {
            arrow.life -= deltaTime;
            if (arrow.life <= 0.0f) shape->visible = false;
            continue;
        }

        arrow.velocity.y -= ARROW_GRAVITY * deltaTime;
        const glm::vec3 start = shape->position;
        glm::vec3 position = start + arrow.velocity * deltaTime;

        const glm::vec3 up = std::abs(glm::normalize(arrow.velocity).y) > 0.98f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);

        // Along the whole way it went this frame, so that a fast arrow does not pass through.
        bool stopped = false;
        const int samples = 4;
        for (int i = 1; i <= samples && !stopped; i++) {
            const glm::vec3 point = glm::mix(start, position, (float)i / samples);

            for (Npc& npc : npcs) {
                if (npc.state == State::Dead) continue;
                Shape* body = scene.findAs<Shape>(npc.id);
                if (!body) continue;
                const KindStats& stats = STATS[(int)npc.kind];
                const float size = std::max(body->scale.x, 0.01f);
                const glm::vec3 offset = point - body->position;
                if (glm::length(glm::vec2(offset.x, offset.z)) < stats.radius * size + 0.15f &&
                    offset.y > -0.2f && offset.y < stats.height * size) {
                    hurt(scene, npc, arrow.damage, *body, point - arrow.velocity);
                    stopped = true;
                    arrow.life = 0.0f;
                    shape->visible = false;
                    break;
                }
            }
            if (stopped) break;

            if (world.ground && world.ground->isValid() && point.y < world.ground->heightAt(point.x, point.z)) {
                stopped = true;
            } else if (world.colliders) {
                if (const SolidBox* hit = world.colliders->firstContaining(point, player)) {
                    stopped = true;
                    if (!hit->shape->isStatic) hit->shape->velocity += glm::normalize(arrow.velocity) * 2.5f;
                }
            }
            if (stopped) position = point;
        }

        shape->setWorldMatrix(glm::inverse(glm::lookAt(position, position + arrow.velocity, up)));
        if (stopped) arrow.flying = false;
    }
}

float GameActors::updateFires(SceneGraph& scene, const CharacterSurroundings& world, glm::vec3 playerFeet, float deltaTime) {
    float damage = 0.0f;
    const glm::vec3 chest = playerFeet + glm::vec3(0.0f, 0.9f, 0.0f);

    for (Fire& fire : fires) {
        if (fire.life <= 0.0f) continue;
        Shape* shape = scene.findAs<Shape>(fire.id);
        if (!shape) continue;

        if (fire.burst > 0.0f) {
            // A flash that swells and is gone.
            fire.burst -= deltaTime;
            const float grown = 1.0f + 2.5f * (1.0f - fire.burst / FIRE_BURST_SECONDS);
            shape->setScale(fire.scale * grown);
            if (fire.burst <= 0.0f) {
                fire.life = 0.0f;
                shape->visible = false;
                shape->setScale(fire.scale);
            }
            continue;
        }

        fire.life -= deltaTime;
        fire.velocity.y -= FIRE_GRAVITY * deltaTime;
        const glm::vec3 position = shape->position + fire.velocity * deltaTime;
        shape->setPosition(position);
        shape->rotate(720.0f * deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));

        const bool direct = glm::length(position - chest) < 1.3f;
        const bool landed = (world.ground && world.ground->isValid() && position.y < world.ground->heightAt(position.x, position.z)) ||
                            (world.colliders && world.colliders->firstContaining(position, player)) || fire.life <= 0.0f;
        if (direct) {
            damage += FIRE_DAMAGE;
            playerPush += flat(fire.velocity) * 3.0f;
        } else if (landed && glm::length(position - chest) < FIRE_SPLASH) {
            damage += FIRE_DAMAGE * 0.6f;
        }
        if (direct || landed) fire.burst = FIRE_BURST_SECONDS;
    }
    return damage;
}

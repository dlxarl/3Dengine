#include "GameAudio.h"
#include "Shape.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
    const float SCAN_SECONDS = 0.25f;
    const float TOWN_NEAR = 22.0f;
    const float TOWN_FAR = 75.0f;

    const float STEP_VOLUME = 0.5f;
    const float RUN_STEP_VOLUME = 0.75f;
    const float RUN_SPEED_THRESHOLD = 8.0f;
    const float HOOF_FULL_SPEED = 13.0f;

    const float BELFRY_HEIGHT = 9.0f;
    const int BELL_HOURS[] = {6, 12, 18};

    std::string lower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return text;
    }

    bool startsWith(const std::string& text, const char* prefix) {
        return text.rfind(prefix, 0) == 0;
    }

    bool endsWith(const std::string& text, const char* suffix) {
        const std::string tail(suffix);
        return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
    }

    bool hasAny(const std::string& text, std::initializer_list<const char*> words) {
        return std::any_of(words.begin(), words.end(), [&](const char* word) { return text.find(word) != std::string::npos; });
    }

    float smoothstep(float low, float high, float value) {
        const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // 0 by day, 1 by night, easing through the hours of dusk and dawn.
    float nightAt(float hour) {
        if (hour >= 21.0f || hour < 5.0f) return 1.0f;
        if (hour >= 19.0f) return (hour - 19.0f) / 2.0f;
        if (hour < 7.0f) return (7.0f - hour) / 2.0f;
        return 0.0f;
    }

    const char* surfaceName(Surface surface) {
        switch (surface) {
            case Surface::Stone: return "stone";
            case Surface::Wood:  return "wood";
            case Surface::Sand:  return "sand";
            case Surface::Snow:  return "snow";
            default:             return "grass";
        }
    }

    float nearestOf(const std::vector<glm::vec3>& places, glm::vec3 point, glm::vec3* found = nullptr) {
        float best = 1e9f;
        for (const glm::vec3& place : places) {
            const float distance = glm::length(place - point);
            if (distance < best) {
                best = distance;
                if (found) *found = place;
            }
        }
        return best;
    }
}

GameAudio::GameAudio(AudioEngine& audioEngine) : engine(audioEngine) {}

std::string GameAudio::path(const std::string& relative) const {
    return (directory / relative).string();
}

float GameAudio::range(float low, float high) {
    return std::uniform_real_distribution<float>(low, high)(random);
}

int GameAudio::pick(int count) {
    return std::uniform_int_distribution<int>(0, count - 1)(random);
}

void GameAudio::begin(const std::filesystem::path& audioDirectory, const SceneGraph& scene) {
    end();

    std::error_code error;
    const bool hasDirectory = std::filesystem::is_directory(audioDirectory, error);
    if (!hasDirectory && scene.getAudioSources().empty()) return;
    directory = audioDirectory;
    projectRoot = audioDirectory.parent_path().parent_path();
    active = true;

    sources.clear();
    for (const auto& source : scene.getAudioSources()) {
        SourceState state;
        state.id = source->id;
        sources.push_back(state);
        if (!source->clip.empty()) engine.preload(sourcePath(source->clip));
    }

    hooves.file = path("horse/horse-galloping.wav");

    river = Emitter();
    river.file = path("outdoor/river.wav");
    river.nearDistance = 5.0f;
    river.farDistance = 55.0f;
    river.dayVolume = river.nightVolume = 0.8f;

    lake = Emitter();
    lake.file = path("outdoor/lake.wav");
    lake.nearDistance = 8.0f;
    lake.farDistance = 65.0f;
    lake.dayVolume = lake.nightVolume = 0.55f;

    fire = Emitter();
    fire.file = path("outdoor/fire.wav");
    fire.nearDistance = 2.5f;
    fire.farDistance = 28.0f;
    fire.dayVolume = 0.6f;
    fire.nightVolume = 0.9f;

    townNoise = Emitter();
    townNoise.file = path("town/town-ambience.wav");
    townNoise.nearDistance = TOWN_NEAR;
    townNoise.farDistance = TOWN_FAR;
    townNoise.dayVolume = 0.7f;
    townNoise.nightVolume = 0.35f;

    crowd = Emitter();
    crowd.file = path("town/crowd.wav");
    crowd.nearDistance = 10.0f;
    crowd.farDistance = 45.0f;
    crowd.dayVolume = 0.5f;
    crowd.nightVolume = 0.0f;

    music = Emitter();
    music.file = path("town/ambience-music.wav");
    music.nearDistance = 3.0f;
    music.farDistance = 40.0f;
    music.dayVolume = music.nightVolume = 0.7f;

    town.clear();
    chapels.clear();
    musicians.clear();
    sheep.clear();
    horses.clear();

    riverBodies.clear();
    lakeBodies.clear();
    for (const auto& water : scene.getWaters()) {
        const bool river = lower(water->name).find("river") != std::string::npos || glm::length(water->flow) > 0.1f;
        (river ? riverBodies : lakeBodies).push_back(water->id);
    }
    for (const auto& fire : scene.getFires()) this->fire.places.push_back(fire->getWorldPosition());

    for (const auto& shape : scene.getShapes()) {
        const std::string name = lower(shape->name);
        const glm::vec3 place = shape->getWorldPosition();

        if (shape->gameRole == GameRole::Fireball || name.find("dragon") != std::string::npos) {
            // not a place with a sound of its own
        } else if (startsWith(name, "river")) {
            river.places.push_back(place);
        } else if (startsWith(name, "lake")) {
            lake.places.push_back(place);
        } else if (endsWith(name, "fire") || hasAny(name, {"campfire", "bonfire"})) {
            fire.places.push_back(place);
        } else if (hasAny(name, {"minstrel", "bard", "musician"})) {
            musicians.push_back(shape->id);
        } else if (name.find("chapel") != std::string::npos && name.find("window") == std::string::npos) {
            chapels.push_back(place);
            town.push_back(place);
        } else if (hasAny(name, {"house", "cottage", "stall", "well", "tavern", "smithy", "forge"}) && name.find("window") == std::string::npos) {
            town.push_back(place);
        }
        if (shape->gameRole == GameRole::Livestock && name.find("sheep") != std::string::npos) sheep.push_back(shape->id);
        if (shape->gameRole == GameRole::Horse) horses.push_back(shape->id);
    }
    townNoise.places = crowd.places = town;

    // The loops that matter first are read now, so that no frame stalls on a long file.
    if (!river.places.empty() || !riverBodies.empty()) engine.preload(river.file);
    if (!town.empty()) engine.preload(townNoise.file);
    if (!musicians.empty()) engine.preload(music.file);

    scanTimer = 0.0f;
    lastBellHour = -1;
    bleatTimer = range(8.0f, 20.0f);
    snortTimer = range(10.0f, 25.0f);
    pigeonTimer = range(6.0f, 16.0f);
}

std::string GameAudio::sourcePath(const std::string& clip) const {
    const std::filesystem::path file = std::filesystem::u8path(clip);
    if (file.is_absolute()) return file.string();
    if (clip.rfind("Assets/", 0) == 0) return (projectRoot / file).string();
    return (directory / file).string();
}

void GameAudio::updateSources(const SceneGraph& scene, float deltaTime) {
    for (SourceState& state : sources) {
        const auto* source = scene.findAs<AudioSourceObject>(state.id);
        if (state.handle != NoSound && !engine.isPlaying(state.handle)) state.handle = NoSound;

        const bool on = source && source->isVisibleInHierarchy() && !source->muted && !source->clip.empty() && source->volume > 0.001f;
        if (!on) {
            if (state.handle != NoSound) engine.stopSound(state.handle, 0.2f);
            state.handle = NoSound;
            continue;
        }
        if (state.failed == source->clip) continue;
        const glm::vec3 place = source->getWorldPosition();

        if (state.handle != NoSound) {   // playing: follow the object and its volume
            engine.setVolume(state.handle, source->volume);
            if (source->spatial) engine.setPosition(state.handle, place);
            continue;
        }
        if (!state.started) {
            state.started = true;
            state.timer = source->startDelay;
        }
        if (!source->loop && state.done && source->repeatMax <= 0.0f) continue;
        state.timer -= deltaTime;
        if (state.timer > 0.0f) continue;

        SoundOptions options;
        options.volume = source->volume;
        options.pitch = source->pitch * (1.0f + range(-source->pitchVariation, source->pitchVariation));
        options.loop = source->loop;
        options.fadeIn = source->fadeIn;
        options.spatial = source->spatial;
        options.position = place;
        options.nearDistance = source->nearDistance;
        options.farDistance = source->farDistance;
        state.handle = engine.play(sourcePath(source->clip), options);
        if (state.handle == NoSound) {
            state.failed = source->clip;
        } else if (!source->loop) {
            state.timer = source->repeatMax > 0.0f ? range(source->repeatMin, source->repeatMax) : 0.0f;
            state.done = source->repeatMax <= 0.0f;
        }
    }
}

void GameAudio::stopLayer(Layer& layer) {
    if (layer.handle != NoSound) engine.stopSound(layer.handle, 0.3f);
    layer.handle = NoSound;
    layer.level = 0.0f;
}

void GameAudio::stopEmitter(Emitter& emitter) {
    if (emitter.handle != NoSound) engine.stopSound(emitter.handle, 0.3f);
    emitter.handle = NoSound;
}

void GameAudio::end() {
    if (!active) return;
    stopLayer(hooves);
    for (Emitter* emitter : {&river, &lake, &fire, &townNoise, &crowd, &music}) stopEmitter(*emitter);
    engine.stopAll(0.4f);
    sources.clear();
    active = false;
}

void GameAudio::drive(Layer& layer, float target, float deltaTime, float fadeSeconds) {
    const float step = deltaTime / fadeSeconds;
    layer.level = layer.level < target ? std::min(target, layer.level + step) : std::max(target, layer.level - step);

    if (layer.handle != NoSound && !engine.isPlaying(layer.handle)) layer.handle = NoSound;
    if (layer.level > 0.002f) {
        if (layer.handle == NoSound) {
            SoundOptions options;
            options.loop = true;
            options.volume = layer.level;
            layer.handle = engine.play(layer.file, options);
        } else {
            engine.setVolume(layer.handle, layer.level);
        }
    } else if (layer.handle != NoSound) {
        engine.stopSound(layer.handle, 0.2f);
        layer.handle = NoSound;
    }
}

void GameAudio::driveEmitter(Emitter& emitter, float volume) {
    if (emitter.handle != NoSound && !engine.isPlaying(emitter.handle)) emitter.handle = NoSound;

    if (emitter.distance < emitter.farDistance && volume > 0.002f) {
        if (emitter.handle == NoSound) {
            SoundOptions options;
            options.loop = true;
            options.spatial = true;
            options.volume = volume;
            options.position = emitter.nearest;
            options.nearDistance = emitter.nearDistance;
            options.farDistance = emitter.farDistance;
            options.fadeIn = 1.0f;
            emitter.handle = engine.play(emitter.file, options);
        } else {
            engine.setPosition(emitter.handle, emitter.nearest);
            engine.setVolume(emitter.handle, volume);
        }
    } else if (emitter.handle != NoSound) {
        engine.stopSound(emitter.handle, 1.0f);
        emitter.handle = NoSound;
    }
}

void GameAudio::playAt(const std::string& relative, glm::vec3 position, float volume, float pitch, float nearDistance,
                       float farDistance) {
    SoundOptions options;
    options.volume = volume;
    options.pitch = pitch;
    options.spatial = true;
    options.position = position;
    options.nearDistance = nearDistance;
    options.farDistance = farDistance;
    engine.play(path(relative), options);
}

void GameAudio::playHere(const std::string& relative, float volume, float pitch) {
    SoundOptions options;
    options.volume = volume;
    options.pitch = pitch;
    engine.play(path(relative), options);
}

void GameAudio::rescan(const SceneGraph& scene, const GameSession& game, glm::vec3 listener) {
    townness = 1.0f - smoothstep(TOWN_NEAR, TOWN_FAR, nearestOf(town, listener));

    // A musician plays where they are standing, for as long as they live.
    music.places.clear();
    for (ObjectId id : musicians) {
        const Shape* player = scene.findAs<Shape>(id);
        if (player && player->isVisibleInHierarchy() && game.isAlive(id)) music.places.push_back(player->getWorldPosition());
    }

    for (Emitter* emitter : {&river, &lake, &fire, &townNoise, &crowd, &music}) {
        emitter->distance = emitter->places.empty() ? 1e9f : nearestOf(emitter->places, listener, &emitter->nearest);
    }

    // A body of water is heard from its nearest edge, not from its middle.
    auto nearestWater = [&](const std::vector<ObjectId>& bodies, Emitter& emitter) {
        for (ObjectId id : bodies) {
            const WaterObject* water = scene.findAs<WaterObject>(id);
            if (!water || !water->isVisibleInHierarchy()) continue;
            const glm::vec3 center = water->getWorldPosition();
            const glm::vec2 half = 0.5f * water->size * glm::vec2(water->getWorldScale().x, water->getWorldScale().z);
            glm::vec2 offset = glm::vec2(listener.x - center.x, listener.z - center.z);
            if (water->round) {
                const glm::vec2 unit = offset / half;
                const float length = glm::length(unit);
                if (length > 1.0f) offset = unit / length * half;
            } else {
                offset = glm::clamp(offset, -half, half);
            }
            const glm::vec3 edge(center.x + offset.x, center.y, center.z + offset.y);
            const float distance = glm::length(edge - listener);
            if (distance < emitter.distance) {
                emitter.distance = distance;
                emitter.nearest = edge;
            }
        }
    };
    nearestWater(riverBodies, river);
    nearestWater(lakeBodies, lake);
}

void GameAudio::playCue(const SoundCue& cue, const GameStatus& status) {
    switch (cue.kind) {
        case SoundCue::Kind::Footstep: {
            int variant;
            do {
                variant = pick(2);
            } while (variant == lastStep && pick(3) != 0);  // mostly alternating, not strictly
            lastStep = variant;
            const bool running = status.moveSpeed > RUN_SPEED_THRESHOLD;
            playHere(std::string("player/") + surfaceName(cue.surface) + "-step" + std::to_string(variant + 1) + ".wav",
                     (running ? RUN_STEP_VOLUME : STEP_VOLUME) * range(0.85f, 1.0f), range(0.93f, 1.07f));
            break;
        }
        case SoundCue::Kind::SwordSwing:
            playHere(pick(2) ? "weapon/sword1.wav" : "weapon/sword2.wav", 0.7f, range(0.95f, 1.08f));
            break;
        case SoundCue::Kind::SwordHit:
            playHere(pick(2) ? "weapon/sword1.wav" : "weapon/sword2.wav", 0.5f, range(0.95f, 1.05f));
            playHere(pick(2) ? "weapon/metal-hit1.wav" : "weapon/metal-hit2.wav", 0.85f, range(0.92f, 1.08f));
            break;
        case SoundCue::Kind::ArrowShot:
            playHere("weapon/arrow.wav", 0.75f, range(0.96f, 1.06f));
            break;
        case SoundCue::Kind::PlayerHurt:
            playHere("weapon/metal-hit2.wav", 0.55f, range(0.7f, 0.8f));
            break;
        case SoundCue::Kind::Defeat:
            playHere("sfx/string-and-stomp.wav", 0.8f);
            break;
        case SoundCue::Kind::LevelUp:
            playHere("sfx/string-and-stomp2.wav", 0.7f);
            break;
        case SoundCue::Kind::Pickup:
            playHere("player/pick-sword-inventory.wav", 0.6f, range(1.1f, 1.25f));
            break;
        case SoundCue::Kind::Equip:
            playHere("player/pick-sword-inventory.wav", 0.7f);
            break;
        case SoundCue::Kind::Mount:
            playAt("horse/horse-snort.wav", cue.position, 0.8f);
            break;
        case SoundCue::Kind::Dismount:
            playAt("horse/horse-snort2.wav", cue.position, 0.7f);
            break;
    }
}

void GameAudio::playCreature(const ActorSound& sound) {
    using Kind = ActorSound::Kind;
    const glm::vec3 at = sound.position;
    auto wolf = [&](float volume, float pitch) {
        static const char* const files[] = {"animals/attacking-wolf.wav", "animals/attacking-wolf2.wav", "animals/attacking-wolf3.wav"};
        playAt(files[pick(3)], at, volume, pitch, 5.0f, 80.0f);
    };

    switch (sound.role) {
        case GameRole::Wolf:
            if (sound.kind == Kind::Attack) wolf(0.9f, range(0.95f, 1.1f));
            if (sound.kind == Kind::Hurt) wolf(0.6f, range(1.25f, 1.4f));
            if (sound.kind == Kind::Death) wolf(0.7f, range(0.75f, 0.85f));
            break;
        case GameRole::Boar:
        case GameRole::Bear:
            if (sound.kind != Kind::Alarm) wolf(sound.kind == Kind::Attack ? 1.0f : 0.8f, range(0.5f, 0.62f));
            break;
        case GameRole::Dragon:
            if (sound.kind != Kind::Hurt) wolf(1.0f, range(0.38f, 0.46f));
            break;
        case GameRole::Deer:
            playAt(sound.kind == Kind::Alarm ? "animals/deer-snort.wav" : "animals/deer.wav", at, 0.7f, range(0.92f, 1.08f), 4.0f, 60.0f);
            break;
        case GameRole::Livestock:
            if (sound.kind != Kind::Attack) playAt("animals/sheep.wav", at, 0.7f, sound.kind == Kind::Death ? 0.8f : range(0.95f, 1.1f), 4.0f, 55.0f);
            break;
        case GameRole::Bird: {
            static const char* const files[] = {"animals/crow.wav", "animals/crow2.wav", "animals/crow3.wav", "animals/pigeons.wav"};
            if (sound.kind != Kind::Attack) playAt(files[pick(4)], at, 0.55f, range(0.95f, 1.1f), 4.0f, 70.0f);
            break;
        }
        case GameRole::Bandit:
        case GameRole::Villager:
            if (sound.kind == Kind::Attack) playAt(pick(2) ? "weapon/sword1.wav" : "weapon/sword2.wav", at, 0.7f, range(0.95f, 1.05f));
            if (sound.kind == Kind::Hurt) playAt(pick(2) ? "weapon/metal-hit1.wav" : "weapon/metal-hit2.wav", at, 0.6f, range(0.9f, 1.05f));
            if (sound.kind == Kind::Death) playAt("weapon/metal-hit2.wav", at, 0.8f, range(0.65f, 0.75f));
            break;
        default:
            break;
    }
}

void GameAudio::playRandomEvents(const SceneGraph& scene, const GameStatus& status, float deltaTime) {
    const glm::vec3 listener = status.listenerPosition;
    auto due = [&](float& timer, float low, float high) {
        timer -= deltaTime;
        if (timer > 0.0f) return false;
        timer = range(low, high);
        return true;
    };
    auto around = [&](float low, float high, float height) {
        const float angle = range(0.0f, 6.2831853f);
        const float distance = range(low, high);
        return listener + glm::vec3(std::cos(angle) * distance, height, std::sin(angle) * distance);
    };

    if (due(pigeonTimer, 10.0f, 26.0f) && nightness < 0.4f && townness > 0.5f) {
        playAt("animals/pigeons.wav", around(6.0f, 16.0f, 2.0f), 0.5f, range(0.95f, 1.05f), 4.0f, 50.0f);
    }

    // Whatever is grazing nearby.
    if (due(bleatTimer, 10.0f, 28.0f)) {
        glm::vec3 best(0.0f);
        float bestDistance = 40.0f;
        for (ObjectId id : sheep) {
            if (const Shape* animal = scene.findAs<Shape>(id)) {
                const float distance = glm::length(animal->getWorldPosition() - listener);
                if (distance < bestDistance && animal->isVisibleInHierarchy()) {
                    bestDistance = distance;
                    best = animal->getWorldPosition();
                }
            }
        }
        if (bestDistance < 40.0f) playAt("animals/sheep.wav", best, 0.6f, range(0.9f, 1.1f), 4.0f, 55.0f);
    }

    if (due(snortTimer, 12.0f, 32.0f)) {
        for (ObjectId id : horses) {
            const Shape* horse = scene.findAs<Shape>(id);
            if (!horse || !horse->isVisibleInHierarchy() || glm::length(horse->getWorldPosition() - listener) > 16.0f) continue;
            playAt(pick(2) ? "horse/horse-snort.wav" : "horse/horse-snort2.wav", horse->getWorldPosition(), 0.55f, range(0.95f, 1.05f));
            break;
        }
    }
}

void GameAudio::update(const SceneGraph& scene, const GameSession& game, float deltaTime) {
    if (!active) return;
    const GameStatus& status = game.getStatus();

    // Sources placed in the scene sound even in a scene with nobody to hear it from.
    if (status.hasPlayer) engine.setListener(status.listenerPosition, status.listenerForward);
    updateSources(scene, deltaTime);
    if (!status.hasPlayer) return;

    nightness = nightAt(status.hour);
    scanTimer -= deltaTime;
    if (scanTimer <= 0.0f) {
        scanTimer = SCAN_SECONDS;
        rescan(scene, game, status.listenerPosition);
    }

    // The sounds that belong to a place get quieter to the right degree with distance alone.
    for (Emitter* emitter : {&river, &lake, &fire, &townNoise, &crowd, &music}) {
        driveEmitter(*emitter, emitter->dayVolume + (emitter->nightVolume - emitter->dayVolume) * nightness);
    }

    // On horseback, the hooves.
    const bool galloping = status.mounted && status.moveSpeed > 1.0f;
    drive(hooves, galloping ? 0.25f + 0.55f * std::min(1.0f, status.moveSpeed / HOOF_FULL_SPEED) : 0.0f, deltaTime, 0.35f);

    for (const SoundCue& cue : game.cues) playCue(cue, status);
    for (const ActorSound& sound : game.creatureSounds) playCreature(sound);
    playRandomEvents(scene, status, deltaTime);

    // The bell of the chapel, three times a day, heard a long way off.
    const int hour = int(status.hour);
    if (hour != lastBellHour) {
        if (lastBellHour >= 0 && !chapels.empty() && std::find(std::begin(BELL_HOURS), std::end(BELL_HOURS), hour) != std::end(BELL_HOURS)) {
            glm::vec3 chapel(0.0f);
            nearestOf(chapels, status.listenerPosition, &chapel);
            playAt("town/church-bell.wav", chapel + glm::vec3(0.0f, BELFRY_HEIGHT, 0.0f), 0.9f, 1.0f, 35.0f, 280.0f);
        }
        lastBellHour = hour;
    }
}

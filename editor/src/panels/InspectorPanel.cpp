#include "InspectorPanel.h"
#include "Player.h"
#include "UiObject.h"
#include "AssetImporter.h"
#include "ObjectFactory.h"
#include "Theme.h"
#include "Widgets.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    bool beginProperties(const char* id) {
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    std::string formatSize(uintmax_t bytes) {
        char buffer[32];
        if (bytes < 1024) {
            std::snprintf(buffer, sizeof(buffer), "%d bytes", (int)bytes);
        } else if (bytes < 1024 * 1024) {
            std::snprintf(buffer, sizeof(buffer), "%.1f KB", bytes / 1024.0);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.1f MB", bytes / (1024.0 * 1024.0));
        }
        return buffer;
    }
}

void InspectorPanel::draw(EditorContext& ctx, AssetThumbnails& thumbnails, bool* open) {
    if (!ImGui::Begin("Inspector", open)) {
        ImGui::End();
        return;
    }

    SceneObject* selected = ctx.getSelected();
    if (previewSound != NoSound && (!selected || selected->id != previewOwner)) {
        preview.stopSound(previewSound);
        previewSound = NoSound;
    }
    if (SceneObject* object = selected) {
        drawObject(ctx, *object);
    } else if (!ctx.selectedAsset.empty()) {
        drawAsset(ctx, thumbnails);
    } else {
        Widgets::wrappedDim("Select an object in the Hierarchy or the Scene view to edit it here.");
    }

    ImGui::End();
}

void InspectorPanel::drawObject(EditorContext& ctx, SceneObject& object) {
    ImGui::PushID((int)object.id);

    const ImVec2 iconPos = ImGui::GetCursorScreenPos();
    Icons::object(ImGui::GetWindowDrawList(), ImVec2(iconPos.x, iconPos.y + 3.0f), 18.0f, object.getType(), Theme::Accent);
    ImGui::SetCursorScreenPos(ImVec2(iconPos.x + 26.0f, iconPos.y));

    ImGui::SetNextItemWidth(-FLT_MIN);
    std::string name = object.name;
    Widgets::inputText("##name", name);
    if (ImGui::IsItemDeactivatedAfterEdit() && !name.empty()) {
        object.name = name;
        ctx.document.markDirty();
    } else if (ImGui::IsItemActive() && !name.empty()) {
        object.name = name;  // live, so the Hierarchy follows the typing
    }

    if (ImGui::Checkbox("Visible", &object.visible)) {
        ctx.document.markDirty();
    }
    ImGui::SameLine();
    Widgets::dimText(ObjectFactory::typeName(object.getType()));

    drawTransform(ctx, object);

    if (auto* player = dynamic_cast<Player*>(&object)) {
        drawPlayer(ctx, *player);
    }
    if (auto* shape = dynamic_cast<Shape*>(&object)) {
        drawShape(ctx, *shape);
    } else if (auto* light = dynamic_cast<LightObject*>(&object)) {
        drawLight(ctx, *light);
    } else if (auto* camera = dynamic_cast<CameraObject*>(&object)) {
        drawCamera(ctx, *camera);
    } else if (auto* emitter = dynamic_cast<ParticleEmitter*>(&object)) {
        drawParticles(ctx, *emitter);
    } else if (auto* fire = dynamic_cast<FireObject*>(&object)) {
        drawFire(ctx, *fire);
    } else if (auto* water = dynamic_cast<WaterObject*>(&object)) {
        drawWater(ctx, *water);
    } else if (auto* source = dynamic_cast<AudioSourceObject*>(&object)) {
        drawAudioSource(ctx, *source);
    } else if (auto* ui = dynamic_cast<UiObject*>(&object)) {
        drawUi(ctx, *ui);
    }

    drawScript(ctx, object);

    ImGui::PopID();
}

void InspectorPanel::drawScript(EditorContext& ctx, SceneObject& object) {
    if (!Widgets::section("Script")) return;
    if (!beginProperties("##script")) return;

    const std::vector<ScriptInfo>& scripts = ctx.availableScripts();
    bool known = object.script.empty();
    for (const ScriptInfo& info : scripts) known |= info.name == object.script;

    Widgets::propertyLabel("Script", "A C++ class from Assets that runs on this object while the scene plays. Create one from a template in the Project panel (right-click, New script).");
    ImGui::SetNextItemWidth(-FLT_MIN);
    const std::string shown = object.script.empty() ? "None" : object.script + (known ? "" : " (missing)");
    if (ImGui::BeginCombo("##scriptChoice", shown.c_str())) {
        if (ImGui::Selectable("None", object.script.empty())) {
            object.script.clear();
            ctx.document.markDirty();
        }
        for (const ScriptInfo& info : scripts) {
            if (ImGui::Selectable(info.name.c_str(), info.name == object.script)) {
                object.script = info.name;
                ctx.document.markDirty();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndTable();

    if (scripts.empty()) {
        Widgets::dimText("No scripts in this project yet.");
    } else if (!object.script.empty() && known) {
        for (const ScriptInfo& info : scripts) {
            if (info.name != object.script) continue;
            Widgets::dimText(ctx.assets.toAssetPath(info.file).c_str());
            break;
        }
    }
}

void InspectorPanel::drawTransform(EditorContext& ctx, SceneObject& object) {
    if (!Widgets::section("Transform")) return;

    if (beginProperties("##transform")) {
        glm::vec3 position = object.position;
        if (Widgets::vec3Row("Position", position, 0.05f, 0.0f)) {
            object.setPosition(position);
            ctx.document.markDirty();
        }

        glm::vec3 rotation = object.rotation;
        if (Widgets::vec3Row("Rotation", rotation, 0.5f, 0.0f)) {
            object.setRotation(rotation);
            ctx.document.markDirty();
        }

        glm::vec3 scale = object.scale;
        if (Widgets::vec3Row("Scale", scale, 0.02f, 1.0f)) {
            object.setScale(scale);
            ctx.document.markDirty();
        }
        ImGui::EndTable();
    }
}

void InspectorPanel::drawShape(EditorContext& ctx, Shape& shape) {
    if (Widgets::section("Rendering")) {
        if (beginProperties("##rendering")) {
            Widgets::propertyLabel("Color");
            glm::vec3 color = shape.getColor();
            if (ImGui::ColorEdit3("##color", &color.x, ImGuiColorEditFlags_Float)) {
                shape.setColor(color);
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Texture", "The surface image. Drag a texture here from the Project panel.");
            const std::shared_ptr<Texture> texture = shape.getTexture("texture_albedo");
            std::string assetPath = texture ? texture->assetPath : std::string();
            if (Widgets::assetSlot("texture", assetPath, AssetKind::Texture)) {
                ctx.setObjectTexture(shape.id, assetPath);
            }

            if (texture) {
                Widgets::propertyLabel("Tint texture", "Multiplies the texture by the color above.");
                if (ImGui::Checkbox("##tint", &shape.tintTexture)) {
                    ctx.document.markDirty();
                }
            }

            Widgets::propertyLabel("Cast shadows");
            if (ImGui::Checkbox("##castShadow", &shape.castShadow)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Emissive", "Drawn at full color, unaffected by light.");
            if (ImGui::Checkbox("##emissive", &shape.emissive)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Wave height", "For water: how far its surface swells. The mesh needs enough vertices.");
            if (ImGui::DragFloat("##waveHeight", &shape.waveHeight, 0.01f, 0.0f, 3.0f, "%.2f")) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Sway", "Bends in the wind, more the higher above its origin, like a tree.");
            if (ImGui::DragFloat("##sway", &shape.sway, 0.01f, 0.0f, 3.0f, "%.2f")) {
                ctx.document.markDirty();
            }

            ImGui::EndTable();
        }
    }

    if (Widgets::section("Physics")) {
        if (beginProperties("##physics")) {
            Widgets::propertyLabel("Static", "Static objects never move during play.");
            if (ImGui::Checkbox("##static", &shape.isStatic)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Gravity", "Falls during play. Has no effect on static objects.");
            if (ImGui::Checkbox("##gravity", &shape.useGravity)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Collision");
            if (ImGui::Checkbox("##collision", &shape.hasCollision)) {
                ctx.document.markDirty();
            }
            ImGui::EndTable();
        }
    }

}

void InspectorPanel::drawLight(EditorContext& ctx, LightObject& light) {
    if (Widgets::section("Light")) {
        if (beginProperties("##light")) {
            Widgets::propertyLabel("Color");
            if (ImGui::ColorEdit3("##lightColor", &light.color.x, ImGuiColorEditFlags_Float)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Directional", "Sunlight: the same direction everywhere, along the light's forward axis.");
            if (ImGui::Checkbox("##directional", &light.directional)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Intensity");
            if (ImGui::DragFloat("##intensity", &light.intensity, 0.01f, 0.0f, 20.0f, "%.2f")) {
                ctx.document.markDirty();
            }
            ImGui::EndTable();
        }

        if (ctx.scene().getLight() != &light) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::Warning);
            ImGui::TextWrapped("The renderer uses one light, and this is not it. The first light in the scene is used.");
            ImGui::PopStyleColor();
        }
    }
}

void InspectorPanel::drawCamera(EditorContext& ctx, CameraObject& camera) {
    if (Widgets::section("Camera")) {
        if (beginProperties("##camera")) {
            Widgets::propertyLabel("First person", "In a game, look through the player's eyes instead of following.");
            if (ImGui::Checkbox("##firstPerson", &camera.firstPerson)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Orthographic");
            if (ImGui::Checkbox("##orthographic", &camera.orthographic)) {
                ctx.document.markDirty();
            }

            if (camera.orthographic) {
                Widgets::propertyLabel("Size", "Half of the visible height, in scene units.");
                if (ImGui::DragFloat("##size", &camera.orthographicSize, 0.05f, 0.01f, 1000.0f, "%.2f")) {
                    ctx.document.markDirty();
                }
            } else {
                Widgets::propertyLabel("Field of view");
                if (ImGui::DragFloat("##fov", &camera.fieldOfView, 0.25f, 1.0f, 170.0f, "%.1f")) {
                    ctx.document.markDirty();
                }
            }

            Widgets::propertyLabel("Near plane");
            if (ImGui::DragFloat("##near", &camera.nearPlane, 0.01f, 0.001f, camera.farPlane, "%.3f")) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Far plane");
            if (ImGui::DragFloat("##far", &camera.farPlane, 1.0f, camera.nearPlane, 100000.0f, "%.1f")) {
                ctx.document.markDirty();
            }
            ImGui::EndTable();
        }

        if (ctx.scene().getMainCamera() != &camera) {
            ImGui::Spacing();
            Widgets::wrappedDim("The Game view looks through the first camera in the scene, which is not this one.");
        }
    }
}

namespace {
    // One labelled row of a property table; true when the value changed.
    bool slider(const char* label, const char* id, float& value, float low, float high, const char* format = "%.2f") {
        Widgets::propertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        return ImGui::SliderFloat(id, &value, low, high, format);
    }

    bool color3(const char* label, const char* id, glm::vec3& value) {
        Widgets::propertyLabel(label);
        return ImGui::ColorEdit3(id, &value.x, ImGuiColorEditFlags_Float);
    }

    bool color4(const char* label, const char* id, glm::vec4& value) {
        Widgets::propertyLabel(label);
        return ImGui::ColorEdit4(id, &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
    }

    bool toggle(const char* label, const char* id, bool& value, const char* tip = nullptr) {
        Widgets::propertyLabel(label, tip);
        return ImGui::Checkbox(id, &value);
    }
}

void InspectorPanel::drawParticles(EditorContext& ctx, ParticleEmitter& emitter) {
    ParticleSettings& p = emitter.system.settings;
    bool changed = false;
    if (Widgets::section("Particles") && beginProperties("##particles")) {
        changed |= toggle("Playing", "##playing", emitter.system.playing);
        changed |= slider("Rate", "##rate", p.rate, 0.0f, 500.0f, "%.1f / s");
        Widgets::propertyLabel("Max particles");
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragInt("##max", &p.maxParticles, 1.0f, 1, 5000);
        changed |= slider("Lifetime min", "##lifeMin", p.lifetimeMin, 0.05f, 20.0f);
        changed |= slider("Lifetime max", "##lifeMax", p.lifetimeMax, 0.05f, 20.0f);
        changed |= slider("Speed min", "##speedMin", p.speedMin, -20.0f, 40.0f);
        changed |= slider("Speed max", "##speedMax", p.speedMax, -20.0f, 40.0f);
        changed |= slider("Spread", "##cone", p.coneDegrees, 0.0f, 180.0f, "%.0f deg");
        changed |= slider("Radius", "##radius", p.radius, 0.0f, 10.0f);
        Widgets::propertyLabel("Gravity", "Acceleration in the world. Upwards makes smoke rise.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat3("##gravity", &p.gravity.x, 0.05f, -50.0f, 50.0f, "%.2f");
        changed |= slider("Drag", "##drag", p.drag, 0.0f, 5.0f);
        changed |= slider("Turbulence", "##turbulence", p.turbulence, 0.0f, 10.0f);
        changed |= slider("Size start", "##sizeStart", p.sizeStart, 0.0f, 10.0f);
        changed |= slider("Size end", "##sizeEnd", p.sizeEnd, 0.0f, 10.0f);
        changed |= slider("Spin", "##spin", p.spin, 0.0f, 10.0f);
        changed |= color4("Color start", "##colorStart", p.colorStart);
        changed |= color4("Color middle", "##colorMid", p.colorMid);
        changed |= color4("Color end", "##colorEnd", p.colorEnd);
        changed |= toggle("Additive", "##additive", p.additive, "Glows and adds up, as fire and sparks do, instead of covering what is behind.");
        changed |= slider("Softness", "##softness", p.softness, 0.0f, 1.0f);
        changed |= slider("Flicker", "##flicker", p.flicker, 0.0f, 1.0f);
        ImGui::EndTable();
    }
    if (changed) ctx.document.markDirty();
}

void InspectorPanel::drawFire(EditorContext& ctx, FireObject& fire) {
    bool changed = false;
    if (Widgets::section("Fire") && beginProperties("##fire")) {
        changed |= toggle("Burning", "##burning", fire.burning);
        changed |= slider("Size", "##size", fire.size, 0.1f, 10.0f);
        changed |= slider("Intensity", "##intensity", fire.intensity, 0.0f, 4.0f);
        changed |= slider("Smoke", "##smoke", fire.smoke, 0.0f, 3.0f);
        changed |= slider("Sparks", "##sparks", fire.sparks, 0.0f, 3.0f);
        changed |= slider("Damage / s", "##damage", fire.damage, 0.0f, 100.0f, "%.1f");
        changed |= slider("Light range", "##lightRange", fire.lightRange, 0.0f, 60.0f, "%.1f");
        changed |= color3("Light color", "##lightColor", fire.lightColor);
        changed |= slider("Flicker", "##flicker", fire.flicker, 0.0f, 1.0f);
        ImGui::EndTable();
    }
    if (changed) ctx.document.markDirty();
}

void InspectorPanel::drawWater(EditorContext& ctx, WaterObject& water) {
    bool changed = false;
    if (Widgets::section("Water") && beginProperties("##water")) {
        Widgets::propertyLabel("Size", "Along x and z, before the object's scale. The height of the object is the surface.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat2("##waterSize", &water.size.x, 0.5f, 0.1f, 5000.0f, "%.1f");
        changed |= toggle("Round", "##round", water.round, "An ellipse inside that rectangle, for a pond.");
        changed |= slider("Wave height", "##waveHeight", water.waveHeight, 0.0f, 2.0f);
        changed |= slider("Wave length", "##waveLength", water.waveLength, 0.5f, 40.0f);
        changed |= slider("Wave speed", "##waveSpeed", water.waveSpeed, 0.0f, 5.0f);
        Widgets::propertyLabel("Current", "Which way the ripples drift, in units per second.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat2("##flow", &water.flow.x, 0.05f, -20.0f, 20.0f, "%.2f");
        changed |= color3("Shallow color", "##shallow", water.shallowColor);
        changed |= color3("Deep color", "##deep", water.deepColor);
        changed |= slider("Opacity", "##opacity", water.opacity, 0.0f, 1.0f);
        ImGui::EndTable();
    }
    if (changed) ctx.document.markDirty();
}

void InspectorPanel::drawUi(EditorContext& ctx, UiObject& ui) {
    static const char* const KINDS[] = {"Panel", "Text", "Image", "Bar", "Button"};
    static const char* const ANCHORS[] = {"Top left", "Top", "Top right", "Left", "Center", "Right", "Bottom left", "Bottom", "Bottom right"};
    static const char* const ALIGNS[] = {"Left", "Center", "Right"};
    static const char* const BINDINGS[] = {"None", "Health", "Stamina"};

    bool changed = false;
    if (Widgets::section("2D Element") && beginProperties("##ui")) {
        auto choice = [&](const char* label, const char* id, int& value, const char* const* names, int count, const char* tip = nullptr) {
            Widgets::propertyLabel(label, tip);
            ImGui::SetNextItemWidth(-FLT_MIN);
            bool picked = false;
            if (ImGui::BeginCombo(id, names[value])) {
                for (int i = 0; i < count; i++) {
                    if (ImGui::Selectable(names[i], i == value)) {
                        value = i;
                        picked = true;
                    }
                }
                ImGui::EndCombo();
            }
            return picked;
        };
        auto colorRow = [&](const char* label, const char* id, glm::vec4& value) {
            Widgets::propertyLabel(label);
            ImGui::SetNextItemWidth(-FLT_MIN);
            return ImGui::ColorEdit4(id, &value.x, ImGuiColorEditFlags_AlphaBar);
        };
        auto vec2Row = [&](const char* label, const char* id, glm::vec2& value, float speed, const char* tip) {
            Widgets::propertyLabel(label, tip);
            ImGui::SetNextItemWidth(-FLT_MIN);
            return ImGui::DragFloat2(id, &value.x, speed, -4000.0f, 4000.0f, "%.0f");
        };

        int kind = (int)ui.kind;
        if (choice("Type", "##kind", kind, KINDS, 5)) ui.kind = (UiKind)kind, changed = true;

        changed |= toggle("Fill parent", "##stretch", ui.stretch, "Covers the whole parent element, or the screen. Offset becomes a margin.");
        if (!ui.stretch) {
            int anchor = (int)ui.anchor;
            if (choice("Anchor", "##anchor", anchor, ANCHORS, 9, "Which point of the parent this element is fixed to.")) ui.anchor = (UiAnchor)anchor, changed = true;
            changed |= vec2Row("Size", "##size", ui.size, 1.0f, "Pixels, at a 720 line screen. It scales with the window.");
            ui.size = glm::max(ui.size, glm::vec2(0.0f));
        }
        changed |= vec2Row("Offset", "##offset", ui.offset, 1.0f, "Pixels from the anchor.");

        if (ui.kind == UiKind::Text || ui.kind == UiKind::Button) {
            Widgets::propertyLabel("Text");
            std::string value = ui.text;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (Widgets::inputText("##text", value)) {
                ui.text = value;
                changed = true;
            }
            changed |= slider("Font size", "##fontSize", ui.fontSize, 8.0f, 120.0f, "%.0f");
        }
        if (ui.kind == UiKind::Text) {
            int align = ui.align;
            if (choice("Align", "##align", align, ALIGNS, 3)) ui.align = align, changed = true;
        }
        if (ui.kind == UiKind::Image) {
            Widgets::propertyLabel("Image", "A texture from the project.");
            changed |= Widgets::assetSlot("image", ui.image, AssetKind::Texture);
        }
        if (ui.kind == UiKind::Bar) {
            changed |= slider("Fill", "##fill", ui.fill, 0.0f, 1.0f);
            int binding = (int)ui.binding;
            if (choice("Follows", "##binding", binding, BINDINGS, 3, "What fills the bar by itself while the game runs.")) ui.binding = (UiBinding)binding, changed = true;
            changed |= colorRow("Background", "##background", ui.background);
        }
        changed |= colorRow(ui.kind == UiKind::Image ? "Tint" : "Color", "##color", ui.color);
        if (ui.kind != UiKind::Text) changed |= slider("Rounding", "##rounding", ui.rounding, 0.0f, 40.0f, "%.0f");
        ImGui::EndTable();
    }
    if (changed) ctx.document.markDirty();
}

void InspectorPanel::drawPlayer(EditorContext& ctx, Player& player) {
    bool changed = false;
    if (Widgets::section("Player") && beginProperties("##player")) {
        changed |= toggle("Game controls", "##gameControls", player.gameControls,
                          "The engine's own game: walking, camera, combat, health and the HUD. Off, the player does nothing until a script moves it.");
        ImGui::EndTable();
    }
    if (changed) ctx.document.markDirty();
}

void InspectorPanel::drawAudioSource(EditorContext& ctx, AudioSourceObject& source) {
    bool changed = false;
    if (Widgets::section("Audio Source") && beginProperties("##audio")) {
        Widgets::propertyLabel("Clip", "A WAV file. Drag one here from the Project panel.");
        changed |= Widgets::assetSlot("clip", source.clip, AssetKind::Audio);
        changed |= toggle("Muted", "##muted", source.muted, "Silences it without removing it.");
        changed |= slider("Volume", "##volume", source.volume, 0.0f, 2.0f);
        changed |= slider("Pitch", "##pitch", source.pitch, 0.25f, 4.0f);
        changed |= slider("Pitch variation", "##pitchVariation", source.pitchVariation, 0.0f, 0.5f);
        changed |= toggle("Loop", "##loop", source.loop, "Plays again and again until the scene ends.");
        changed |= toggle("3D sound", "##spatial", source.spatial, "Heard from where this object stands, louder close and softer far away. Off, it is heard the same everywhere.");
        if (source.spatial) {
            changed |= slider("Near distance", "##near", source.nearDistance, 0.1f, 100.0f, "%.1f");
            changed |= slider("Far distance", "##far", source.farDistance, 0.5f, 500.0f, "%.1f");
            source.nearDistance = std::max(0.1f, source.nearDistance);
            source.farDistance = std::max(source.nearDistance + 0.1f, source.farDistance);
        }
        changed |= slider("Fade in", "##fadeIn", source.fadeIn, 0.0f, 10.0f, "%.1f s");
        Widgets::propertyLabel("Start delay", "Seconds after the scene starts before it first plays.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat("##startDelay", &source.startDelay, 0.1f, 0.0f, 3600.0f, "%.1f s");
        if (!source.loop) {
            Widgets::propertyLabel("Repeat", "Seconds between plays, at random between the two. 0 and 0 plays it once.");
            ImGui::SetNextItemWidth(-FLT_MIN);
            float range[2] = {source.repeatMin, source.repeatMax};
            if (ImGui::DragFloat2("##repeat", range, 0.1f, 0.0f, 3600.0f, "%.1f s")) {
                source.repeatMin = std::max(0.0f, range[0]);
                source.repeatMax = std::max(source.repeatMin, range[1]);
                changed = true;
            }
        }
        ImGui::EndTable();
    }

    // Listen to it here, with the pitch and volume it has.
    if (previewSound != NoSound && (!preview.isPlaying(previewSound) || previewOwner != source.id)) {
        preview.stopSound(previewSound);
        previewSound = NoSound;
    }
    const bool playing = previewSound != NoSound;
    ImGui::BeginDisabled(source.clip.empty() && !playing);
    if (ImGui::Button(playing ? "Stop" : "Preview", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        if (playing) {
            preview.stopSound(previewSound);
            previewSound = NoSound;
        } else {
            if (!preview.isOutputOpen()) preview.start();
            SoundOptions options;
            options.volume = source.volume;
            options.pitch = source.pitch;
            options.loop = source.loop;
            options.fadeIn = source.fadeIn;
            previewSound = preview.play(ctx.assets.resolve(source.clip).string(), options);
            previewOwner = source.id;
            if (previewSound == NoSound) ctx.setStatus("Could not play " + source.clip + ": it must be a WAV file");
        }
    }
    ImGui::EndDisabled();
    if (playing && previewOwner == source.id) {
        preview.setVolume(previewSound, source.volume);
    }
    if (changed) ctx.document.markDirty();
}

void InspectorPanel::drawAsset(EditorContext& ctx, AssetThumbnails& thumbnails) {
    const fs::path file = ctx.assets.resolve(ctx.selectedAsset);

    std::error_code ec;
    if (file.empty() || !fs::exists(file, ec)) {
        ctx.selectedAsset.clear();
        return;
    }

    ImGui::PushFont(nullptr, 16.0f);
    ImGui::TextWrapped("%s", file.filename().u8string().c_str());
    ImGui::PopFont();

    const AssetKind kind = classifyAsset(file);

    if (kind == AssetKind::Texture) {
        if (const unsigned int preview = thumbnails.get(ctx.selectedAsset)) {
            ImGui::Spacing();
            const float side = std::min(ImGui::GetContentRegionAvail().x, 220.0f);
            ImGui::Image((ImTextureID)(intptr_t)preview, ImVec2(side, side));
        }
    }

    if (Widgets::section("File")) {
        if (beginProperties("##file")) {
            Widgets::propertyLabel("Path");
            ImGui::TextWrapped("%s", ctx.selectedAsset.c_str());

            if (kind != AssetKind::Folder) {
                Widgets::propertyLabel("Size");
                ImGui::TextUnformatted(formatSize(fs::file_size(file, ec)).c_str());
            }
            ImGui::EndTable();
        }
    }
}

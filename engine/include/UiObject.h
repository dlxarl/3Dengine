#pragma once
#include "SceneObject.h"

#include <glm/glm.hpp>
#include <string>

enum class UiKind { Panel, Text, Image, Bar, Button };

// Where an element sits in its parent, and which of its own points sits there.
enum class UiAnchor { TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight };

// What fills a Bar by itself while the game runs. Anything else is set by a script.
enum class UiBinding { None, Health, Stamina };

// A flat element of the game's interface: a panel, a line of text, an image, a bar or a
// button. Elements are laid out in screen pixels inside their parent element (or the screen),
// and are drawn over the Game view in hierarchy order, children on top.
class UiObject : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::Ui; }

    UiKind kind = UiKind::Panel;
    UiAnchor anchor = UiAnchor::TopLeft;
    glm::vec2 offset = glm::vec2(0.0f);           // pixels from the anchor, at a 720 line screen
    glm::vec2 size = glm::vec2(200.0f, 60.0f);
    bool stretch = false;                         // fills its parent; offset becomes a margin

    glm::vec4 color = glm::vec4(1.0f);            // text color, image tint, or the fill of a panel, bar or button
    glm::vec4 background = glm::vec4(0.0f, 0.0f, 0.0f, 0.55f);   // behind a bar; the hover color of a button is derived from color
    float rounding = 0.0f;

    std::string font;                             // asset path of a .ttf or .otf; empty is Inter
    std::string text;
    float fontSize = 24.0f;
    int align = 0;                                // 0 left, 1 center, 2 right

    std::string image;                            // asset path of a texture
    float fill = 1.0f;                            // a bar's level, 0 to 1
    UiBinding binding = UiBinding::None;

    // Set while the game runs, so that scripts can react to the mouse.
    bool hovered = false;
    bool pressed = false;
};

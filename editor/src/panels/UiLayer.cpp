#include "UiLayer.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace {
    ImU32 pack(const glm::vec4& c, float brighten = 1.0f) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(std::min(c.r * brighten, 1.0f), std::min(c.g * brighten, 1.0f),
                                                     std::min(c.b * brighten, 1.0f), c.a));
    }

    // The anchor as a fraction of the parent: 0 is left or top, 1 right or bottom.
    ImVec2 anchorPoint(UiAnchor anchor) {
        const int column = (int)anchor % 3;
        const int row = (int)anchor / 3;
        return ImVec2(column * 0.5f, row * 0.5f);
    }
}

UiLayer::Rect UiLayer::layout(const UiObject& ui, const Rect& parent) const {
    const ImVec2 parentSize(parent.max.x - parent.min.x, parent.max.y - parent.min.y);
    const ImVec2 offset(ui.offset.x * scale, ui.offset.y * scale);

    if (ui.stretch) {
        return {ImVec2(parent.min.x + offset.x, parent.min.y + offset.y),
                ImVec2(parent.max.x - offset.x, parent.max.y - offset.y)};
    }

    const ImVec2 size(ui.size.x * scale, ui.size.y * scale);
    const ImVec2 at = anchorPoint(ui.anchor);
    const ImVec2 min(parent.min.x + parentSize.x * at.x - size.x * at.x + offset.x,
                     parent.min.y + parentSize.y * at.y - size.y * at.y + offset.y);
    return {min, ImVec2(min.x + size.x, min.y + size.y)};
}

void UiLayer::draw(EditorContext& ctx, ImVec2 imageMin, ImVec2 imageSize) {
    bool any = false;
    for (const auto& object : ctx.scene().getObjects()) {
        if (object->getType() == ObjectType::Ui) {
            any = true;
            break;
        }
    }
    if (!any) return;

    context = &ctx;
    list = ImGui::GetWindowDrawList();
    scale = std::max(0.1f, imageSize.y / 720.0f);

    const Rect screen{imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y)};
    list->PushClipRect(screen.min, screen.max, true);

    const bool interactive = !ctx.play.isStopped() && !ctx.wantsMouseCapture;
    for (SceneObject* root : ctx.scene().getRoots()) visit(*root, screen, interactive);

    list->PopClipRect();
}

void UiLayer::visit(SceneObject& object, const Rect& parent, bool interactive) {
    if (!object.visible) return;

    Rect rect = parent;
    if (auto* ui = dynamic_cast<UiObject*>(&object)) {
        rect = layout(*ui, parent);
        drawElement(*ui, rect, interactive);
    }
    for (SceneObject* child : object.getChildren()) visit(*child, rect, interactive);
}

void UiLayer::drawElement(UiObject& ui, const Rect& rect, bool interactive) {
    const ImVec2 size(rect.max.x - rect.min.x, rect.max.y - rect.min.y);
    const float rounding = ui.rounding * scale;
    font = context->fontFor ? context->fontFor(ui.font) : nullptr;
    if (!font) font = context->gameFont ? context->gameFont : ImGui::GetFont();

    ui.hovered = false;
    ui.pressed = false;
    if (interactive && ui.kind == UiKind::Button) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        ui.hovered = mouse.x >= rect.min.x && mouse.x < rect.max.x && mouse.y >= rect.min.y && mouse.y < rect.max.y;
        ui.pressed = ui.hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    }

    switch (ui.kind) {
        case UiKind::Panel:
            if (ui.color.a > 0.0f) list->AddRectFilled(rect.min, rect.max, pack(ui.color), rounding);
            break;

        case UiKind::Button: {
            const float shade = ui.pressed ? 0.8f : (ui.hovered ? 1.25f : 1.0f);
            list->AddRectFilled(rect.min, rect.max, pack(ui.color, shade), rounding);
            list->AddRect(rect.min, rect.max, pack(glm::vec4(1.0f, 1.0f, 1.0f, 0.25f)), rounding);
            if (!ui.text.empty()) {
                const float px = ui.fontSize * scale;
                const ImVec2 extent = font->CalcTextSizeA(px, FLT_MAX, 0.0f, ui.text.c_str());
                list->AddText(font, px, ImVec2(rect.min.x + (size.x - extent.x) * 0.5f, rect.min.y + (size.y - extent.y) * 0.5f),
                              IM_COL32_WHITE, ui.text.c_str());
            }
            break;
        }

        case UiKind::Text: {
            if (ui.text.empty()) break;
            const float px = ui.fontSize * scale;
            const ImVec2 extent = font->CalcTextSizeA(px, FLT_MAX, 0.0f, ui.text.c_str());
            const float x = ui.align == 1 ? rect.min.x + (size.x - extent.x) * 0.5f
                            : ui.align == 2 ? rect.max.x - extent.x : rect.min.x;
            list->AddText(font, px, ImVec2(x, rect.min.y + (size.y - extent.y) * 0.5f), pack(ui.color), ui.text.c_str());
            break;
        }

        case UiKind::Image: {
            std::shared_ptr<Texture> texture = ui.image.empty() ? nullptr : context->assets.getTexture(ui.image);
            if (texture && texture->loaded) {
                list->AddImageRounded((ImTextureID)(intptr_t)texture->ID, rect.min, rect.max, ImVec2(0, 0), ImVec2(1, 1),
                                      pack(ui.color), rounding);
            } else {
                list->AddRectFilled(rect.min, rect.max, Theme::Sunken, rounding);
                list->AddRect(rect.min, rect.max, Theme::TextDim, rounding);
            }
            break;
        }

        case UiKind::Bar: {
            float level = ui.fill;
            const GameStatus& status = context->play.getGameStatus();
            if (!context->play.isStopped() && status.hasPlayer) {
                if (ui.binding == UiBinding::Health && status.maxHealth > 0.0f) level = status.health / status.maxHealth;
                if (ui.binding == UiBinding::Stamina) level = status.stamina / 100.0f;
            }
            level = std::clamp(level, 0.0f, 1.0f);
            list->AddRectFilled(rect.min, rect.max, pack(ui.background), rounding);
            if (level > 0.0f) {
                list->AddRectFilled(rect.min, ImVec2(rect.min.x + size.x * level, rect.max.y), pack(ui.color), rounding);
            }
            break;
        }
    }

    if (ui.id == context->selectedObject && context->play.isStopped()) {
        list->AddRect(rect.min, rect.max, Theme::Accent, 0.0f, 0, 2.0f);
    }
}

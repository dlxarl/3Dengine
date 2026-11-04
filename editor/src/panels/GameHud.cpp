#include "GameHud.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    const ImU32 SHADOW = Theme::rgb(0x000000, 200);
    const ImU32 PANEL = Theme::rgb(0x14161A, 215);
    const ImU32 HEALTH = Theme::rgb(0xC9524A);
    const ImU32 STAMINA = Theme::rgb(0xB9B85A);
    const ImU32 BREATH = Theme::rgb(0x5AA6C9);
    const ImU32 EXPERIENCE = Theme::rgb(0x6FBFAE);
    const float MARGIN = 18.0f;
}

void GameHud::text(float fontSize, ImVec2 position, ImU32 color, const char* value) const {
    list->AddText(font, fontSize, ImVec2(position.x + 2.0f, position.y + 2.0f), SHADOW, value);
    list->AddText(font, fontSize, position, color, value);
}

void GameHud::centered(float fontSize, float y, ImU32 color, const char* value) const {
    const float width = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, value).x;
    text(fontSize, ImVec2(min.x + (size.x - width) * 0.5f, y), color, value);
}

float GameHud::wrapped(float fontSize, ImVec2 position, float width, ImU32 color, const char* value) const {
    list->AddText(font, fontSize, position, color, value, nullptr, width);
    return font->CalcTextSizeA(fontSize, FLT_MAX, width, value).y;
}

void GameHud::bar(ImVec2 position, float width, float fraction, ImU32 color) const {
    list->AddRectFilled(position, ImVec2(position.x + width, position.y + 9.0f), Theme::rgb(0x000000, 170));
    list->AddRectFilled(position, ImVec2(position.x + width * std::clamp(fraction, 0.0f, 1.0f), position.y + 9.0f), color);
}

void GameHud::panel(ImVec2 from, ImVec2 to) const {
    list->AddRectFilled(from, to, PANEL, 3.0f);
    list->AddRect(from, to, Theme::rgb(0x6FBFAE, 90), 3.0f);
}

void GameHud::draw(EditorContext& ctx, ImVec2 imageMin, ImVec2 imageSize) {
    const GameStatus& status = ctx.play.getGameStatus();
    if (ctx.play.isStopped() || !status.hasPlayer) return;

    const GameProgress& progress = ctx.play.getGame().getProgress();

    list = ImGui::GetWindowDrawList();
    font = ctx.gameFont ? ctx.gameFont : ImGui::GetFont();
    min = imageMin;
    size = imageSize;
    max = ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
    list->PushClipRect(min, max, true);

    if (status.hasWorld && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) showInventory = !showInventory;
        if (ImGui::IsKeyPressed(ImGuiKey_J, false)) showJournal = !showJournal;
    }

    // Sleeping fades through black.
    if (status.rested > 0.0f) {
        list->AddRectFilled(min, max, Theme::rgb(0x000000, (unsigned)(255.0f * std::min(1.0f, status.rested * 1.4f))));
    }

    if (status.hasCombat) {
        drawVitals(status, progress);
    } else {
        drawCourse(status);
    }

    if (status.hasWorld) {
        drawClockAndQuests(status, progress);
        drawNotices(progress);
    } else if (status.hasCombat) {
        char line[48];
        std::snprintf(line, sizeof(line), "Bandits left: %d", status.banditsLeft);
        centered(20.0f, min.y + 16.0f, Theme::Warning, line);
    }

    const ImVec2 middle(min.x + size.x * 0.5f, min.y + size.y * 0.5f);
    const bool talking = progress.getDialogue().open;

    if ((status.firstPerson || status.hasCombat) && !status.won && !talking) {
        list->AddCircleFilled(middle, 3.5f, SHADOW, 12);
        list->AddCircleFilled(middle, 2.0f, Theme::Text, 12);
        if (!ctx.wantsMouseCapture) centered(15.0f, middle.y + 24.0f, Theme::Text, "Click to look around");
    }

    if (!status.prompt.empty() && !talking) {
        char line[160];
        std::snprintf(line, sizeof(line), "F   %s", status.prompt.c_str());
        centered(17.0f, middle.y + 44.0f, Theme::Text, line);
    }

    if (talking) drawDialogue(progress);
    if (showInventory && !talking) drawInventory(progress);
    if (showJournal && !talking) drawJournal(progress);

    drawEnd(status);
    list->PopClipRect();
}

void GameHud::drawCourse(const GameStatus& status) {
    char line[64];
    std::snprintf(line, sizeof(line), "%d / %d", status.collected, status.total);
    text(26.0f, ImVec2(min.x + 20.0f, min.y + 16.0f), Theme::Accent, line);

    std::snprintf(line, sizeof(line), "%d:%02d", (int)status.seconds / 60, (int)status.seconds % 60);
    const float width = font->CalcTextSizeA(26.0f, FLT_MAX, 0.0f, line).x;
    text(26.0f, ImVec2(max.x - width - 20.0f, min.y + 16.0f), Theme::Text, line);

    if (status.falls > 0) {
        std::snprintf(line, sizeof(line), status.falls == 1 ? "1 fall" : "%d falls", status.falls);
        text(15.0f, ImVec2(min.x + 20.0f, min.y + 48.0f), Theme::TextDim, line);
    }
    if (status.goalLocked) {
        centered(20.0f, min.y + size.y * 0.38f, Theme::Warning, "Collect every crystal to open the gate");
    } else if (status.seconds < 6.0f) {
        centered(15.0f, max.y - 34.0f, Theme::Text,
                 status.firstPerson ? "W A S D move    Space jump    Mouse look    Esc free the mouse    V change view"
                                    : "W A S D move    Space jump    Q E turn the camera    V change view    R restart");
    }
}

void GameHud::drawVitals(const GameStatus& status, const GameProgress& progress) {
    const ImVec2 at(min.x + MARGIN, min.y + MARGIN);
    const float width = 190.0f;

    bar(at, width, status.health / status.maxHealth, HEALTH);
    bar(ImVec2(at.x, at.y + 14.0f), width * 0.8f, status.stamina / 100.0f, STAMINA);

    char line[96];
    float y = at.y + 30.0f;
    if (status.hasWorld) {
        bar(ImVec2(at.x, y), width * 0.6f, (float)progress.xp / progress.xpForNextLevel(), EXPERIENCE);
        y += 14.0f;
        std::snprintf(line, sizeof(line), "Level %d    %d gold", progress.level, progress.gold);
        text(16.0f, ImVec2(at.x, y), Theme::Text, line);
        y += 22.0f;
    }

    const bool arrows = progress.usesArrows();
    if (status.weapon == 0) {
        std::snprintf(line, sizeof(line), "Empty hands");
    } else if (status.weapon == 1) {
        std::snprintf(line, sizeof(line), "Sword");
    } else if (arrows) {
        std::snprintf(line, sizeof(line), "Bow    %d arrows", progress.count("arrow"));
    } else {
        std::snprintf(line, sizeof(line), "Bow");
    }
    text(16.0f, ImVec2(at.x, y), Theme::Text, line);

    const std::string potion = progress.bestPotion();
    if (!potion.empty()) {
        std::snprintf(line, sizeof(line), "H   %s x%d", progress.itemName(potion).c_str(), progress.count(potion));
        text(14.0f, ImVec2(at.x, y + 22.0f), Theme::TextDim, line);
    }

    if (status.underwater || status.breath < status.maxBreath - 0.05f) {
        const float width = 180.0f;
        bar(ImVec2(0.5f * (min.x + max.x) - 0.5f * width, max.y - 84.0f), width, status.breath / status.maxBreath, BREATH);
    }
    if (status.burning) centered(16.0f, max.y - 110.0f, Theme::rgb(0xFF8A3D), "You are burning");
    if (status.swimming) {
        centered(15.0f, max.y - 60.0f, Theme::Text, status.diving ? "Diving    Space rise" : "Swimming    Ctrl or C dive");
    }

    if (status.seconds < 8.0f && !progress.getDialogue().open) {
        centered(14.0f, max.y - 34.0f, Theme::Text,
                 status.hasWorld ? "W A S D move   Shift run   Click attack   F use   1 2 weapons   3 empty hands   H potion   Tab bag   J journal   V view"
                                 : "W A S D move    Mouse look    Click attack    1 sword    2 bow    3 empty hands    F ride    V change view    Esc free mouse");
    }
}

void GameHud::drawClockAndQuests(const GameStatus& status, const GameProgress& progress) {
    char line[96];
    const int hours = (int)status.hour;
    const int minutes = (int)((status.hour - hours) * 60.0f);
    std::snprintf(line, sizeof(line), "Day %d   %02d:%02d", status.day, hours, minutes);
    const float width = font->CalcTextSizeA(18.0f, FLT_MAX, 0.0f, line).x;
    text(18.0f, ImVec2(max.x - width - MARGIN, min.y + MARGIN), status.night ? Theme::rgb(0x9DB0E0) : Theme::Text, line);

    // The quests under way, newest last, as the tracker.
    float y = min.y + MARGIN + 30.0f;
    int shown = 0;
    for (const QuestDefinition& quest : progress.getQuests()) {
        if (progress.getQuest(quest.id).stage != QuestStage::Active || shown == 3) continue;
        const std::string detail = progress.describe(quest);
        const float right = max.x - MARGIN;
        const float titleWidth = font->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, quest.title.c_str()).x;
        text(15.0f, ImVec2(right - titleWidth, y), Theme::Accent, quest.title.c_str());
        const float detailWidth = font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, detail.c_str()).x;
        text(14.0f, ImVec2(right - detailWidth, y + 18.0f), Theme::Text, detail.c_str());
        y += 42.0f;
        shown++;
    }
}

void GameHud::drawNotices(const GameProgress& progress) {
    float y = min.y + size.y * 0.32f;
    for (const Notice& notice : progress.getNotices()) {
        const unsigned alpha = (unsigned)(255.0f * std::min(1.0f, notice.seconds));
        list->AddText(font, 16.0f, ImVec2(min.x + MARGIN + 2.0f, y + 2.0f), Theme::rgb(0x000000, alpha * 200 / 255), notice.text.c_str());
        list->AddText(font, 16.0f, ImVec2(min.x + MARGIN, y), (Theme::Text & 0x00FFFFFF) | (alpha << 24), notice.text.c_str());
        y += 22.0f;
    }
}

void GameHud::drawDialogue(const GameProgress& progress) {
    const DialogueView& dialogue = progress.getDialogue();
    const float width = std::min(size.x - 80.0f, 720.0f);
    const float padding = 16.0f;
    const float textWidth = width - padding * 2.0f;

    const float textHeight = font->CalcTextSizeA(16.0f, FLT_MAX, textWidth, dialogue.text.c_str()).y;
    const float height = padding * 2.0f + 26.0f + textHeight + 12.0f + dialogue.options.size() * 24.0f;

    const ImVec2 from(min.x + (size.x - width) * 0.5f, max.y - height - 24.0f);
    panel(from, ImVec2(from.x + width, from.y + height));

    float y = from.y + padding;
    list->AddText(font, 18.0f, ImVec2(from.x + padding, y), Theme::Accent, dialogue.speaker.c_str());
    y += 26.0f;
    y += wrapped(16.0f, ImVec2(from.x + padding, y), textWidth, Theme::Text, dialogue.text.c_str()) + 12.0f;

    char line[200];
    for (size_t i = 0; i < dialogue.options.size(); i++) {
        std::snprintf(line, sizeof(line), "%zu   %s", i + 1, dialogue.options[i].c_str());
        list->AddText(font, 16.0f, ImVec2(from.x + padding, y), Theme::rgb(0xE8D9A8), line);
        y += 24.0f;
    }
}

void GameHud::drawInventory(const GameProgress& progress) {
    const auto& items = progress.getInventory();
    const float width = 300.0f;
    const float height = 64.0f + std::max<size_t>(items.size(), 1) * 22.0f;
    const ImVec2 from(min.x + MARGIN, min.y + 150.0f);
    panel(from, ImVec2(from.x + width, from.y + height));

    char line[128];
    std::snprintf(line, sizeof(line), "Bag    %d gold", progress.gold);
    list->AddText(font, 18.0f, ImVec2(from.x + 14.0f, from.y + 12.0f), Theme::Accent, line);

    float y = from.y + 44.0f;
    if (items.empty()) list->AddText(font, 15.0f, ImVec2(from.x + 14.0f, y), Theme::TextDim, "Nothing yet");
    for (const auto& [id, count] : items) {
        std::snprintf(line, sizeof(line), "%s", progress.itemName(id).c_str());
        list->AddText(font, 15.0f, ImVec2(from.x + 14.0f, y), Theme::Text, line);
        std::snprintf(line, sizeof(line), "x%d", count);
        list->AddText(font, 15.0f, ImVec2(from.x + width - 50.0f, y), Theme::TextDim, line);
        y += 22.0f;
    }
}

void GameHud::drawJournal(const GameProgress& progress) {
    const float width = 380.0f;
    const ImVec2 from(max.x - width - MARGIN, min.y + 150.0f);

    float height = 48.0f;
    for (const QuestDefinition& quest : progress.getQuests()) {
        const QuestStage stage = progress.getQuest(quest.id).stage;
        if (stage == QuestStage::Active || stage == QuestStage::Done) height += 44.0f;
    }
    panel(from, ImVec2(from.x + width, from.y + std::max(height, 80.0f)));
    list->AddText(font, 18.0f, ImVec2(from.x + 14.0f, from.y + 12.0f), Theme::Accent, "Journal");

    float y = from.y + 44.0f;
    bool any = false;
    for (const QuestDefinition& quest : progress.getQuests()) {
        const QuestStage stage = progress.getQuest(quest.id).stage;
        if (stage != QuestStage::Active && stage != QuestStage::Done) continue;
        any = true;
        list->AddText(font, 15.0f, ImVec2(from.x + 14.0f, y), stage == QuestStage::Done ? Theme::TextDim : Theme::Text, quest.title.c_str());
        const std::string detail = progress.describe(quest);
        list->AddText(font, 14.0f, ImVec2(from.x + 14.0f, y + 18.0f), stage == QuestStage::Done ? Theme::TextDim : Theme::rgb(0xE8D9A8),
                      detail.c_str());
        y += 44.0f;
    }
    if (!any) list->AddText(font, 15.0f, ImVec2(from.x + 14.0f, y), Theme::TextDim, "Talk to people to find work");
}

void GameHud::drawEnd(const GameStatus& status) {
    if (!status.won) return;
    const float middle = min.y + size.y * 0.38f;
    char line[64];
    std::snprintf(line, sizeof(line), "%d:%02d with %d %s", (int)status.seconds / 60, (int)status.seconds % 60, status.falls,
                  status.falls == 1 ? "fall" : "falls");
    centered(44.0f, middle, Theme::Accent, status.hasCombat ? "Victory" : "Run complete");
    centered(20.0f, middle + 56.0f, Theme::Text, line);
    centered(15.0f, middle + 88.0f, Theme::TextDim, "Press R to play again");
}

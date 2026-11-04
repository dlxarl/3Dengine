#pragma once
#include "EditorContext.h"

#include "imgui.h"

// What the player sees over the Game view while playing: health and stamina, the time, the
// quest being followed, what can be done right now, conversations, and on request the
// inventory (Tab) and the journal (J).
class GameHud {
public:
    void draw(EditorContext& ctx, ImVec2 imageMin, ImVec2 imageSize);

private:
    bool showInventory = false;
    bool showJournal = false;

    ImDrawList* list = nullptr;
    ImFont* font = nullptr;
    ImVec2 min;
    ImVec2 max;
    ImVec2 size;

    void text(float fontSize, ImVec2 position, ImU32 color, const char* value) const;
    void centered(float fontSize, float y, ImU32 color, const char* value) const;
    float wrapped(float fontSize, ImVec2 position, float width, ImU32 color, const char* value) const;
    void bar(ImVec2 position, float width, float fraction, ImU32 color) const;
    void panel(ImVec2 from, ImVec2 to) const;

    void drawCourse(const GameStatus& status);
    void drawVitals(const GameStatus& status, const GameProgress& progress);
    void drawClockAndQuests(const GameStatus& status, const GameProgress& progress);
    void drawNotices(const GameProgress& progress);
    void drawDialogue(const GameProgress& progress);
    void drawInventory(const GameProgress& progress);
    void drawJournal(const GameProgress& progress);
    void drawEnd(const GameStatus& status);
};

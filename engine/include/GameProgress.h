#pragma once
#include "GameDefinition.h"

#include <map>
#include <string>
#include <vector>

struct Notice {
    std::string text;
    float seconds;
};

// What the conversation box shows.
struct DialogueView {
    bool open = false;
    std::string speaker;
    std::string text;
    std::vector<std::string> options;
};

enum class QuestStage { Unavailable, Available, Active, Done };

struct QuestProgress {
    QuestStage stage = QuestStage::Unavailable;
    int count = 0;
};

// The player's inventory, money, experience and quests, and the conversations that move them
// along. It knows nothing of the scene: the session reports what happens in the world.
class GameProgress {
public:
    void begin(const GameDefinition& definition);
    void update(float deltaTime);

    // Inventory. Gold is an item too, as "gold".
    int count(const std::string& item) const;
    void give(const std::string& item, int amount, bool announce = true);
    bool take(const std::string& item, int amount);
    const std::map<std::string, int>& getInventory() const { return inventory; }
    const ItemDefinition* findItem(const std::string& item) const { return definition.findItem(item); }
    std::string itemName(const std::string& item) const;
    bool usesArrows() const { return definition.findItem("arrow") != nullptr; }
    float bestWeaponDamage(float fallback) const;
    std::string bestPotion() const;  // the strongest healing item carried, or empty

    int gold = 0;
    void loseGold(int amount) { gold = gold > amount ? gold - amount : 0; }

    // Experience
    int level = 1;
    int xp = 0;
    int xpForNextLevel() const { return 100 * level; }
    float maxHealth() const { return 100.0f + 15.0f * (level - 1); }
    float damageScale() const { return 1.0f + 0.1f * (level - 1); }
    void addXp(int amount);

    // What happens in the world, which quests may be waiting for.
    void onKill(const std::string& role, const std::string& name);
    void onReach(const std::string& objectName);
    std::vector<std::string> reachTargets() const;

    // Conversations. choose() takes a 0-based option.
    void openDialogue(const std::string& character);
    void openMessage(const std::string& title, const std::string& text);
    void choose(int option);
    void closeDialogue();
    const DialogueView& getDialogue() const { return dialogue; }
    const std::string& getPartner() const { return partner; }

    // Quests
    const std::vector<QuestDefinition>& getQuests() const { return definition.quests; }
    const QuestProgress& getQuest(const std::string& id) const;
    std::string describe(const QuestDefinition& quest) const;
    bool hasQuests() const { return !definition.quests.empty(); }
    bool allQuestsDone() const;

    void notify(const std::string& text, float seconds = 4.0f);
    const std::vector<Notice>& getNotices() const { return notices; }

private:
    struct Action {
        enum Kind { Close, Accept, Complete, Buy, Sell } kind = Close;
        std::string id;
        int price = 0;
        int amount = 0;
    };

    GameDefinition definition;
    std::map<std::string, int> inventory;
    std::map<std::string, QuestProgress> quests;
    std::vector<Notice> notices;

    DialogueView dialogue;
    std::vector<Action> actions;
    std::string partner;
    std::map<std::string, size_t> nextLine;

    bool objectiveMet(const QuestDefinition& quest) const;
    std::string turnInOf(const QuestDefinition& quest) const;
    void refreshAvailability();
    void complete(const QuestDefinition& quest);
    int saleValue() const;
    void addOption(const std::string& text, Action action);
};

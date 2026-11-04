#pragma once
#include <string>
#include <vector>

// What a game is made of beyond its objects: items, characters to talk to, quests and the
// passing of time. Stored with the scene, so that a game is one file the Editor can open.

struct ItemStack {
    std::string item;
    int count = 1;
};

struct ItemDefinition {
    std::string id;
    std::string name;
    std::string kind;     // potion, arrow, weapon, pelt, meat, valuable, herb, quest
    int value = 0;        // what a merchant pays for it
    float heal = 0.0f;    // potions and food
    float damage = 0.0f;  // weapons; the best one carried is the one used
};

struct QuestDefinition {
    std::string id;
    std::string title;
    std::string summary;   // the task, in a line, for the quest tracker
    std::string giver;     // who offers it, by object name
    std::string turnIn;    // who it is handed in to; empty means the giver
    std::string after;     // a quest that must be done before this one is offered
    std::string offer;     // what the giver says when offering it
    std::string progress;  // ...while it is under way
    std::string complete;  // ...when it is handed in

    // talk: speak to target. reach: get close to the object named target.
    // collect: carry count of item target, handed over at the end.
    // kill: defeat count of target, which is a role (wolf, bandit) or the start of a name.
    std::string objective;
    std::string target;
    int count = 1;

    int rewardGold = 0;
    int rewardXp = 0;
    std::vector<ItemStack> rewardItems;
};

struct TradeOffer {
    std::string item;
    int price = 0;
    int count = 1;
};

// Someone the player can talk to. Matched to a Villager object by name.
struct CharacterDefinition {
    std::string name;
    std::vector<std::string> lines;  // said in turn when there is nothing else to say
    bool stays = true;               // keeps to where it was placed instead of wandering
    std::vector<TradeOffer> sells;
    bool buys = false;               // buys pelts, meat and valuables at their value
};

struct WorldDefinition {
    float dayMinutes = 0.0f;  // real minutes per game day; 0 leaves the sky as it is
    float startHour = 9.0f;
    int startGold = 0;
    std::vector<ItemStack> startItems;
};

struct GameDefinition {
    WorldDefinition world;
    std::vector<ItemDefinition> items;
    std::vector<QuestDefinition> quests;
    std::vector<CharacterDefinition> characters;

    bool isEmpty() const;
    const ItemDefinition* findItem(const std::string& id) const;
    const CharacterDefinition* findCharacter(const std::string& name) const;
};

#include "GameDefinition.h"

bool GameDefinition::isEmpty() const {
    return items.empty() && quests.empty() && characters.empty() && world.dayMinutes <= 0.0f &&
           world.startGold == 0 && world.startItems.empty();
}

const ItemDefinition* GameDefinition::findItem(const std::string& id) const {
    for (const ItemDefinition& item : items) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

const CharacterDefinition* GameDefinition::findCharacter(const std::string& name) const {
    for (const CharacterDefinition& character : characters) {
        if (character.name == name) return &character;
    }
    return nullptr;
}

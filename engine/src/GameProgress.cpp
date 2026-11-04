#include "GameProgress.h"

#include <algorithm>

namespace {
    const QuestProgress NO_PROGRESS;

    bool startsWith(const std::string& text, const std::string& prefix) {
        return !prefix.empty() && text.compare(0, prefix.size(), prefix) == 0;
    }

    bool isSellable(const ItemDefinition& item) {
        return item.value > 0 && (item.kind == "pelt" || item.kind == "meat" || item.kind == "valuable");
    }
}

void GameProgress::begin(const GameDefinition& source) {
    definition = source;
    inventory.clear();
    quests.clear();
    notices.clear();
    nextLine.clear();
    closeDialogue();

    gold = definition.world.startGold;
    level = 1;
    xp = 0;

    for (const ItemStack& stack : definition.world.startItems) {
        give(stack.item, stack.count, false);
    }
    for (const QuestDefinition& quest : definition.quests) {
        quests[quest.id] = QuestProgress();
    }
    refreshAvailability();
}

void GameProgress::update(float deltaTime) {
    for (Notice& notice : notices) notice.seconds -= deltaTime;
    notices.erase(std::remove_if(notices.begin(), notices.end(), [](const Notice& n) { return n.seconds <= 0.0f; }),
                  notices.end());
}

void GameProgress::notify(const std::string& text, float seconds) {
    notices.push_back({text, seconds});
    if (notices.size() > 6) notices.erase(notices.begin());
}

int GameProgress::count(const std::string& item) const {
    if (item == "gold") return gold;
    auto found = inventory.find(item);
    return found == inventory.end() ? 0 : found->second;
}

void GameProgress::give(const std::string& item, int amount, bool announce) {
    if (amount <= 0) return;
    if (item == "gold") {
        gold += amount;
    } else {
        inventory[item] += amount;
    }
    if (announce) notify("+" + std::to_string(amount) + " " + itemName(item));
}

bool GameProgress::take(const std::string& item, int amount) {
    if (count(item) < amount) return false;
    if (item == "gold") {
        gold -= amount;
        return true;
    }
    int& carried = inventory[item];
    carried -= amount;
    if (carried == 0) inventory.erase(item);
    return true;
}

std::string GameProgress::itemName(const std::string& item) const {
    if (item == "gold") return "gold";
    const ItemDefinition* found = definition.findItem(item);
    return found ? found->name : item;
}

float GameProgress::bestWeaponDamage(float fallback) const {
    float best = fallback;
    for (const auto& [id, carried] : inventory) {
        const ItemDefinition* item = definition.findItem(id);
        if (item && item->kind == "weapon") best = std::max(best, item->damage);
    }
    return best;
}

std::string GameProgress::bestPotion() const {
    std::string best;
    float most = 0.0f;
    for (const auto& [id, carried] : inventory) {
        const ItemDefinition* item = definition.findItem(id);
        if (item && item->kind == "potion" && item->heal > most) {
            most = item->heal;
            best = id;
        }
    }
    return best;
}

void GameProgress::addXp(int amount) {
    if (amount <= 0) return;
    xp += amount;
    notify("+" + std::to_string(amount) + " experience");
    while (xp >= xpForNextLevel()) {
        xp -= xpForNextLevel();
        level++;
        notify("Level " + std::to_string(level) + ": more health and harder blows", 6.0f);
    }
}

const QuestProgress& GameProgress::getQuest(const std::string& id) const {
    auto found = quests.find(id);
    return found == quests.end() ? NO_PROGRESS : found->second;
}

std::string GameProgress::turnInOf(const QuestDefinition& quest) const {
    return quest.turnIn.empty() ? quest.giver : quest.turnIn;
}

bool GameProgress::objectiveMet(const QuestDefinition& quest) const {
    if (quest.objective == "collect") return count(quest.target) >= quest.count;
    return getQuest(quest.id).count >= quest.count;
}

std::string GameProgress::describe(const QuestDefinition& quest) const {
    const QuestProgress& state = getQuest(quest.id);
    if (state.stage == QuestStage::Done) return "Done";
    if (objectiveMet(quest)) return "Return to " + turnInOf(quest);

    if (quest.objective == "collect") {
        return itemName(quest.target) + ": " + std::to_string(std::min(count(quest.target), quest.count)) + " / " +
               std::to_string(quest.count);
    }
    if (quest.objective == "kill" && quest.count > 1) {
        return quest.summary + " (" + std::to_string(state.count) + " / " + std::to_string(quest.count) + ")";
    }
    return quest.summary;
}

bool GameProgress::allQuestsDone() const {
    if (!hasQuests()) return false;
    for (const auto& [id, state] : quests) {
        if (state.stage != QuestStage::Done) return false;
    }
    return true;
}

void GameProgress::refreshAvailability() {
    for (const QuestDefinition& quest : definition.quests) {
        QuestProgress& state = quests[quest.id];
        if (state.stage == QuestStage::Unavailable &&
            (quest.after.empty() || getQuest(quest.after).stage == QuestStage::Done)) {
            state.stage = QuestStage::Available;
        }
    }
}

void GameProgress::onKill(const std::string& role, const std::string& name) {
    for (const QuestDefinition& quest : definition.quests) {
        QuestProgress& state = quests[quest.id];
        if (state.stage != QuestStage::Active || quest.objective != "kill" || state.count >= quest.count) continue;
        if (quest.target == role || startsWith(name, quest.target)) {
            state.count++;
            notify(quest.title + ": " + describe(quest));
        }
    }
}

void GameProgress::onReach(const std::string& objectName) {
    for (const QuestDefinition& quest : definition.quests) {
        QuestProgress& state = quests[quest.id];
        if (state.stage == QuestStage::Active && quest.objective == "reach" && quest.target == objectName &&
            state.count < quest.count) {
            state.count = quest.count;
            notify(quest.title + ": " + describe(quest), 5.0f);
        }
    }
}

std::vector<std::string> GameProgress::reachTargets() const {
    std::vector<std::string> targets;
    for (const QuestDefinition& quest : definition.quests) {
        if (getQuest(quest.id).stage == QuestStage::Active && quest.objective == "reach" && !objectiveMet(quest)) {
            targets.push_back(quest.target);
        }
    }
    return targets;
}

void GameProgress::addOption(const std::string& text, Action action) {
    dialogue.options.push_back(text);
    actions.push_back(std::move(action));
}

int GameProgress::saleValue() const {
    int value = 0;
    for (const auto& [id, carried] : inventory) {
        const ItemDefinition* item = definition.findItem(id);
        if (item && isSellable(*item)) value += item->value * carried;
    }
    return value;
}

void GameProgress::openDialogue(const std::string& character) {
    dialogue = DialogueView();
    actions.clear();
    dialogue.open = true;
    dialogue.speaker = character;
    partner = character;

    // Being spoken to is itself the goal of some quests.
    for (const QuestDefinition& quest : definition.quests) {
        QuestProgress& state = quests[quest.id];
        if (state.stage == QuestStage::Active && quest.objective == "talk" && quest.target == character) {
            state.count = quest.count;
        }
    }

    for (const QuestDefinition& quest : definition.quests) {
        if (getQuest(quest.id).stage == QuestStage::Active && turnInOf(quest) == character && objectiveMet(quest)) {
            dialogue.text = quest.complete;
            addOption("Continue", {Action::Complete, quest.id});
            return;
        }
    }

    for (const QuestDefinition& quest : definition.quests) {
        if (getQuest(quest.id).stage == QuestStage::Available && quest.giver == character) {
            dialogue.text = quest.offer;
            addOption("I will do it", {Action::Accept, quest.id});
            addOption("Not now", {Action::Close});
            return;
        }
    }

    const CharacterDefinition* who = definition.findCharacter(character);

    for (const QuestDefinition& quest : definition.quests) {
        if (getQuest(quest.id).stage == QuestStage::Active && (quest.giver == character || turnInOf(quest) == character)) {
            dialogue.text = quest.progress;
            break;
        }
    }

    if (dialogue.text.empty()) {
        if (who && !who->lines.empty()) {
            size_t& index = nextLine[character];
            dialogue.text = who->lines[index % who->lines.size()];
            index++;
        } else {
            dialogue.text = "...";
        }
    }

    if (who) {
        for (const TradeOffer& offer : who->sells) {
            const std::string amount = offer.count > 1 ? std::to_string(offer.count) + " " : "";
            addOption("Buy " + amount + itemName(offer.item) + " for " + std::to_string(offer.price) + " gold",
                      {Action::Buy, offer.item, offer.price, offer.count});
        }
        const int value = saleValue();
        if (who->buys && value > 0) {
            addOption("Sell pelts and goods for " + std::to_string(value) + " gold", {Action::Sell});
        }
    }
    addOption("Goodbye", {Action::Close});
}

void GameProgress::openMessage(const std::string& title, const std::string& text) {
    dialogue = DialogueView();
    actions.clear();
    partner.clear();
    dialogue.open = true;
    dialogue.speaker = title;
    dialogue.text = text;
    addOption("Close", {Action::Close});
}

void GameProgress::closeDialogue() {
    dialogue = DialogueView();
    actions.clear();
}

void GameProgress::complete(const QuestDefinition& quest) {
    if (quest.objective == "collect") take(quest.target, quest.count);

    quests[quest.id].stage = QuestStage::Done;
    notify("Quest completed: " + quest.title, 6.0f);

    if (quest.rewardGold > 0) give("gold", quest.rewardGold);
    for (const ItemStack& stack : quest.rewardItems) give(stack.item, stack.count);
    addXp(quest.rewardXp);

    refreshAvailability();
}

void GameProgress::choose(int option) {
    if (!dialogue.open || option < 0 || option >= (int)actions.size()) return;

    const Action action = actions[option];
    const std::string speaker = partner;

    switch (action.kind) {
        case Action::Close:
            closeDialogue();
            return;

        case Action::Accept:
            for (const QuestDefinition& quest : definition.quests) {
                if (quest.id == action.id) {
                    quests[quest.id].stage = QuestStage::Active;
                    quests[quest.id].count = 0;
                    notify("New quest: " + quest.title, 5.0f);
                }
            }
            closeDialogue();
            return;

        case Action::Complete:
            for (const QuestDefinition& quest : definition.quests) {
                if (quest.id == action.id) complete(quest);
            }
            closeDialogue();
            // Straight on to whatever this character has next, such as a follow-up quest.
            for (const QuestDefinition& quest : definition.quests) {
                if (getQuest(quest.id).stage == QuestStage::Available && quest.giver == speaker) {
                    openDialogue(speaker);
                    return;
                }
            }
            return;

        case Action::Buy:
            if (gold < action.price) {
                notify("Not enough gold");
            } else {
                gold -= action.price;
                give(action.id, action.amount);
            }
            openDialogue(speaker);
            return;

        case Action::Sell: {
            const int value = saleValue();
            for (auto it = inventory.begin(); it != inventory.end();) {
                const ItemDefinition* item = definition.findItem(it->first);
                it = item && isSellable(*item) ? inventory.erase(it) : std::next(it);
            }
            give("gold", value);
            openDialogue(speaker);
            return;
        }
    }
}

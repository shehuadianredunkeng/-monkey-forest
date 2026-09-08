#include "SaveManager.h"

#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "Item.h"
#include "Player.h"
#include "WorldState.h"

namespace {

struct PlayerSnapshot {
    int health = 100;
    int stamina = 60;
    int strength = 1;
    int wisdom = 1;
    int reputation = 0;
    std::map<SkillType, int> skills = {
        {SkillType::Gather, 1},
        {SkillType::Climb, 1},
        {SkillType::Combat, 1},
        {SkillType::Leadership, 1},
    };
    std::string roomId = "room_tree";
    std::vector<Item> items;
};

std::string resourceKey(ResourceType type) {
    switch (type) {
        case ResourceType::Food:
            return "resource_food";
        case ResourceType::Water:
            return "resource_water";
        case ResourceType::Morale:
            return "resource_morale";
        case ResourceType::MigrationSupply:
            return "resource_migration_supply";
    }
    return "resource_unknown";
}

std::optional<ResourceType> parseResourceKey(const std::string& token) {
    if (token == "resource_food") {
        return ResourceType::Food;
    }
    if (token == "resource_water") {
        return ResourceType::Water;
    }
    if (token == "resource_morale") {
        return ResourceType::Morale;
    }
    if (token == "resource_migration_supply") {
        return ResourceType::MigrationSupply;
    }
    return std::nullopt;
}

std::string skillKey(SkillType type) {
    switch (type) {
        case SkillType::Gather:
            return "skill_gather";
        case SkillType::Climb:
            return "skill_climb";
        case SkillType::Combat:
            return "skill_combat";
        case SkillType::Leadership:
            return "skill_leadership";
    }
    return "skill_unknown";
}

std::optional<SkillType> parseSkillKey(const std::string& token) {
    if (token == "skill_gather") {
        return SkillType::Gather;
    }
    if (token == "skill_climb") {
        return SkillType::Climb;
    }
    if (token == "skill_combat") {
        return SkillType::Combat;
    }
    if (token == "skill_leadership") {
        return SkillType::Leadership;
    }
    return std::nullopt;
}

bool validSnapshot(const PlayerSnapshot& snapshot) {
    return snapshot.health >= 0 && snapshot.health <= 100 &&
           snapshot.stamina >= 0 && snapshot.stamina <= 100 &&
           snapshot.strength >= 1 && snapshot.strength <= 5 &&
           snapshot.wisdom >= 1 && snapshot.wisdom <= 5 &&
           snapshot.reputation >= 0 && snapshot.reputation <= 100 &&
           snapshot.skills.at(SkillType::Gather) >= 1 &&
           snapshot.skills.at(SkillType::Gather) <= 3 &&
           snapshot.skills.at(SkillType::Climb) >= 1 &&
           snapshot.skills.at(SkillType::Climb) <= 3 &&
           snapshot.skills.at(SkillType::Combat) >= 1 &&
           snapshot.skills.at(SkillType::Combat) <= 3 &&
           snapshot.skills.at(SkillType::Leadership) >= 1 &&
           snapshot.skills.at(SkillType::Leadership) <= 3;
}

}  // namespace

bool SaveManager::saveGame(const std::string& path, const GameContext& ctx) const {
    std::ofstream out(path);
    if (!out) {
        return false;
    }

    out << "monkey_forest_save 1\n";
    out << "stage " << ctx.world.getStage() << '\n';
    out << "turns " << ctx.world.getTurnCount() << '\n';

    const ResourceType resources[] = {ResourceType::Food,
                                      ResourceType::Water,
                                      ResourceType::Morale,
                                      ResourceType::MigrationSupply};
    for (ResourceType type : resources) {
        out << resourceKey(type) << ' ' << ctx.world.getResource(type) << '\n';
    }

    for (const std::string& flag : ctx.world.getFlags()) {
        out << "flag " << std::quoted(flag) << '\n';
    }

    const Player& player = ctx.player;
    out << "player_health " << player.getHealth() << '\n';
    out << "player_stamina " << player.getStamina() << '\n';
    out << "player_strength " << player.getStrength() << '\n';
    out << "player_wisdom " << player.getWisdom() << '\n';
    out << "player_reputation " << player.getReputation() << '\n';

    const SkillType skills[] = {SkillType::Gather,
                                SkillType::Climb,
                                SkillType::Combat,
                                SkillType::Leadership};
    for (SkillType skill : skills) {
        out << skillKey(skill) << ' ' << player.getSkillLevel(skill) << '\n';
    }

    out << "room " << std::quoted(player.getCurrentRoomId()) << '\n';

    for (const Item& item : player.getInventory().getItems()) {
        out << "item " << std::quoted(item.getId()) << ' '
            << std::quoted(item.getName()) << ' ' << (item.isImportant() ? 1 : 0)
            << ' ' << item.getCount() << '\n';
    }

    return out.good();
}

bool SaveManager::loadGame(const std::string& path, GameContext& ctx) const {
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    std::string header;
    int version = 0;
    if (!(in >> header >> version) || header != "monkey_forest_save" ||
        version != 1) {
        return false;
    }

    WorldState loadedWorld;
    PlayerSnapshot snapshot;

    std::string key;
    while (in >> key) {
        if (key == "stage") {
            if (!(in >> version)) {
                return false;
            }
            loadedWorld.setStage(version);
            continue;
        }
        if (key == "turns") {
            if (!(in >> version)) {
                return false;
            }
            loadedWorld.setTurnCount(version);
            continue;
        }
        if (const auto resource = parseResourceKey(key)) {
            if (!(in >> version)) {
                return false;
            }
            loadedWorld.setResource(*resource, version);
            continue;
        }
        if (key == "flag") {
            std::string flag;
            if (!(in >> std::quoted(flag))) {
                return false;
            }
            loadedWorld.setFlag(flag);
            continue;
        }
        if (key == "player_health") {
            if (!(in >> snapshot.health)) {
                return false;
            }
            continue;
        }
        if (key == "player_stamina") {
            if (!(in >> snapshot.stamina)) {
                return false;
            }
            continue;
        }
        if (key == "player_strength") {
            if (!(in >> snapshot.strength)) {
                return false;
            }
            continue;
        }
        if (key == "player_wisdom") {
            if (!(in >> snapshot.wisdom)) {
                return false;
            }
            continue;
        }
        if (key == "player_reputation") {
            if (!(in >> snapshot.reputation)) {
                return false;
            }
            continue;
        }
        if (const auto skill = parseSkillKey(key)) {
            if (!(in >> version)) {
                return false;
            }
            snapshot.skills[*skill] = version;
            continue;
        }
        if (key == "room") {
            if (!(in >> std::quoted(snapshot.roomId))) {
                return false;
            }
            continue;
        }
        if (key == "item") {
            std::string id;
            std::string name;
            int important = 0;
            int count = 0;
            if (!(in >> std::quoted(id) >> std::quoted(name) >> important >> count)) {
                return false;
            }
            snapshot.items.emplace_back(id, name, important != 0, count);
            continue;
        }
        return false;
    }

    if (!validSnapshot(snapshot)) {
        return false;
    }

    Player loadedPlayer;
    loadedPlayer.changeHealth(snapshot.health - loadedPlayer.getHealth());
    loadedPlayer.changeStamina(snapshot.stamina - loadedPlayer.getStamina());
    loadedPlayer.changeStrength(snapshot.strength - loadedPlayer.getStrength());
    loadedPlayer.changeWisdom(snapshot.wisdom - loadedPlayer.getWisdom());
    loadedPlayer.changeReputation(snapshot.reputation -
                                  loadedPlayer.getReputation());
    for (const auto& [skill, level] : snapshot.skills) {
        const int current = loadedPlayer.getSkillLevel(skill);
        loadedPlayer.changeSkillLevel(skill, level - current);
    }
    loadedPlayer.setCurrentRoomId(snapshot.roomId);
    for (const Item& item : snapshot.items) {
        if (!loadedPlayer.addItem(item)) {
            return false;
        }
    }

    ctx.world = loadedWorld;
    ctx.player = loadedPlayer;
    return true;
}

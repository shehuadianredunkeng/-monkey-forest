#include "SaveSlots.h"

#include "SaveManager.h"
#include "WorldState.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>

namespace {
std::string roomName(const std::string& roomId) {
    static const std::map<std::string, std::string> names = {
        {"room_tree", "猴王树"}, {"room_forest", "果实森林"},
        {"room_river", "清泉河谷"}, {"room_cave", "回声山洞"},
        {"room_base", "实验基地"}};
    const auto found = names.find(roomId);
    return found == names.end() ? roomId : found->second;
}
}

SaveSlots::SaveSlots(std::string folder) : folder_(std::move(folder)) {}

std::string SaveSlots::path(int slot) const {
    return folder_ + "/slot" + std::to_string(slot) + ".txt";
}

bool SaveSlots::exists(int slot) const {
    return slot >= 1 && slot <= 3 && std::filesystem::exists(path(slot));
}

bool SaveSlots::any() const {
    return exists(1) || exists(2) || exists(3);
}

bool SaveSlots::save(int slot, const GameContext& ctx,
                     const SaveManager& manager) const {
    if (slot < 1 || slot > 3) return false;
    std::error_code error;
    std::filesystem::create_directories(folder_, error);
    return !error && manager.saveGame(path(slot), ctx);
}

bool SaveSlots::load(int slot, GameContext& ctx,
                     const SaveManager& manager) const {
    return exists(slot) && manager.loadGame(path(slot), ctx);
}

std::vector<std::string> SaveSlots::descriptions() const {
    std::vector<std::string> result;
    for (int slot = 1; slot <= 3; ++slot) {
        std::ifstream in(path(slot));
        if (!in) {
            result.push_back("空");
            continue;
        }
        std::string key;
        std::string roomId = "room_tree";
        int stage = 1;
        int turns = 0;
        while (in >> key) {
            if (key == "stage") in >> stage;
            else if (key == "turns") in >> turns;
            else if (key == "room") in >> std::quoted(roomId);
            else {
                std::string rest;
                std::getline(in, rest);
            }
        }
        result.push_back("第" + std::to_string(stage) + "阶段 · " +
                         roomName(roomId) + " · " + std::to_string(turns) +
                         "回合");
    }
    return result;
}

void mergeCollectionFlags(const WorldState& from, WorldState& into) {
    for (const std::string& flag : from.getFlags()) {
        if (flag.rfind("flag_collection_", 0) == 0)
            into.setFlag(flag);
    }
}

bool loadCollectionProfile(const std::string& path, WorldState& profile) {
    std::ifstream in(path);
    if (!in) return false;
    std::string flag;
    while (std::getline(in, flag)) {
        if (flag.rfind("flag_collection_", 0) == 0)
            profile.setFlag(flag);
    }
    return true;
}

bool saveCollectionProfile(const std::string& path,
                           const WorldState& profile) {
    std::ofstream out(path);
    if (!out) return false;
    for (const std::string& flag : profile.getFlags()) {
        if (flag.rfind("flag_collection_", 0) == 0) out << flag << '\n';
    }
    return out.good();
}

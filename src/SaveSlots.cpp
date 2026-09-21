#include "SaveSlots.h"

#include "SaveManager.h"
#include "WorldState.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>

using namespace std;

SaveSlots::SaveSlots(string folder) : folder_(move(folder)) {}

string SaveSlots::path(int slot) const {
    return folder_ + "/slot" + to_string(slot) + ".txt";
}

bool SaveSlots::exists(int slot) const {
    return slot >= 1 && slot <= 3 && filesystem::exists(path(slot));
}

bool SaveSlots::any() const {
    return exists(1) || exists(2) || exists(3);
}

int SaveSlots::mostRecentSlot() const {
    int newest = 0;
    filesystem::file_time_type newestTime{};
    for (int slot = 1; slot <= 3; ++slot) {
        if (!exists(slot)) continue;
        error_code error;
        const auto changed = filesystem::last_write_time(path(slot), error);
        if (!error && (newest == 0 || changed > newestTime)) {
            newest = slot;
            newestTime = changed;
        }
    }
    return newest;
}

bool SaveSlots::save(int slot, const GameContext& ctx,
                     const SaveManager& manager) const {
    if (slot < 1 || slot > 3) return false;
    error_code error;
    filesystem::create_directories(folder_, error);
    return !error && manager.saveGame(path(slot), ctx);
}

bool SaveSlots::load(int slot, GameContext& ctx,
                     const SaveManager& manager) const {
    return exists(slot) && manager.loadGame(path(slot), ctx);
}

vector<string> SaveSlots::descriptions(
    const map<string, Room>& rooms) const {
    // 菜单只需要阶段、位置和战斗血量，不需要把整份存档恢复出来。
    vector<string> result;
    for (int slot = 1; slot <= 3; ++slot) {
        ifstream in(path(slot));
        if (!in) {
            result.push_back("空");
            continue;
        }
        string key;
        string roomId = "room_tree";
        int stage = 1;
        int turns = 0;
        bool inBattle = false;
        string enemyHealth;
        while (in >> key) {
            if (key == "stage") in >> stage;
            else if (key == "turns") in >> turns;
            else if (key == "room") in >> quoted(roomId);
            else if (key == "flag") {
                string flag;
                in >> quoted(flag);
                // 战斗信息也存在 flag 里，这里只取出存档列表要显示的部分。
                if (flag.rfind("flag_saved_battle_enemy_", 0) == 0)
                    inBattle = true;
                const string prefix = "flag_saved_battle_health_";
                if (flag.rfind(prefix, 0) == 0)
                    enemyHealth = flag.substr(prefix.size());
            }
            else {
                string rest;
                getline(in, rest);
            }
        }
        const auto room = rooms.find(roomId);
        const string name = room == rooms.end() ? roomId : room->second.getName();
        result.push_back("第" + to_string(stage) + "阶段 · " +
                         name + " · " + to_string(turns) +
                         "回合" + (inBattle ? " · 战斗中（敌生命" + enemyHealth + "）" : ""));
    }
    return result;
}

void mergeCollectionFlags(const WorldState& from, WorldState& into) {
    for (const string& flag : from.getFlags()) {
        if (flag.rfind("flag_collection_", 0) == 0)
            into.setFlag(flag);
    }
}

bool loadCollectionProfile(const string& path, WorldState& profile) {
    ifstream in(path);
    if (!in) return false;
    string flag;
    while (getline(in, flag)) {
        if (flag.rfind("flag_collection_", 0) == 0)
            profile.setFlag(flag);
    }
    return true;
}

bool saveCollectionProfile(const string& path,
                           const WorldState& profile) {
    ofstream out(path);
    if (!out) return false;
    for (const string& flag : profile.getFlags()) {
        if (flag.rfind("flag_collection_", 0) == 0) out << flag << '\n';
    }
    return out.good();
}

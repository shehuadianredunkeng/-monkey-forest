#pragma once

#include "WorldState.h"

#include <algorithm>
#include <string>

using namespace std;

// 敌人实例 ID 形如 enemy_bees@2，旗标名不能含 @。
inline string baseEnemyId(const string& id) {
    const size_t separator = id.find('@');
    return separator == string::npos ? id : id.substr(0, separator);
}

inline string safeFlagPart(string text) {
    replace(text.begin(), text.end(), '@', '_');
    return text;
}

// “随机事件已结束”不等于“豆豆已经回家”，只有真正完成 NPC 救援才算。
inline bool childReturnedToTree(const WorldState& world) {
    return world.hasFlag("flag_child_saved") ||
           world.hasFlag("flag_child_rescued") ||
           world.hasFlag("flag_child_returned");
}

constexpr int TURNS_PER_SEASON = 6;
constexpr int SEASON_COUNT = 4;

inline int seasonIndex(int turnCount) {
    return (turnCount / TURNS_PER_SEASON) % SEASON_COUNT;
}

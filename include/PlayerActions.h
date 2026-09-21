#pragma once

#include "CommonTypes.h"
#include "Player.h"

#include <string>

using namespace std;

// 从当前房间拾取指定物品。
ActionResult takeItem(const string& itemId, GameContext& ctx);

// 使用背包中的消耗品或检查剧情物品。
ActionResult useItem(const string& itemId, GameContext& ctx);

// 生成界面上显示的背包文字。
string showInventory(const Player& player);

#pragma once

#include "CommonTypes.h"
#include "Player.h"

#include <string>

using namespace std;

// 这里没有再设计一个动作类，三个接口都是普通的自由函数。
// 它们通过GameContext取得玩家、房间和世界状态，完成一次玩家操作。

// 动作函数：从当前房间拾取指定物品，返回本次动作是否成功。
ActionResult takeItem(const string& itemId, GameContext& ctx);

// 动作函数：使用背包中的消耗品或检查剧情物品。
ActionResult useItem(const string& itemId, GameContext& ctx);

// 查询函数：只读取Player，生成界面上显示的背包文字。
string showInventory(const Player& player);

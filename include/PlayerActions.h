#pragma once

#include "CommonTypes.h"
#include "Player.h"

#include <string>

using namespace std;

ActionResult takeItem(const string& itemId, GameContext& ctx);

ActionResult useItem(const string& itemId, GameContext& ctx);

string showInventory(const Player& player);

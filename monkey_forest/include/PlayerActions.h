#pragma once

#include "CommonTypes.h"
#include "Player.h"

#include <string>

ActionResult takeItem(const std::string& itemId, GameContext& ctx);

ActionResult useItem(const std::string& itemId, GameContext& ctx);

ActionResult trainSkill(SkillType type, GameContext& ctx);

ActionResult rest(GameContext& ctx);

std::string showInventory(const Player& player);

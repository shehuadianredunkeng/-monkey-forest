#pragma once

#include "CommonTypes.h"
#include "Player.h"
#include "WorldState.h"

void setTestPlayerStamina(Player& player, int stamina);
void setTestPlayerSkill(Player& player, SkillType skill, int level);
void setTestWorldFlag(WorldState& world, const std::string& flag, bool enabled);

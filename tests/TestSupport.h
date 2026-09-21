#pragma once

#include "CommonTypes.h"
#include "Player.h"
#include "WorldState.h"

#include <string>

using namespace std;

void setTestPlayerStamina(Player& player, int stamina);
void setTestPlayerStrength(Player& player, int strength);
void setTestPlayerSkill(Player& player, SkillType skill, int level);
void setTestWorldFlag(WorldState& world, const string& flag, bool enabled);

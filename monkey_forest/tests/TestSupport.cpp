#include "TestSupport.h"

void setTestPlayerStamina(Player& player, int stamina) {
    player.changeStamina(stamina - player.getStamina());
}

void setTestPlayerSkill(Player& player, SkillType skill, int level) {
    player.changeSkillLevel(skill, level - player.getSkillLevel(skill));
}

void setTestWorldFlag(WorldState& world, const std::string& flag, bool enabled) {
    if (enabled) {
        world.setFlag(flag);
    } else {
        world.removeFlag(flag);
    }
}

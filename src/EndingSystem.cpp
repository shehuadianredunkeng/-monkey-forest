#include "EndingSystem.h"

#include "WorldState.h"

std::string EndingSystem::determineEndingId(const GameContext& ctx) const {
    if (ctx.world.hasFlag("flag_choice_resist")) {
        return "ending_resist";
    }
    if (ctx.world.hasFlag("flag_choice_hack")) {
        return "ending_hack";
    }
    if (ctx.world.hasFlag("flag_choice_migrate")) {
        return "ending_migrate";
    }
    return "ending_fail";
}

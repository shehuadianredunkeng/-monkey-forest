#include "EndingSystem.h"

#include "WorldState.h"

using namespace std;

string EndingSystem::determineEndingId(const GameContext& ctx) const {
    // 前面剧情只负责留下选择标记，这里再把标记对应到具体结局。
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

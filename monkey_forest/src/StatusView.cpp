#include "StatusView.h"

#include <sstream>

#include "Player.h"
#include "WorldState.h"

namespace {

const char* skillLabel(SkillType type) {
    switch (type) {
        case SkillType::Gather:
            return "采集";
        case SkillType::Climb:
            return "攀爬";
        case SkillType::Combat:
            return "战斗";
        case SkillType::Leadership:
            return "领导";
    }
    return "未知";
}

int skillLevel(const GameContext& ctx, SkillType type) {
    return ctx.player.getSkillLevel(type);
}

}  // namespace

std::string buildStatusText(const GameContext& ctx) {
    const Player& player = ctx.player;
    const WorldState& world = ctx.world;

    std::ostringstream out;
    out << "===== 状态 =====\n";
    out << "阶段：" << world.getStage() << "    回合："
        << world.getTurnCount() << '\n';
    out << "位置：" << player.getCurrentRoomId() << '\n';
    out << "生命：" << player.getHealth()
        << "    体力：" << player.getStamina()
        << "    力量：" << player.getStrength()
        << "    智慧：" << player.getWisdom()
        << "    声望：" << player.getReputation() << '\n';

    const SkillType skills[] = {SkillType::Gather,
                                SkillType::Climb,
                                SkillType::Combat,
                                SkillType::Leadership};
    out << "技能：";
    for (SkillType skill : skills) {
        out << skillLabel(skill) << skillLevel(ctx, skill) << ' ';
    }
    out << '\n';

    out << "公共资源：食物" << world.getResource(ResourceType::Food)
        << " 水源" << world.getResource(ResourceType::Water)
        << " 士气" << world.getResource(ResourceType::Morale)
        << " 迁徙物资" << world.getResource(ResourceType::MigrationSupply)
        << '\n';

    const auto flags = world.getFlags();
    if (!flags.empty()) {
        out << "关键状态：";
        for (const std::string& flag : flags) {
            out << flag << ' ';
        }
        out << '\n';
    }

    return out.str();
}

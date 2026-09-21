#include "StatusView.h"

#include <map>
#include <sstream>
#include <vector>

#include "Player.h"
#include "Room.h"
#include "WorldState.h"

using namespace std;

namespace {

const char* skillLabel(SkillType type) {
    switch (type) {
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

string roomLabel(const GameContext& ctx) {
    const string& roomId = ctx.player.getCurrentRoomId();
    const auto it = ctx.rooms.find(roomId);
    return it == ctx.rooms.end() ? roomId : it->second.getName();
}

const map<string, string>& flagLabels() {
    static const map<string, string> labels = {
        {"flag_base_open", "废弃基地入口已打开"},
        {"flag_chip_found", "已找到星猿晶片"},
        {"flag_complete_log", "星猿日志已补全"},
        {"flag_water_fixed", "河谷水源已恢复"},
        {"flag_new_home_found", "已找到迁徙新家园"},
        {"flag_drone_analyzed", "已分析坠落无人机"},
        {"flag_scout_help", "闪尾愿意协助"},
        {"flag_scout_left", "闪尾已离队"},
        {"flag_healer_supplied", "叶婆婆提供了草药"},
        {"flag_child_found", "已发现豆豆"},
        {"flag_child_saved", "豆豆已获救"},
        {"flag_child_rescued", "豆豆已获救"},
        {"flag_child_returned", "豆豆已回到猴王树"},
        {"flag_child_carried_home", "你亲自背回豆豆"},
        {"flag_king_support", "猴王岩背支持你"},
        {"flag_bees_defeated", "已击退野蜂"},
        {"flag_robot_defeated", "已击败巡逻机器"},
        {"flag_hertz_defeated", "已击败赫兹"},
        {"flag_route_resist_ready", "反击路线准备完成"},
        {"flag_route_hack_ready", "智取路线准备完成"},
        {"flag_route_migrate_ready", "迁徙路线准备完成"},
        {"flag_choice_resist", "最终路线：正面反击"},
        {"flag_choice_hack", "最终路线：破解设备"},
        {"flag_choice_migrate", "最终路线：带领迁徙"},
        {"flag_final_choice", "最终抉择已完成"},
        {"flag_bad_ending_forest_fire", "已触发坏结局：放火烧山"},
        {"flag_bad_ending_second_banana", "已触发坏结局：有了第一次就有第二次"},
        {"flag_bad_ending_gluttony", "已触发坏结局：暴食罪"},
        {"flag_bad_ending_coward", "已触发坏结局：你是狗熊"},
        {"flag_hidden_ending_together_forever", "已触发隐藏结局：双宿双飞"},
        {"flag_hidden_ending_earth_gift", "已触发隐藏结局：地球的礼物"},
        {"flag_hidden_ending_spark", "已触发隐藏结局：星火"},
        {"flag_normal_ending_not_hero", "已触发普通结局：绝大多数的现实"},
        {"flag_achievement_monkey_borrow", "成就：吗喽的事怎么能叫偷呢"},
        {"flag_achievement_you_fight_back", "成就：你倒是还手啊"},
        {"flag_achievement_doudou_bond", "成就：不要小瞧你与豆豆的羁绊"},
        {"flag_achievement_no_rice", "成就：巧妇难为无米之炊"},
        {"flag_achievement_next_line_after_forest_fire", "成就：放火烧山的下一句"},
        {"flag_achievement_no_monkey_at_tree", "成就：猴王树查无此猴"},
        {"flag_achievement_last_season", "成就：最后的季节"},
        {"flag_achievement_combat_god_candidate", "成就：你是战神吗"},
        {"flag_achievement_combat_god", "成就：你就是战神"}
    };
    return labels;
}

bool shouldHideFlag(const string& flag) {
    return flag.rfind("flag_pending_", 0) == 0 ||
           flag.rfind("flag_wisdom_", 0) == 0 ||
           flag.rfind("flag_collection_", 0) == 0 ||
           flag.rfind("flag_taken_", 0) == 0 ||
           flag.rfind("flag_combat_victory_count_", 0) == 0 ||
           flag.rfind("flag_saved_battle_", 0) == 0 ||
           flag.rfind("flag_escape_count_", 0) == 0 ||
           flag == "flag_escape_normal_endings_locked" ||
           flag.rfind("flag_enemy_stolen_", 0) == 0;
}

}  // namespace

string buildStatusText(const GameContext& ctx) {
    const Player& player = ctx.player;
    const WorldState& world = ctx.world;

    ostringstream out;
    out << "===== 状态 =====\n";
    out << "阶段：" << world.getStage() << "    回合："
        << world.getTurnCount() << '\n';
    out << "位置：" << roomLabel(ctx) << '\n';
    out << "生命：" << player.getHealth()
        << "    体力：" << player.getStamina()
        << "    力量：" << player.getStrength()
        << "    智慧：" << player.getWisdom()
        << "    声望：" << player.getReputation() << '\n';

    const SkillType skills[] = {SkillType::Climb,
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
    vector<string> readableFlags;
    const auto& labels = flagLabels();
    for (const string& flag : flags) {
        const auto label = labels.find(flag);
        if (label != labels.end()) {
            readableFlags.push_back(label->second);
        } else if (!shouldHideFlag(flag)) {
            readableFlags.push_back("未知进度");
        }
    }

    if (!readableFlags.empty()) {
        out << "关键状态：";
        for (size_t i = 0; i < readableFlags.size(); ++i) {
            if (i > 0) out << "；";
            out << readableFlags[i];
        }
        out << '\n';
    }

    return out.str();
}

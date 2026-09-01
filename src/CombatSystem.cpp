#include "CombatSystem.h"

#include "Player.h"
#include "PlayerActions.h"
#include "WorldState.h"

#include <algorithm>
#include <string>

namespace {
std::string defeatedFlag(const std::string& id) {
    if (id == "enemy_bees") return "flag_bees_defeated";
    if (id == "enemy_robot") return "flag_robot_defeated";
    if (id == "enemy_hertz") return "flag_hertz_defeated";
    return "";
}

std::string normalizeBattleAction(const std::string& action) {
    if (action == "攻击") return "attack";
    if (action == "防御") return "guard";
    if (action == "分析") return "analyze";
    if (action == "破解") return "hack";
    if (action == "使用") return "use";
    if (action == "逃跑" || action == "撤退") return "escape";
    if (action == "香蕉" || action == "巴拿拿") return "banana";
    return action;
}
}

void CombatSystem::initializeEnemies() {
    enemies_.clear();
    enemies_.emplace("enemy_bees", Enemy{"enemy_bees", "野蜂群",
        "会在果树间聚散突袭的躁动蜂群。", 18, 6, 0, 5});
    enemies_.emplace("enemy_robot", Enemy{"enemy_robot", "星猿巡逻机",
        "每隔数回合蓄力射击的机械守卫。", 30, 9, 2, 12});
    enemies_.emplace("enemy_hertz", Enemy{"enemy_hertz", "赫兹",
        "拥有能源护甲的星猿工程官。", 42, 11, 3, 20});
}

const Enemy* CombatSystem::currentEnemy() const {
    const auto it = enemies_.find(battleState_.enemyId);
    return it == enemies_.end() ? nullptr : &it->second;
}

ActionResult CombatSystem::startBattle(const std::string& enemyId,
                                       GameContext& ctx) {
    if (enemies_.empty()) initializeEnemies();
    if (battleState_.inBattle)
        return {false, "当前战斗尚未结束。", false, false};
    const auto it = enemies_.find(enemyId);
    if (it == enemies_.end())
        return {false, "找不到这个敌人。", false, false};
    const std::string flag = defeatedFlag(enemyId);
    if (!flag.empty() && ctx.world.hasFlag(flag))
        return {false, "这个敌人已经被击败，不会重复出现。", false, false};

    battleState_ = {true, enemyId, it->second.getMaxHealth(), false, false, false};
    battleTurn_ = 0;
    enemyArmorActive_ = enemyId == "enemy_hertz";
    std::string hint;
    if (enemyId == "enemy_bees")
        hint = "蜂群怕稳固防守，选择“防御”可以打乱它们。";
    else if (enemyId == "enemy_robot")
        hint = "巡逻机每第三回合会蓄力射击；智慧足够时可以尝试“破解”。";
    else if (ctx.world.hasFlag("flag_scout_banana_promise")) {
        battleState_.awaitingBananaChoice = true;
        hint = "赫兹忽然递来一根巴拿拿。\n"
               "1. 把巴拿拿给闪尾\n"
               "2. 把巴拿拿吃了\n"
               "3. 拒绝香蕉【需要智慧≥3】\n"
               "请输入：香蕉 1/2/3（banana 1/2/3）。";
    } else
        hint = "能源护甲会削弱攻击；读过日志后可以尝试“分析”或“破解”。";
    return {true, "战斗开始：" + it->second.getName() + "，敌方生命" +
                  std::to_string(battleState_.enemyHealth) + "。" + hint,
            false, false};
}

int CombatSystem::playerAttackDamage(const Enemy& enemy,
                                     const GameContext& ctx) const {
    int damage = 3 + ctx.player.getStrength() * 2 +
                 ctx.player.getSkillLevel(SkillType::Combat) - enemy.getDefense();
    if (ctx.world.hasFlag("flag_king_support")) damage += 2;
    if (enemyArmorActive_) damage = std::max(1, damage / 3);
    return std::max(1, damage);
}

ActionResult CombatSystem::performBattleAction(const std::string& action,
                                               const std::string& target,
                                               GameContext& ctx) {
    if (!battleState_.inBattle)
        return {false, "当前不在战斗中。", false, false};
    const Enemy* enemy = currentEnemy();
    if (!enemy) {
        clearBattle();
        return {false, "战斗数据异常，战斗已结束。", false, false};
    }

    const std::string command = normalizeBattleAction(action);

    if (battleState_.awaitingBananaChoice || battleState_.bananaGreedLoop) {
        if (command != "banana")
            return {false, "赫兹正举着巴拿拿等你选择。请输入“香蕉 1/2/3（banana 1/2/3）”。",
                    false, false};
        return handleBananaChoice(target, ctx);
    }

    if (command == "attack") {
        ++battleTurn_;
        const int damage = playerAttackDamage(*enemy, ctx);
        battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - damage);
        if (battleState_.enemyHealth == 0) return finishVictory(ctx, *enemy);
        ActionResult result = enemyCounterAttack(ctx, false);
        result.message = "你造成" + std::to_string(damage) + "点伤害。" +
            (enemyArmorActive_ ? "能源护甲吸收了大部分冲击。" : "") + result.message;
        return result;
    }
    if (command == "guard") {
        ++battleTurn_;
        battleState_.playerGuarding = true;
        if (battleState_.enemyId == "enemy_bees") {
            battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - 2);
            if (battleState_.enemyHealth == 0) return finishVictory(ctx, *enemy);
        }
        ActionResult result = enemyCounterAttack(ctx, true);
        result.message = "你稳住重心摆出防御姿态。" + result.message;
        return result;
    }
    if (command == "analyze") {
        if (battleState_.enemyId != "enemy_hertz")
            return {false, "这个敌人没有能源护甲可分析。", false, false};
        if (!ctx.world.hasFlag("flag_complete_log") || ctx.player.getWisdom() < 3)
            return {false, "需要完整日志且智慧至少3，才能找出护甲弱点。", false, false};
        if (!enemyArmorActive_)
            return {false, "赫兹的能源护甲已经关闭。", false, false};
        ++battleTurn_;
        enemyArmorActive_ = false;
        ActionResult result = enemyCounterAttack(ctx, false);
        result.message = "你根据日志切断护甲供能。" + result.message;
        return result;
    }
    if (command == "hack") {
        int damage = 0;
        if (battleState_.enemyId == "enemy_robot" && ctx.player.getWisdom() >= 3)
            damage = 12;
        else if (battleState_.enemyId == "enemy_hertz" &&
                 ctx.player.getWisdom() >= 4 && ctx.world.hasFlag("flag_complete_log")) {
            damage = 8;
            enemyArmorActive_ = false;
        } else {
            return {false, "破解失败：目标不适用，或智慧与剧情条件不足。", false, false};
        }
        ++battleTurn_;
        battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - damage);
        if (battleState_.enemyHealth == 0) return finishVictory(ctx, *enemy);
        ActionResult result = enemyCounterAttack(ctx, false);
        result.message = "破解成功，造成" + std::to_string(damage) + "点伤害。" + result.message;
        return result;
    }
    if (command == "use") {
        if (target.empty()) return {false, "请输入要使用的物品ID。", false, false};
        ActionResult used = useItem(target, ctx);
        if (!used.success) return used;
        ++battleTurn_;
        ActionResult result = enemyCounterAttack(ctx, false);
        result.message = used.message + result.message;
        return result;
    }
    if (command == "escape") {
        if (!ctx.world.hasFlag("flag_skill_escape_unlocked"))
            return {false, "你还没有解锁“逃跑”。完成闪尾的藤蔓任务后再试。", false, false};
        clearBattle();
        return {true, "闪尾从天而降，抓起你就荡着藤蔓跑了。", true, false};
    }
    return {false, "无法识别这个战斗指令。你可以选择：攻击、防御、分析、破解、使用物品或逃跑。",
            false, false};
}

ActionResult CombatSystem::handleBananaChoice(const std::string& target,
                                              GameContext& ctx) {
    if (target != "1" && target != "2" && target != "3")
        return {false, "请选择 1、2 或 3。", false, false};

    if (battleState_.bananaGreedLoop) {
        if (target == "2") {
            ctx.player.changeHealth(-ctx.player.getHealth());
            ctx.world.setFlag("flag_bad_ending_second_banana");
            clearBattle();
            return {false,
                    "你嘴上说着不吃了，手却再次伸向巴拿拿。\n"
                    "坏结局：有了第一次就有第二次！",
                    true, true};
        }
        if (target == "3")
            return {false, "诱惑已经控制了你。现在只能选择“继续吃”或“不吃了”。",
                    false, false};

        ++battleTurn_;
        ActionResult hit = enemyCounterAttack(ctx, false);
        if (ctx.player.getHealth() <= 0) {
            ctx.world.setFlag("flag_bad_ending_gluttony");
            return {false,
                    "赫兹不断递来巴拿拿，而你只知道一根接一根地吃。\n"
                    "坏结局：你犯下了暴食罪！",
                    true, true};
        }
        hit.message = "你接过巴拿拿继续吃，赫兹趁机发动攻击。" + hit.message +
                      "\n1. 继续吃　2. 不吃了";
        return hit;
    }

    if (target == "1") {
        battleState_.awaitingBananaChoice = false;
        ctx.world.setFlag("flag_banana_given_to_scout");
        return {true,
                "你把巴拿拿抛给闪尾。闪尾一口接住：讲究！这边交给哥！\n"
                "香蕉诱惑已解除，可以继续战斗。",
                true, false};
    }
    if (target == "3") {
        if (ctx.player.getWisdom() < 3)
            return {false, "香味让你挪不开眼。拒绝香蕉需要智慧≥3。", false, false};
        battleState_.awaitingBananaChoice = false;
        ctx.world.setFlag("flag_banana_refused");
        return {true, "你识破了诱惑，果断拒绝巴拿拿。可以继续战斗。", true, false};
    }

    battleState_.awaitingBananaChoice = false;
    battleState_.bananaGreedLoop = true;
    ++battleTurn_;
    ActionResult hit = enemyCounterAttack(ctx, false);
    if (ctx.player.getHealth() <= 0) {
        ctx.world.setFlag("flag_bad_ending_gluttony");
        return {false, "你沉迷巴拿拿，直到倒在赫兹面前。\n坏结局：你犯下了暴食罪！",
                true, true};
    }
    hit.message = "你咬下一大口巴拿拿，注意力完全被香甜味道占据。" + hit.message +
                  "\n赫兹又递来一根：1. 继续吃　2. 不吃了";
    return hit;
}

ActionResult CombatSystem::enemyCounterAttack(GameContext& ctx, bool guarded) {
    const Enemy* enemy = currentEnemy();
    if (!enemy) return {false, "敌人数据不存在。", false, false};
    int attack = enemy->getAttack();
    std::string move = "敌人反击";
    if (battleState_.enemyId == "enemy_robot" && battleTurn_ % 3 == 0) {
        attack += 6;
        move = "巡逻机蓄力射击";
    } else if (battleState_.enemyId == "enemy_hertz" && battleTurn_ % 3 == 0) {
        attack += 5;
        move = "赫兹释放能源震荡";
    }
    int damage = std::max(1, attack - ctx.player.getStrength());
    if (ctx.world.hasFlag("flag_scout_help")) damage = std::max(1, damage - 2);
    if (guarded) damage = std::max(1, damage / 2);
    ctx.player.changeHealth(-damage);
    if (ctx.world.hasFlag("flag_healer_supplied") && ctx.player.getHealth() > 0)
        ctx.player.changeHealth(2);
    battleState_.playerGuarding = false;
    if (ctx.player.getHealth() <= 0) {
        clearBattle();
        return {false, move + "造成" + std::to_string(damage) +
                       "点伤害。你失去了意识。", true, false};
    }
    const std::string heal = ctx.world.hasFlag("flag_healer_supplied")
        ? "叶婆婆的草药让你恢复2点生命。" : "";
    return {true, move + "造成" + std::to_string(damage) +
                  "点伤害；敌方剩余生命" +
                  std::to_string(battleState_.enemyHealth) + "。" + heal,
            true, false};
}

ActionResult CombatSystem::finishVictory(GameContext& ctx, const Enemy& enemy) {
    ctx.player.changeReputation(enemy.getReputationReward());
    const std::string flag = defeatedFlag(enemy.getId());
    if (!flag.empty()) ctx.world.setFlag(flag);
    const int reward = enemy.getReputationReward();
    const std::string name = enemy.getName();
    clearBattle();
    return {true, "你击败了" + name + "。声望+" + std::to_string(reward) + "。",
            true, false};
}

const BattleState& CombatSystem::getBattleState() const { return battleState_; }
bool CombatSystem::isInBattle() const { return battleState_.inBattle; }

void CombatSystem::clearBattle() {
    battleState_ = BattleState{};
    battleTurn_ = 0;
    enemyArmorActive_ = false;
}

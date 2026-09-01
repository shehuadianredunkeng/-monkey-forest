#include "CombatSystem.h"

#include "Player.h"
#include "PlayerActions.h"
#include "Item.h"
#include "WorldState.h"

#include <algorithm>
#include <random>
#include <sstream>
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
    if (action == "偷窃" || action == "顺手牵羊" || action == "偷") return "steal";
    if (action == "背包") return "inventory";
    return action;
}

int randomPercent() {
    static thread_local std::mt19937 generator(std::random_device{}());
    return std::uniform_int_distribution<int>(1, 100)(generator);
}

std::string randomScoutFollowUp() {
    static const char* verbs[] = {"挠", "抠", "扁", "拍"};
    static const char* parts[] = {"屁股", "脑瓜子", "后腰", "脚后跟"};
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> verb(0, 3);
    std::uniform_int_distribution<int> part(0, 3);
    if (randomPercent() <= 20) return "闪尾趁乱冲对方扔了一块香蕉皮，追击造成3点伤害。";
    return "闪尾趁乱" + std::string(verbs[verb(generator)]) + "了对方的" +
           parts[part(generator)] + "，追击造成3点伤害。";
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

    battleState_ = BattleState{};
    battleState_.inBattle = true;
    battleState_.enemyId = enemyId;
    battleState_.enemyHealth = it->second.getMaxHealth();
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
    std::string tutorial;
    if (!ctx.world.hasFlag("flag_combat_tutorial_seen")) {
        ctx.world.setFlag("flag_combat_tutorial_seen");
        tutorial = "\n【首次战斗教学】攻击（attack）造成伤害；防御（guard）降低伤害；"
                   "偷窃（steal）每场限一次；背包（inventory）查看物品；"
                   "完成闪尾任务后可使用逃跑（escape）。特殊战斗会另行显示分析、破解等选项。\n";
    }
    const std::string flintWarning = ctx.player.hasItem("item_flint")
        ? "\n提示：你带着燧石，可输入“使用 燧石（use flint）”发动火攻；有较高风险，建议先存档。"
        : "";
    const std::string hertzLine = enemyId == "enemy_hertz"
        ? "\n赫兹：你们把守护叫作勇气，我把开发叫作进步。让我看看谁能站到最后。"
        : "";
    return {true, "战斗开始：" + it->second.getName() + "，敌方生命" +
                  std::to_string(battleState_.enemyHealth) + "。" + tutorial + hint +
                  flintWarning + hertzLine,
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
        int damage = playerAttackDamage(*enemy, ctx);
        std::string blessing;
        if (battleState_.doubleDamageTurns > 0) {
            damage *= 2;
            --battleState_.doubleDamageTurns;
            blessing = "豆豆的神秘祝福让本次攻击伤害翻倍！";
        }
        battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - damage);
        if (battleState_.enemyHealth == 0) return finishVictory(ctx, *enemy);
        std::string followUp;
        if (ctx.world.hasFlag("flag_scout_help") &&
            !ctx.world.hasFlag("flag_scout_left") && randomPercent() <= 35) {
            battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - 3);
            followUp = randomScoutFollowUp();
            if (battleState_.enemyHealth == 0) {
                ActionResult victory = finishVictory(ctx, *enemy);
                victory.message = blessing + "你造成" + std::to_string(damage) + "点伤害。" +
                                  followUp + victory.message;
                return victory;
            }
        }
        ActionResult result = enemyCounterAttack(ctx, false);
        result.message = blessing + "你造成" + std::to_string(damage) + "点伤害。" + followUp +
            (enemyArmorActive_ ? "能源护甲吸收了大部分冲击。" : "") + result.message;
        return result;
    }
    if (command == "guard") {
        ++battleTurn_;
        battleState_.playerGuarding = true;
        std::string guardText;
        if (battleState_.enemyId == "enemy_bees") {
            ++battleState_.consecutiveGuards;
            int counterDamage = 0;
            if (battleState_.consecutiveGuards == 1) {
                counterDamage = 6;
                guardText = "蜂群没料到你居然就地抱头，顿时乱了阵型。";
            } else if (battleState_.consecutiveGuards == 2) {
                counterDamage = 3;
                guardText = "蜂群逐渐找到了攻击你的方式，但你灵活走位，仍让它们撞作一团。";
            } else {
                ctx.world.setFlag("flag_achievement_you_fight_back");
                guardText = "蜂群已经看穿你的套路。隐藏成就解锁：你倒是还手啊！";
            }
            battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - counterDamage);
            if (battleState_.enemyHealth == 0) return finishVictory(ctx, *enemy);
        } else {
            battleState_.consecutiveGuards = 0;
        }
        const bool guardWorks = battleState_.enemyId != "enemy_bees" ||
                                battleState_.consecutiveGuards <= 2;
        ActionResult result = enemyCounterAttack(ctx, guardWorks);
        result.message = "你稳住重心摆出防御姿态。" + guardText + result.message;
        return result;
    }
    if (command == "steal") return handleTheft(ctx);
    if (command == "inventory") {
        return {true, showInventory(ctx.player) +
                      "\n战斗中可输入“使用 物品（use item）”。燧石可发动火攻。",
                false, false};
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
        if (target.empty()) return {false, "请输入要使用的物品名称。", false, false};
        if (target == "item_flint" || target == "flint" || target == "燧石")
            return handleFlintAttack(ctx);
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
        if (ctx.world.hasFlag("flag_scout_left"))
            return {false, "闪尾已经离开队伍，现在没人能带你荡出战场。", false, false};
        clearBattle();
        if (!ctx.world.hasFlag("flag_escape_used_1")) {
            ctx.world.setFlag("flag_escape_used_1");
        } else if (!ctx.world.hasFlag("flag_escape_used_2")) {
            ctx.world.setFlag("flag_escape_used_2");
        } else if (!ctx.world.hasFlag("flag_escape_used_3")) {
            ctx.world.setFlag("flag_escape_used_3");
        } else if (!ctx.world.hasFlag("flag_scout_wander_invitation_resolved")) {
            ctx.world.setFlag("flag_pending_scout_wander_choice");
            return {true,
                    "闪尾从天而降，抓起你就荡着藤蔓跑了。\n"
                    "落地后，闪尾忽然认真起来：小猴儿，咱俩配合这么默契，"
                    "干脆别守在这一棵树上了。跟哥一起闯荡天涯，怎么样？\n"
                    "1. 接受\n2. 拒绝\n请直接输入 1 或 2。",
                    true, false};
        }
        return {true, "闪尾从天而降，抓起你就荡着藤蔓跑了。", true, false};
    }
    return {false, "无法识别这个战斗指令。你可以选择：攻击、防御、分析、破解、使用物品或逃跑。",
            false, false};
}

ActionResult CombatSystem::chooseEscapeEndingOption(int option,
                                                    GameContext& ctx) {
    if (!ctx.world.hasFlag("flag_pending_scout_wander_choice"))
        return {false, "当前没有等待处理的闪尾邀请。", false, false};
    if (option != 1 && option != 2)
        return {false, "请选择 1 或 2。", false, false};

    ctx.world.removeFlag("flag_pending_scout_wander_choice");
    ctx.world.setFlag("flag_scout_wander_invitation_resolved");
    if (option == 1) {
        ctx.world.setFlag("flag_hidden_ending_together_forever");
        ctx.world.setFlag("flag_achievement_no_monkey_at_tree");
        return {true,
                "你伸出手，闪尾咧嘴一笑。两只吗喽抓住同一根藤蔓，"
                "越过河谷，也越过了青木谷的边界。\n"
                "隐藏结局：双宿双飞\n"
                "隐藏成就解锁：猴王树查无此猴",
                true, true};
    }

    ctx.world.setFlag("flag_scout_left");
    ctx.world.removeFlag("flag_scout_help");
    std::string cost;
    if (ctx.player.hasItem("item_fruit") && ctx.player.removeItem("item_fruit")) {
        cost = "闪尾临走时顺手带走了一个水果。";
    } else if (ctx.world.getResource(ResourceType::MigrationSupply) > 0) {
        ctx.world.changeResource(ResourceType::MigrationSupply, -1);
        cost = "闪尾临走时带走了1份迁徙物资。";
    } else if (ctx.world.getResource(ResourceType::Food) > 0) {
        ctx.world.changeResource(ResourceType::Food, -1);
        cost = "闪尾临走时带走了1份食物。";
    } else {
        cost = "闪尾翻遍背包也没找到水果，只好空着手走了。";
    }
    return {true,
            "你摇了摇头。闪尾沉默片刻，还是挥挥手独自荡向远方。" + cost +
            "闪尾已离开队伍。",
            true, false};
}

void CombatSystem::recordTheftAchievement(GameContext& ctx) {
    if (!ctx.world.hasFlag("flag_theft_streak_1")) {
        ctx.world.setFlag("flag_theft_streak_1");
    } else if (!ctx.world.hasFlag("flag_theft_streak_2")) {
        ctx.world.setFlag("flag_theft_streak_2");
    } else {
        ctx.world.setFlag("flag_achievement_monkey_borrow");
    }
}

ActionResult CombatSystem::handleTheft(GameContext& ctx) {
    const Enemy* enemy = currentEnemy();
    if (!enemy) return {false, "当前没有可以偷窃的目标。", false, false};
    if (battleState_.theftUsed)
        return {false, "多次偷窃使你良心不安，错失了良机。", false, false};
    battleState_.theftUsed = true;
    recordTheftAchievement(ctx);

    std::string reward;
    if (battleState_.enemyId == "enemy_bees") {
        ctx.player.addItem(Item("item_honey", "蜂蜜"));
        ctx.player.changeHealth(12);
        reward = "你从蜂巢边顺走一小罐蜂蜜，生命+12，并获得蜂蜜。";
    } else if (battleState_.enemyId == "enemy_robot") {
        ctx.player.addItem(Item("item_material_fragment", "材料碎片"));
        ctx.player.changeStrength(1);
        reward = "你拆下一块材料碎片，力量+1，并获得材料碎片。";
    } else {
        ctx.player.addItem(Item("item_book", "星猿研究手册"));
        ctx.player.changeWisdom(1);
        reward = "你顺走赫兹的研究手册，智慧+1，并获得星猿研究手册。";
    }
    if (ctx.world.hasFlag("flag_achievement_monkey_borrow"))
        reward += "隐藏成就解锁：吗喽的事怎么能叫偷呢！";

    ++battleTurn_;
    ActionResult result = enemyCounterAttack(ctx, false);
    result.message = reward + result.message;
    return result;
}

ActionResult CombatSystem::handleFlintAttack(GameContext& ctx) {
    if (!ctx.player.hasItem("item_flint"))
        return {false, "背包中没有燧石。", false, false};
    if (battleState_.fireUsed)
        return {false, "周围已经火星四溅，不能再次使用燧石。", false, false};
    battleState_.fireUsed = true;
    ++battleTurn_;
    if (randomPercent() <= 50) {
        ctx.player.changeHealth(-ctx.player.getHealth());
        ctx.world.setFlag("flag_bad_ending_forest_fire");
        ctx.world.setFlag("flag_achievement_next_line_after_forest_fire");
        clearBattle();
        return {false,
                "火星落进枯叶，风把火舌卷向整片青木谷。\n"
                "坏结局：放火烧山。\n"
                "隐藏成就解锁：放火烧山的下一句",
                true, true};
    }
    const Enemy* enemy = currentEnemy();
    if (!enemy) return {false, "敌人数据不存在。", false, false};
    battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - 20);
    if (battleState_.enemyHealth == 0) return finishVictory(ctx, *enemy);
    ActionResult result = enemyCounterAttack(ctx, false);
    result.message = "你用燧石发动火攻，造成20点伤害！" + result.message;
    return result;
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
    std::string blessing;
    // 选择沉迷香蕉后必须完整承担该路线后果，豆豆祝福不在此路线中打断结局。
    if (ctx.world.hasFlag("flag_child_rescued") && !battleState_.bananaGreedLoop) {
        const int roll = randomPercent();
        if (roll == 1) {
            ctx.world.setFlag("flag_achievement_doudou_bond");
            ActionResult victory = finishVictory(ctx, *enemy);
            victory.message = "豆豆的神秘祝福化作一道金光，直接击败了对手！"
                              "隐藏成就解锁：不要小瞧你与豆豆的羁绊啊！" + victory.message;
            return victory;
        }
        if (roll <= 6) {
            battleState_.doubleDamageTurns = 3;
            blessing = "豆豆的神秘祝福生效：接下来三次攻击伤害翻倍。";
        } else if (roll <= 26) {
            ctx.player.changeHealth(8);
            blessing = "豆豆的神秘祝福为你恢复8点生命。";
        }
    }
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
    if (ctx.world.hasFlag("flag_scout_help") && !ctx.world.hasFlag("flag_scout_left"))
        damage = std::max(1, damage - 2);
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
    return {true, blessing + move + "造成" + std::to_string(damage) +
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

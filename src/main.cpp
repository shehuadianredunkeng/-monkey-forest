#include "CombatSystem.h"
#include "CollectionSystem.h"
#include "EndingSystem.h"
#include "EventSystem.h"
#include "InteractiveMap.h"
#include "Item.h"
#include "NPCSystem.h"
#include "Player.h"
#include "PlayerActions.h"
#include "ProgressSystem.h"
#include "Room.h"
#include "SaveManager.h"
#include "SaveSlots.h"
#include "UI/InteractiveGameUI.h"
#include "UI/TextLayout.h"
#include "WorldState.h"

#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void configureApplication() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    wchar_t executable[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        std::error_code error;
        std::filesystem::current_path(
            std::filesystem::path(executable).parent_path(), error);
    }
#endif
}

void initializeNewWorld(WorldState& world) {
    world.setStage(1);
    world.setTurnCount(0);
    world.setResource(ResourceType::Food, 0);
    world.setResource(ResourceType::Water, 0);
    world.setResource(ResourceType::Morale, 50);
    world.setResource(ResourceType::MigrationSupply, 0);
}

ActionResult result(bool success, std::string message,
                    bool turn = false, bool stage = false) {
    return {success, std::move(message), turn, stage};
}

std::string trim(const std::string& text) {
    std::size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    std::size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

std::vector<std::string> words(const std::string& text) {
    std::istringstream input(text);
    std::vector<std::string> result;
    std::string word;
    while (input >> word) result.push_back(word);
    return result;
}

std::string lowerAscii(std::string text) {
    for (char& ch : text)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return text;
}

std::string pendingEnemyId(const WorldState& world) {
    if (world.hasFlag("flag_pending_battle_bees") &&
        !world.hasFlag("flag_bees_defeated")) return "enemy_bees";
    if (world.hasFlag("flag_pending_battle_robot") &&
        !world.hasFlag("flag_robot_defeated")) return "enemy_robot";
    if (world.hasFlag("flag_pending_battle_hertz") &&
        !world.hasFlag("flag_hertz_defeated")) return "enemy_hertz";
    return "";
}

bool hasPendingMainChoice(const WorldState& world) {
    for (const std::string& flag : world.getFlags())
        if (flag.rfind("flag_pending_event_", 0) == 0) return true;
    return false;
}

bool hasAllSeasonalRelics(const WorldState& world) {
    return world.hasFlag("flag_season_relic_spring") &&
           world.hasFlag("flag_season_relic_summer") &&
           world.hasFlag("flag_season_relic_autumn") &&
           world.hasFlag("flag_season_relic_winter");
}

std::string seasonName(int turn) {
    static const char* names[] = {"春", "夏", "秋", "冬"};
    return names[(turn / 6) % 4];
}

bool hasAnyFinalRoute(const GameContext& ctx) {
    const bool resist = ctx.world.hasFlag("flag_route_resist_ready") &&
        ctx.player.getReputation() >= 60 &&
        ctx.player.getSkillLevel(SkillType::Combat) >= 2;
    const bool hack = ctx.world.hasFlag("flag_route_hack_ready") &&
        ctx.player.getWisdom() >= 4 &&
        ctx.world.hasFlag("flag_complete_log");
    const bool migrate = ctx.world.hasFlag("flag_route_migrate_ready") &&
        ctx.world.getResource(ResourceType::MigrationSupply) >= 8 &&
        ctx.player.getSkillLevel(SkillType::Leadership) >= 2 &&
        ctx.world.hasFlag("flag_new_home_found");
    return resist || hack || migrate || hasAllSeasonalRelics(ctx.world);
}

std::string objective(const GameContext& ctx) {
    if (ctx.world.hasFlag("flag_pending_scout_wander_choice"))
        return "闪尾正在等待答复：直接按 1 接受，或按 2 拒绝。";
    if (hasPendingMainChoice(ctx.world))
        return "剧情正在等待选择：直接按数字键 1、2 或 3。";
    if (ctx.world.hasFlag("flag_final_choice") ||
        ctx.world.hasFlag("flag_hidden_ending_together_forever"))
        return "本轮故事已经结束，结局已收入收藏。";
    switch (ctx.world.getStage()) {
    case 1: return "前往果实森林，踩上红色！并按 Enter，完成树冠试炼。";
    case 2: return "返回猴王树，在红色！处处理寒冬缺粮。";
    case 3:
        return ctx.world.hasFlag("flag_event_glowing_river_done")
            ? "前往回声山洞，在红色！处追查机械回声。"
            : "前往清泉河谷，在红色！处调查发光河水。";
    case 4:
        return ctx.world.hasFlag("flag_event_drought_choice_done")
            ? "返回猴王树，在红色！处平息猴群分歧。"
            : "前往清泉河谷，在红色！处决定生存路线。";
    case 5: return "进入实验基地，在红色！处取得完整日志。";
    case 6: return "返回猴王树，在红色！处作出家园抉择。";
    default: return "自由探索青木谷。";
    }
}

std::string roomArrival(const GameContext& ctx) {
    const auto found = ctx.rooms.find(ctx.player.getCurrentRoomId());
    return found == ctx.rooms.end()
        ? "进入未知区域。"
        : "【进入" + found->second.getName() + "】\n" +
              found->second.getBaseDescription();
}

ActionResult takeMapItem(const std::string& itemId, GameContext& ctx) {
    const std::string flag = "flag_taken_" + ctx.player.getCurrentRoomId() +
                             "_" + itemId;
    if (ctx.world.hasFlag(flag)) return result(false, "这里已经空了。");
    ActionResult picked = takeItem(itemId, ctx);
    if (picked.success) ctx.world.setFlag(flag);
    return picked;
}

ActionResult openChest(const std::string& chestId, GameContext& ctx) {
    const std::string flag = "flag_opened_" + chestId;
    if (ctx.world.hasFlag(flag)) return result(false, "宝箱已经打开过了。");

    Item reward("item_fruit", "果实", false, 1);
    std::string text = "果实";
    if (chestId == "chest_forest") {
        reward = Item("item_herb", "草药", false, 1);
        text = "草药";
    } else if (chestId == "chest_cave") {
        reward = Item("item_rope", "藤索", true, 1);
        text = "藤索";
    } else if (chestId == "chest_base") {
        reward = Item("item_herb", "草药", false, 2);
        text = "两份草药";
    }
    if (!ctx.player.addItem(reward))
        return result(false, "背包已满，宝箱先替你保管奖励。");
    ctx.world.setFlag(flag);
    return result(true, "咔哒！你打开宝箱，获得" + text + "。", true);
}

ActionResult findEasterEgg(const std::string& eggId, GameContext& ctx) {
    const std::string flag = "flag_found_" + eggId;
    if (ctx.world.hasFlag(flag)) return result(false, "这里的秘密已经被发现了。");
    ctx.world.setFlag(flag);
    if (eggId.rfind("season_relic_", 0) == 0) {
        const std::string season = eggId.substr(std::string("season_relic_").size());
        std::string name = season == "spring" ? "春花" :
                           season == "summer" ? "蝉蜕" :
                           season == "autumn" ? "秋叶" : "落雪";
        ctx.world.setFlag("flag_season_relic_" + season);
        ctx.player.addItem(Item("item_" + season + "_token", name, true, 1));
        std::string message = "你小心收起黄色光芒中的【" + name + "】，获得一件四季信物。";
        if (hasAllSeasonalRelics(ctx.world)) {
            ctx.world.setFlag("flag_achievement_last_season");
            CollectionSystem().unlockAchievement("achievement_last_season", ctx.world);
            message += "\n【成就解锁】最后的季节";
        }
        return result(true, message, true);
    }
    if (eggId == "egg_tree_ring") {
        ctx.player.changeSkillLevel(SkillType::Leadership, 1);
        return result(true, "你数清了猴王树年轮中的旧记号。领导技能+1。", true);
    }
    if (eggId == "egg_golden_banana") {
        ctx.player.changeStrength(1);
        ctx.player.changeStamina(25);
        return result(true, "你发现一根金光闪闪的巴拿拿！力量+1，体力+25。", true);
    }
    if (eggId == "egg_moon_pebble") {
        ctx.player.changeWisdom(1);
        return result(true, "月光石映出了水流规律。智慧+1。", true);
    }
    if (eggId == "egg_stone_tablet") {
        ctx.player.changeWisdom(1);
        return result(true, "你读懂了模糊碑文：知识会为森林开路。智慧+1。", true);
    }
    ctx.player.changeSkillLevel(SkillType::Combat, 1);
    ctx.player.changeHealth(20);
    return result(true, "你按下测试香蕉，训练程序启动。战斗技能+1，生命+20。", true);
}

ActionResult resolveLocationEvent(const std::string& eventId,
                                  GameContext& ctx) {
    const std::string flag = "flag_resolved_" + eventId;
    if (ctx.world.hasFlag(flag))
        return result(false, "这处异动已经恢复平静。等待下一回合会出现新的地点事件。");
    ctx.world.setFlag(flag);
    const int kind = ctx.world.getTurnCount() % 5;
    if (kind == 0) {
        ctx.player.changeStamina(10);
        return result(true, "你在树根下找到一处避风窝，短暂休息后体力+10。", true);
    }
    if (kind == 1) {
        ctx.player.changeWisdom(1);
        return result(true, "风吹过石缝发出规律回声，你记下节奏，智慧+1。", true);
    }
    if (kind == 2) {
        ctx.world.changeResource(ResourceType::Food, 1);
        return result(true, "一群松鼠遗落了坚果，你将它们放进公共储粮，食物+1。", true);
    }
    if (kind == 3) {
        ctx.player.changeHealth(8);
        return result(true, "你发现一小片清凉苔藓，敷在伤口上，生命+8。", true);
    }
    ctx.player.changeReputation(2);
    return result(true, "你顺手扶起被风吹倒的路标，路过的吗喽向你致谢，声望+2。", true);
}

std::string battleHelp() {
    return "战斗仍使用短指令：\n"
           "攻击（attack）  防御（guard）  偷窃（steal）\n"
           "分析（analyze） 破解（hack） 逃跑（escape）\n"
           "使用 草药 / use herb；背包 / inventory；存档 / save\n"
           "赫兹递出香蕉时，可直接输入 1、2 或 3。";
}

std::string gameHelp() {
    return "【探索操作】\n"
           "W/A/S/D：在房间内移动\n"
           "↑/↓：逐行翻看以往剧情；PgUp/PgDn：快速翻页\n"
           "Enter / 空格：与身边目标互动\n"
           "I：查看背包    U：输入名称使用物品\n"
           "P：选择存档位保存    Esc：暂停菜单\n\n"
           "【地图图例】\n"
           "猴=玩家  友/伴=NPC  物=物品  宝=宝箱  敌=战斗  ！=关键剧情\n"
           "青色“门”可切换房间，红色“锁”表示尚未满足通行条件，“奇”是本回合地点事件。\n"
           "剧情和NPC选项出现后，直接按1/2/3/4。\n\n" +
           battleHelp();
}

std::string specialEndingId(const GameContext& ctx,
                            const EndingSystem& endings) {
    if (ctx.world.hasFlag("flag_hidden_ending_together_forever"))
        return "ending_together_forever";
    if (ctx.world.hasFlag("flag_bad_ending_forest_fire"))
        return "ending_forest_fire";
    if (ctx.world.hasFlag("flag_bad_ending_second_banana"))
        return "ending_second_banana";
    if (ctx.world.hasFlag("flag_bad_ending_gluttony"))
        return "ending_gluttony";
    if (ctx.world.hasFlag("flag_bad_ending_coward"))
        return "ending_coward";
    if (ctx.world.hasFlag("flag_hidden_ending_earth_gift"))
        return "ending_earth_gift";
    if (ctx.world.hasFlag("flag_hidden_ending_spark"))
        return "ending_spark";
    if (ctx.world.hasFlag("flag_normal_ending_not_hero"))
        return "ending_not_hero";
    if (ctx.player.getHealth() <= 0) return "ending_fail";
    return endings.determineEndingId(ctx);
}

std::string endingText(const std::string& id, const EventSystem& events) {
    if (id == "ending_together_forever")
        return "【隐藏结局：双宿双飞】\n你最终握住的不是武器，而是闪尾从树冠垂下的藤蔓。你们没有回头接受岩背准备好的庆功果，也没有等族群替你写下英雄的名字。夜风把青木谷的灯火推到身后，闪尾在前方笑着喊你跟紧一点，你第一次发现离开并不等于逃跑。一路上，你们替陌生猴群赶走盗果的山魈，在雨林深处交换各自没有讲完的故事，也会为了最后一根巴拿拿争得面红耳赤。多年以后，猴王树仍偶尔收到没有署名的叶片，上面画着两条并肩荡向远方的尾巴。你没有成为家园传说里的英雄，却找到了愿意与你共享危险、食物与明天的同伴。青木谷查无此猴，而世界的每片树冠，都可能留下你们经过的影子。";
    if (id == "ending_forest_fire")
        return "【隐藏结局：放火烧山】\n最初只是一粒不起眼的火星。你以为火焰会像战斗指令一样听话，烧到敌人脚下便乖乖停住，可干燥的落叶替它选择了完全不同的方向。风越过果实森林，把红光送上猴王树；河谷里的动物仓皇奔逃，赫兹的机器也在浓烟里失去轮廓。你抱着仅存的水罐站在灰烬边，终于明白力量如果没有判断，就会把想守护的一切变成代价。族群活了下来，却再也无法回到熟悉的枝头。往后的迁徙途中，没有谁责骂你，沉默反而比责骂更重。每当夜里点起篝火，你都会坐到最远处看守，生怕又有一粒火星越过石圈。放火烧山的下一句不再是玩笑，而是你用余生记住的警告。";
    if (id == "ending_second_banana")
        return "【坏结局：有了第一次就有第二次！】\n你明明已经说出“不吃了”，手却比意志更快地伸向下一根巴拿拿。赫兹没有催促，只把金黄的果实一根根摆在面前，像是在验证一条早已写进报告的结论：守护者也可以被最简单的欲望拖住。远处传来抽取塔启动的震动，你告诉自己再吃一口就回去，再休息一会儿就反击，可每一次让步都替下一次找好了理由。等盘子终于空了，清泉已经停止流动，猴王树的叶片也蒙上一层灰白。你没有输给更强的武器，而是输给那个不断替自己宽限的念头。后来族群谈起这场失败，总会提醒幼猴：真正危险的从来不是第一根香蕉，而是相信第二次仍然可以随时停下。";
    if (id == "ending_gluttony")
        return "【坏结局：你犯下了暴食罪！】\n赫兹每递来一根巴拿拿，你都听见同伴在身后呼喊；可香甜的气味盖过警报，也盖过你曾经说过的誓言。你机械地咀嚼，任由一次次攻击落在身上，还安慰自己吃饱以后会更有力气。最后一根香蕉落地时，你已经无力伸手，抽取塔的轰鸣却比任何时候都清晰。赫兹收起记录板，没有嘲笑，只平静地把“无法抵抗即时奖励”写进观察结论。猴群被迫离开枯竭的河谷，而你的名字成了一个带着苦味的故事：拥有力量却不懂节制，拥有目标却被眼前满足牵走。森林没有审判你，季节仍旧轮转；真正的惩罚，是你再也看不到下一次春花开放。";
    return events.getEndingText(id);
}

void applyResult(const ActionResult& action, GameContext& ctx,
                 ProgressSystem& progress, EventSystem& events,
                 UI::InteractiveGameUI& ui) {
    const int oldStage = ctx.world.getStage();
    progress.applyActionResult(action, ctx.world);
    if (!action.message.empty()) ui.appendLog(action.message);
    if (action.success && action.stageCompleted &&
        ctx.world.getStage() != oldStage && ctx.world.getStage() <= 6)
        ui.appendLog(events.getStageIntroduction(ctx.world.getStage()));
}

void tryStartPendingBattle(GameContext& ctx, CombatSystem& combat,
                           UI::InteractiveGameUI& ui) {
    if (combat.isInBattle()) return;
    const std::string enemyId = pendingEnemyId(ctx.world);
    if (enemyId.empty()) return;
    const ActionResult started = combat.startBattle(enemyId, ctx);
    if (!started.message.empty()) ui.appendLog(started.message);
}

std::vector<std::wstring> slotDescriptions(const SaveSlots& slots) {
    std::vector<std::wstring> result;
    for (const std::string& text : slots.descriptions())
        result.push_back(UI::fromUtf8(text));
    return result;
}

void updateProfile(GameContext& ctx, WorldState& profile,
                   CollectionSystem& collections) {
    collections.syncLegacyFlags(ctx.world);
    mergeCollectionFlags(ctx.world, profile);
    saveCollectionProfile("collection_profile.txt", profile);
}

void handleBattleCommand(const std::string& line, GameContext& ctx,
                         CombatSystem& combat, EventSystem& events,
                         ProgressSystem& progress, UI::InteractiveGameUI& ui,
                         InteractiveMap& map, SaveSlots& slots,
                         SaveManager& saveManager) {
    const std::vector<std::string> parts = words(trim(line));
    if (parts.empty()) return;
    std::string action = lowerAscii(parts.front());
    std::string target = parts.size() > 1 ? lowerAscii(parts[1]) : "";
    if (action == "1" || action == "2" || action == "3") {
        target = action;
        action = "banana";
    }
    if (action == "背包" || action == "inventory" || action == "bag") {
        ui.appendLog(showInventory(ctx.player));
        return;
    }
    if (action == "存档" || action == "save" || action == "p" ||
        action == "k") {
        const int slot = ui.showSlotMenu(L"战 斗 中 存 档",
                                          slotDescriptions(slots), true);
        if (slot > 0) {
            map.storePosition(ctx);
            ui.appendLog(slots.save(slot, ctx, saveManager)
                ? "战斗进度已保存到存档位" + std::to_string(slot) +
                      "；读档后可在当前位置重新挑战该敌人。"
                : "保存失败，请检查目录权限。");
        }
        return;
    }
    const ActionResult battle = combat.performBattleAction(action, target, ctx);
    applyResult(battle, ctx, progress, events, ui);
    if (!combat.isInBattle() && ctx.player.getHealth() > 0) {
        const ActionResult resumed = events.resumePendingEventAfterBattle(ctx);
        if (!resumed.message.empty())
            applyResult(resumed, ctx, progress, events, ui);
    }
}

enum class GameExit { Menu, Closed };

GameExit play(GameContext& ctx, UI::InteractiveGameUI& ui,
              SaveSlots& slots, SaveManager& saveManager,
              WorldState& profile, CollectionSystem& collections) {
    EventSystem events;
    events.initializeEvents();
    NPCSystem npcs;
    npcs.initializeNPCs();
    CombatSystem combat;
    combat.initializeEnemies();
    ProgressSystem progress;
    EndingSystem endings;
    InteractiveMap map;
    map.resetForRoom(ctx);

    ui.clearLog();
    ui.appendLog(events.getStageIntroduction(ctx.world.getStage()));
    ui.appendLog(roomArrival(ctx));
    ui.appendLog("用WASD行走；靠近彩色目标后按Enter或空格互动。");
    int announcedTurn = -1;

    while (true) {
        map.ensureCurrentRoom(ctx);
        updateProfile(ctx, profile, collections);
        if (!combat.isInBattle() && announcedTurn != ctx.world.getTurnCount()) {
            announcedTurn = ctx.world.getTurnCount();
            ui.appendLog("【第" + std::to_string(announcedTurn) + "回合·" +
                         seasonName(announcedTurn) +
                         "季】地图上的游荡敌人与青色“奇”地点已经刷新。每个季节持续6回合。");
        }
        const bool ended = ctx.player.getHealth() <= 0 ||
            ctx.world.hasFlag("flag_final_choice") ||
            ctx.world.hasFlag("flag_hidden_ending_together_forever") ||
            ctx.world.hasFlag("flag_bad_ending_forest_fire") ||
            ctx.world.hasFlag("flag_bad_ending_second_banana") ||
            ctx.world.hasFlag("flag_bad_ending_gluttony") ||
            ctx.world.hasFlag("flag_bad_ending_coward") ||
            ctx.world.hasFlag("flag_hidden_ending_earth_gift") ||
            ctx.world.hasFlag("flag_hidden_ending_spark") ||
            ctx.world.hasFlag("flag_normal_ending_not_hero");
        if (ended) {
            const std::string id = specialEndingId(ctx, endings);
            collections.unlockEnding(id, ctx.world);
            updateProfile(ctx, profile, collections);
            ui.showEndingCinematic(L"本 轮 结 局", UI::fromUtf8(endingText(id, events)));
            return GameExit::Menu;
        }

        if (!ui.render(ctx, map, combat, objective(ctx)))
            return GameExit::Closed;

        if (combat.isInBattle()) {
            const std::optional<std::string> command =
                ui.readTypedCommand(L"战斗指令 > ");
            if (command) handleBattleCommand(*command, ctx, combat, events,
                                              progress, ui, map, slots,
                                              saveManager);
            continue;
        }

        const UI::ExploreAction action = ui.readExploreAction();
        if (action == UI::ExploreAction::EndOfInput) return GameExit::Closed;
        if (action == UI::ExploreAction::None) continue;

        int dx = 0;
        int dy = 0;
        if (action == UI::ExploreAction::MoveUp) dy = -1;
        if (action == UI::ExploreAction::MoveDown) dy = 1;
        if (action == UI::ExploreAction::MoveLeft) dx = -1;
        if (action == UI::ExploreAction::MoveRight) dx = 1;
        if (dx != 0 || dy != 0) {
            const MapMoveResult moved = map.move(dx, dy, ctx);
            if (moved.roomChanged) {
                applyResult(moved.action, ctx, progress, events, ui);
                ui.appendLog(roomArrival(ctx));
            } else if (!moved.action.message.empty()) {
                ui.appendLog(moved.action.message);
            }
            continue;
        }

        if (action == UI::ExploreAction::Interact) {
            const MapInteraction interaction = map.interact(ctx);
            ActionResult outcome;
            switch (interaction.kind) {
            case InteractionKind::Npc:
                outcome = npcs.talkToNPC(interaction.id, ctx);
                break;
            case InteractionKind::Item:
                outcome = takeMapItem(interaction.id, ctx);
                break;
            case InteractionKind::Chest:
                outcome = openChest(interaction.id, ctx);
                break;
            case InteractionKind::EasterEgg:
                outcome = findEasterEgg(interaction.id, ctx);
                break;
            case InteractionKind::Enemy:
                outcome = combat.startBattle(interaction.id, ctx);
                break;
            case InteractionKind::Quest:
                if (ctx.world.getStage() == 6 &&
                    !hasPendingMainChoice(ctx.world) &&
                    !hasAnyFinalRoute(ctx)) {
                    ctx.world.setFlag("flag_final_choice");
                    const bool spark =
                        ctx.world.hasFlag("flag_child_rescued") &&
                        ctx.world.hasFlag("flag_doudou_blessing_triggered");
                    ctx.world.setFlag(spark ? "flag_hidden_ending_spark"
                                            : "flag_normal_ending_not_hero");
                    outcome = result(true,
                        "条件判定中……\n" +
                        std::string(spark ? "【条件判定成功】\n【结局达成】星火"
                                          : "【条件判定失败】\n【结局达成】绝大多数的现实"),
                        true, true);
                } else {
                    outcome = events.triggerAvailableMainEvent(ctx);
                }
                if (!outcome.success && outcome.message.empty())
                    outcome = result(false, "这个任务点暂时没有新的剧情。先查看右侧目标。");
                break;
            case InteractionKind::LockedDoor:
                outcome = result(false,
                    "【无法解锁】实验基地门禁仍是红色。需要先取得基地线索，推进至第四阶段并在猴王树平息猴群分歧。");
                break;
            case InteractionKind::LockedItem:
                outcome = result(false, "【暂时无法拾取】" + interaction.hint + "。");
                break;
            case InteractionKind::RandomEvent:
                outcome = resolveLocationEvent(interaction.id, ctx);
                break;
            default:
                outcome = result(false, "附近没有可互动目标。面向目标再按一次Enter。");
                break;
            }
            applyResult(outcome, ctx, progress, events, ui);
            tryStartPendingBattle(ctx, combat, ui);
            continue;
        }

        if (action == UI::ExploreAction::Choice1 ||
            action == UI::ExploreAction::Choice2 ||
            action == UI::ExploreAction::Choice3 ||
            action == UI::ExploreAction::Choice4) {
            const int option = action == UI::ExploreAction::Choice1 ? 1 :
                               action == UI::ExploreAction::Choice2 ? 2 :
                               action == UI::ExploreAction::Choice3 ? 3 : 4;
            ActionResult choice;
            if (ctx.world.hasFlag("flag_pending_scout_wander_choice"))
                choice = combat.chooseEscapeEndingOption(option, ctx);
            else {
                choice = npcs.chooseDialogueOption(option, ctx);
                if (!choice.success &&
                    choice.message.find("当前没有等待选择的对话") != std::string::npos)
                    choice = events.chooseEventOption("", option, ctx);
            }
            applyResult(choice, ctx, progress, events, ui);
            tryStartPendingBattle(ctx, combat, ui);
            continue;
        }

        if (action == UI::ExploreAction::Inventory) {
            ui.appendLog(showInventory(ctx.player));
            continue;
        }
        if (action == UI::ExploreAction::UseItem) {
            const std::optional<std::string> item =
                ui.readTypedCommand(L"物品名称 > ");
            if (item) applyResult(useItem(trim(*item), ctx), ctx, progress,
                                  events, ui);
            continue;
        }
        if (action == UI::ExploreAction::Help) {
            ui.showTextPage(L"操 作 帮 助", UI::fromUtf8(gameHelp()));
            continue;
        }
        if (action == UI::ExploreAction::Save) {
            const int slot = ui.showSlotMenu(L"选 择 存 档 位",
                                              slotDescriptions(slots), true);
            if (slot > 0) {
                map.storePosition(ctx);
                ui.appendLog(slots.save(slot, ctx, saveManager)
                    ? "已保存到存档位" + std::to_string(slot) + "。"
                    : "保存失败，请检查目录权限。");
            }
            continue;
        }
        if (action == UI::ExploreAction::Menu) {
            const int paused = ui.showPauseMenu();
            if (paused == 1) {
                const int slot = ui.showSlotMenu(L"选 择 存 档 位",
                                                  slotDescriptions(slots), true);
                if (slot > 0) {
                    map.storePosition(ctx);
                    ui.appendLog(slots.save(slot, ctx, saveManager)
                        ? "已保存到存档位" + std::to_string(slot) + "。"
                        : "保存失败，请检查目录权限。");
                }
            } else if (paused == 2) {
                return GameExit::Menu;
            }
        }
    }
}

}  // namespace

int main() {
#ifndef _WIN32
    return 1;
#else
    configureApplication();
    UI::ConsoleRenderer renderer;
    UI::InteractiveGameUI ui(renderer);
    SaveSlots slots;
    SaveManager saveManager;
    CollectionSystem collections;
    WorldState profile;
    loadCollectionProfile("collection_profile.txt", profile);

    while (true) {
        const int selected = ui.showMainMenu(slots.any());
        if (selected < 0 || selected == 4) break;
        if (selected == 2) {
            ui.showTextPage(L"结 局 收 集",
                UI::fromUtf8(collections.getEndingCollectionText(profile)));
            continue;
        }
        if (selected == 3) {
            ui.showTextPage(L"成 就 系 统",
                UI::fromUtf8(collections.getAchievementCollectionText(profile)));
            continue;
        }

        Player player;
        WorldState world;
        initializeNewWorld(world);
        std::map<std::string, Room> rooms = createAllRooms();
        GameContext ctx{player, world, rooms};

        if (selected == 1) {
            if (!slots.any()) {
                ui.showTextPage(L"继 续 游 戏", L"目前还没有存档。请先选择“新的开始”。");
                continue;
            }
            const int slot = ui.showSlotMenu(L"读 取 存 档",
                                              slotDescriptions(slots), false);
            if (slot == 0) continue;
            if (!slots.load(slot, ctx, saveManager)) {
                ui.showTextPage(L"读 取 失 败", L"这个存档无法读取，请换一个存档位。");
                continue;
            }
        }

        mergeCollectionFlags(profile, ctx.world);
        if (play(ctx, ui, slots, saveManager, profile, collections) ==
            GameExit::Closed) break;
    }
    renderer.restore();
    return 0;
#endif
}

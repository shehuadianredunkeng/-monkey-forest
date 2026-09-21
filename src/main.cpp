#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "CombatSystem.h"
#include "CollectionSystem.h"
#include "CommonUtils.h"
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

using namespace std;

namespace {

void configureApplication() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    wchar_t executable[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        error_code error;
        filesystem::current_path(
            filesystem::path(executable).parent_path(), error);
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

ActionResult result(bool success, string message,
                    bool turn = false, bool stage = false) {
    return {success, move(message), turn, stage};
}

bool consumesMapInteractionStamina(InteractionKind kind) {
    return kind == InteractionKind::Item || kind == InteractionKind::Chest ||
           kind == InteractionKind::Enemy || kind == InteractionKind::Quest ||
           kind == InteractionKind::EasterEgg ||
           kind == InteractionKind::RandomEvent;
}

ActionResult chargeMapInteractionStamina(InteractionKind kind,
                                         ActionResult action,
                                         GameContext& ctx) {
    if (!action.success || !consumesMapInteractionStamina(kind)) return action;
    ctx.player.changeStamina(-1);
    if (!action.message.empty()) action.message += "\n";
    action.message += "本次互动消耗 1 点体力。";
    return action;
}

string trim(const string& text) {
    size_t first = 0;
    while (first < text.size() &&
           isspace(static_cast<unsigned char>(text[first]))) ++first;
    size_t last = text.size();
    while (last > first &&
           isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

vector<string> words(const string& text) {
    istringstream input(text);
    vector<string> result;
    string word;
    while (input >> word) result.push_back(word);
    return result;
}

string lowerAscii(string text) {
    for (char& ch : text)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return text;
}

string pendingEnemyId(const WorldState& world) {
    if (world.hasFlag("flag_pending_battle_bees") &&
        !world.hasFlag("flag_bees_defeated")) return "enemy_bees";
    if (world.hasFlag("flag_pending_battle_robot") &&
        !world.hasFlag("flag_robot_defeated")) return "enemy_robot";
    if (world.hasFlag("flag_pending_battle_hertz") &&
        !world.hasFlag("flag_hertz_defeated")) return "enemy_hertz";
    return "";
}

bool hasPendingMainChoice(const WorldState& world) {
    for (const string& flag : world.getFlags())
        if (flag.rfind("flag_pending_event_", 0) == 0) return true;
    return false;
}

bool hasAllSeasonalRelics(const WorldState& world) {
    return world.hasFlag("flag_season_relic_spring") &&
           world.hasFlag("flag_season_relic_summer") &&
           world.hasFlag("flag_season_relic_autumn") &&
           world.hasFlag("flag_season_relic_winter");
}

string seasonName(int turn) {
    static const char* names[] = {"春", "夏", "秋", "冬"};
    return names[seasonIndex(turn)];
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

string objective(const GameContext& ctx) {
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

string roomArrival(const GameContext& ctx) {
    const auto found = ctx.rooms.find(ctx.player.getCurrentRoomId());
    return found == ctx.rooms.end()
        ? "进入未知区域。"
        : "【进入" + found->second.getName() + "】\n" +
              found->second.getBaseDescription();
}

ActionResult takeMapItem(const string& itemId, GameContext& ctx) {
    const string flag = "flag_taken_" + ctx.player.getCurrentRoomId() +
                             "_" + itemId;
    if (ctx.world.hasFlag(flag)) return result(false, "这里已经空了。");
    ActionResult picked = takeItem(itemId, ctx);
    if (picked.success) ctx.world.setFlag(flag);
    return picked;
}

ActionResult openChest(const string& chestId, GameContext& ctx) {
    const string flag = "flag_opened_" + chestId;
    if (ctx.world.hasFlag(flag)) return result(false, "宝箱已经打开过了。");

    Item reward("item_fruit", "果实", false, 1);
    string text = "果实";
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

ActionResult findEasterEgg(const string& eggId, GameContext& ctx) {
    const string flag = "flag_found_" + eggId;
    if (ctx.world.hasFlag(flag)) return result(false, "这里的秘密已经被发现了。");
    ctx.world.setFlag(flag);
    if (eggId.rfind("season_relic_", 0) == 0) {
        const string season = eggId.substr(string("season_relic_").size());
        string name = season == "spring" ? "春花" :
                           season == "summer" ? "蝉蜕" :
                           season == "autumn" ? "秋叶" : "落雪";
        ctx.world.setFlag("flag_season_relic_" + season);
        ctx.player.addItem(Item("item_" + season + "_token", name, true, 1));
        string message = "你小心收起黄色光芒中的【" + name + "】，获得一件四季信物。";
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
    ctx.player.changeHealth(20);
    ctx.player.changeStamina(20);
    return result(true,
                  "你按下测试香蕉，修复程序喷出一团急救泡沫。生命+20，体力+20。",
                  true);
}

ActionResult resolveLocationEvent(const string& eventId,
                                  GameContext& ctx) {
    const string flag = "flag_resolved_" + eventId;
    if (ctx.world.hasFlag(flag))
        return result(false, "这处异动已经恢复平静。等待下一回合会出现新的地点事件。");
    ctx.world.setFlag(flag);
    const int kind = ctx.world.getTurnCount() % 5;
    if (kind == 0) {
        ctx.player.changeStamina(10);
        return result(true, "你在树根下找到一处避风窝，短暂休息后体力+10。", true);
    }
    if (kind == 1) {
        ctx.player.changeReputation(2);
        return result(true, "风吹过石缝发出规律回声，你帮迷路的同伴重新辨明方向，声望+2。", true);
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

string battleHelp() {
    return "战斗仍使用短指令：\n"
           "攻击（attack）  防御（guard）  偷窃（steal）\n"
           "分析（analyze） 破解（hack） 逃跑（escape）\n"
           "使用 草药/蜂蜜 / use herb/honey；背包 / inventory；存档 / save\n"
           "战斗中按 P 直接打开存档菜单；其他指令输入后按回车。\n"
           "赫兹递出香蕉时，可直接输入 1、2 或 3。";
}

string gameHelp() {
    return "【探索操作】\n"
           "W/A/S/D：在房间内移动\n"
           "↑/↓：逐行翻看以往剧情；PgUp/PgDn：快速翻页\n"
           "Enter / 空格：与身边目标互动\n"
           "I：查看背包    U：输入名称使用物品\n"
           "P：选择存档位保存    Esc：暂停菜单\n\n"
           "【地图图例】\n"
           "猴=玩家  岩=岩背  叶=叶婆婆  闪=闪尾  豆=豆豆  赫=赫兹\n"
           "伴=同行的闪尾  物=物品  宝=宝箱  敌=战斗  ！=关键剧情\n"
           "青色“门”可切换房间，红色“锁”表示尚未满足通行条件，“奇”是本回合地点事件。\n"
           "剧情和NPC选项出现后，直接按1/2/3/4。\n\n" +
           battleHelp();
}

string specialEndingId(const GameContext& ctx,
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

void applyBattleResult(const ActionResult& action, GameContext& ctx,
                       ProgressSystem& progress, EventSystem& events,
                       UI::InteractiveGameUI& ui) {
    ActionResult display = action;
    // 坏结局或玩家倒下后会直接进入结局页，不再插入一次地图刷新。
    if (display.stageCompleted || ctx.player.getHealth() <= 0)
        display.turnConsumed = false;
    applyResult(display, ctx, progress, events, ui);
}

void tryStartPendingBattle(GameContext& ctx, CombatSystem& combat,
                           UI::InteractiveGameUI& ui) {
    if (combat.isInBattle()) return;
    const string enemyId = pendingEnemyId(ctx.world);
    if (enemyId.empty()) return;
    const ActionResult started = combat.startBattle(enemyId, ctx);
    if (!started.message.empty()) ui.appendLog(started.message);
}

vector<wstring> slotDescriptions(const SaveSlots& slots,
                                           const GameContext& ctx) {
    vector<wstring> result;
    for (const string& text : slots.descriptions(ctx.rooms))
        result.push_back(UI::fromUtf8(text));
    return result;
}

void updateProfile(GameContext& ctx, WorldState& profile,
                   CollectionSystem& collections) {
    collections.syncLegacyFlags(ctx.world);
    mergeCollectionFlags(ctx.world, profile);
    saveCollectionProfile("collection_profile.txt", profile);
}

void handleBattleCommand(const string& line, GameContext& ctx,
                         CombatSystem& combat, EventSystem& events,
                         ProgressSystem& progress, UI::InteractiveGameUI& ui,
                         InteractiveMap& map, SaveSlots& slots,
                         SaveManager& saveManager) {
    const vector<string> parts = words(trim(line));
    if (parts.empty()) return;
    string action = lowerAscii(parts.front());
    string target = parts.size() > 1 ? lowerAscii(parts[1]) : "";
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
                                          slotDescriptions(slots, ctx), true);
        if (slot > 0) {
            map.storePosition(ctx);
            combat.saveBattleState(ctx.world);
            ui.appendLog(slots.save(slot, ctx, saveManager)
                ? "战斗进度已保存到存档位" + to_string(slot) +
                      "；读档后将恢复双方当前生命和战斗状态。"
                : "保存失败，请检查目录权限。");
        }
        return;
    }
    ActionResult battle = combat.performBattleAction(action, target, ctx);
    const bool endedBattle = !combat.isInBattle();
    if (!endedBattle) battle.turnConsumed = false;
    else if (battle.success && !battle.stageCompleted && ctx.player.getHealth() > 0)
        battle.turnConsumed = true;
    applyBattleResult(battle, ctx, progress, events, ui);
    if (endedBattle && ctx.player.getHealth() > 0) {
        ActionResult resumed = events.resumePendingEventAfterBattle(ctx);
        resumed.turnConsumed = false;
        if (!resumed.message.empty())
            applyResult(resumed, ctx, progress, events, ui);
    }
}

enum class GameExit { Menu, Ending, Closed };

GameExit play(GameContext& ctx, UI::InteractiveGameUI& ui,
              SaveSlots& slots, SaveManager& saveManager,
              WorldState& profile, CollectionSystem& collections) {
    EventSystem events;
    events.initializeEvents();
    NPCSystem npcs;
    npcs.initializeNPCs();
    CombatSystem combat;
    ProgressSystem progress;
    EndingSystem endings;
    InteractiveMap map;
    map.resetForRoom(ctx);
    // 读档时可能停在一场尚未结束的战斗里。
    const bool resumedBattle = combat.restoreBattleState(ctx);

    ui.clearLog();
    ui.appendLog(events.getStageIntroduction(ctx.world.getStage()));
    ui.appendLog(roomArrival(ctx));
    ui.appendLog("用WASD行走；靠近彩色目标后按Enter或空格互动。");
    if (resumedBattle)
        ui.appendLog("【战斗读档成功】已恢复你和敌人存档时的生命与战斗进度。");
    int announcedSeason = -1;

    while (true) {
        map.ensureCurrentRoom(ctx);
        updateProfile(ctx, profile, collections);
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
            const string id = specialEndingId(ctx, endings);
            collections.unlockEnding(id, ctx.world);
            updateProfile(ctx, profile, collections);
            ui.showEndingCinematic(L"本 轮 结 局",
                                   UI::fromUtf8(events.getEndingText(id)));
            return GameExit::Ending;
        }
        const int turn = ctx.world.getTurnCount();
        if (!combat.isInBattle() && announcedSeason != turn / 6) {
            announcedSeason = turn / 6;
            ui.appendLog("【第" + to_string(turn) + "回合·" + seasonName(turn) +
                         "季】地图上的游荡敌人与青色“奇”地点已经刷新。");
        }

        if (!ui.render(ctx, map, combat, objective(ctx)))
            return GameExit::Closed;

        if (combat.isInBattle()) {
            const optional<string> command =
                ui.readTypedCommand(L"战斗指令（P 存档） > ", true);
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
        // 方向键先换算成地图位移，其他按键在后面分别处理。
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
            if (consumesMapInteractionStamina(interaction.kind) &&
                ctx.player.getStamina() <= 0) {
                outcome = result(false,
                    "体力不足：非NPC互动需要 1 点体力。请先使用果实、草药或寻找休息机会。",
                    false, false);
                applyResult(outcome, ctx, progress, events, ui);
                continue;
            }
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
                    ctx.world.hasFlag("flag_escape_normal_endings_locked")) {
                    ctx.world.setFlag("flag_final_choice");
                    ctx.world.setFlag("flag_bad_ending_coward");
                    outcome = result(true,
                        "你来到最终抉择，却发现曾经逃避的五场战斗已经关闭了三条常规路线。\n"
                        "【条件判定】累计逃跑≥5次\n"
                        "【结局达成】你是狗熊",
                        true, true);
                } else if (ctx.world.getStage() == 6 &&
                    !hasPendingMainChoice(ctx.world) &&
                    hasAllSeasonalRelics(ctx.world)) {
                    ctx.world.setFlag("flag_final_choice");
                    ctx.world.setFlag("flag_hidden_ending_earth_gift");
                    outcome = result(true,
                        "四季信物在猴王树根下同时亮起。\n"
                        "【条件判定成功】\n【结局达成】地球的礼物",
                        true, true);
                } else if (ctx.world.getStage() == 6 &&
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
                        string(spark ? "【条件判定成功】\n【结局达成】星火"
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
            outcome = chargeMapInteractionStamina(interaction.kind,
                                                  move(outcome), ctx);
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
                    choice.message.find("当前没有等待选择的对话") != string::npos)
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
            const optional<string> item =
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
                                              slotDescriptions(slots, ctx), true);
            if (slot > 0) {
                map.storePosition(ctx);
                combat.clearSavedBattleState(ctx.world);
                ui.appendLog(slots.save(slot, ctx, saveManager)
                    ? "已保存到存档位" + to_string(slot) + "。"
                    : "保存失败，请检查目录权限。");
            }
            continue;
        }
        if (action == UI::ExploreAction::Menu) {
            const int paused = ui.showPauseMenu();
            if (paused == 1) {
                const int slot = ui.showSlotMenu(L"选 择 存 档 位",
                                                  slotDescriptions(slots, ctx), true);
                if (slot > 0) {
                    map.storePosition(ctx);
                    combat.clearSavedBattleState(ctx.world);
                    ui.appendLog(slots.save(slot, ctx, saveManager)
                        ? "已保存到存档位" + to_string(slot) + "。"
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

    bool preferContinue = false;
    while (true) {
        const int selected = ui.showMainMenu(slots.any(), preferContinue);
        preferContinue = false;
        if (selected < 0 || selected == 4) break;
        if (selected == 2) {
            ui.showCollectionPage(
                L"结 局 收 集",
                UI::fromUtf8(collections.getEndingCollectionText(profile, false)),
                UI::fromUtf8(collections.getEndingCollectionText(profile, true)));
            continue;
        }
        if (selected == 3) {
            ui.showCollectionPage(
                L"成 就 系 统",
                UI::fromUtf8(collections.getAchievementCollectionText(profile, false)),
                UI::fromUtf8(collections.getAchievementCollectionText(profile, true)));
            continue;
        }

        Player player;
        WorldState world;
        initializeNewWorld(world);
        map<string, Room> rooms = createAllRooms();
        GameContext ctx{player, world, rooms};

        if (selected == 1) {
            if (!slots.any()) {
                ui.showTextPage(L"继 续 游 戏", L"目前还没有存档。请先选择“新的开始”。");
                continue;
            }
            const int slot = ui.showSlotMenu(L"读 取 存 档",
                                              slotDescriptions(slots, ctx), false,
                                              slots.mostRecentSlot());
            if (slot == 0) continue;
            if (!slots.load(slot, ctx, saveManager)) {
                ui.showTextPage(L"读 取 失 败", L"这个存档无法读取，请换一个存档位。");
                continue;
            }
        }

        if (selected == 0) {
            ui.showTextPage(L"新 手 指 引",
                L"你将扮演一只吗喽，探索森林，和伙伴一起完成六阶段主线。\n\n"
                L"1. 按 W/A/S/D 移动，靠近人物或目标后按 Enter/空格互动。\n"
                L"2. 先看看地图上的“！”，它代表当前的关键剧情。\n"
                L"3. 岩、叶、闪、豆、赫分别是岩背、叶婆婆、闪尾、豆豆、赫兹。\n"
                L"4. 出现选项时按 1/2/3/4；走到“门”附近可前往其他房间。\n"
                L"5. I 查看背包，U 输入物品名称；H 随时查看操作帮助。\n"
                L"6. 探索或战斗时按 P，选择存档位并回车保存，不消耗战斗回合。\n"
                L"7. 战斗指令也要按回车确认，例如 attack（攻击）、guard（防御）。\n"
                L"8. 下次在主菜单选“继续游戏”，读取同一存档位继续。\n\n"
                L"按回车开始吧！");
        }
        mergeCollectionFlags(profile, ctx.world);
        const GameExit exit = play(ctx, ui, slots, saveManager, profile,
                                   collections);
        if (exit == GameExit::Closed) break;
        if (exit == GameExit::Ending && slots.any()) preferContinue = true;
    }
    renderer.restore();
    return 0;
#endif
}

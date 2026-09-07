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

std::string battleHelp() {
    return "战斗仍使用短指令：\n"
           "攻击（attack）  防御（guard）  偷窃（steal）\n"
           "分析（analyze） 破解（hack） 逃跑（escape）\n"
           "使用 草药 / use herb；背包 / inventory\n"
           "赫兹递出香蕉时，可直接输入 1、2 或 3。";
}

std::string gameHelp() {
    return "【探索操作】\n"
           "W/A/S/D 或方向键：在房间内移动\n"
           "Enter / 空格：与身边目标互动\n"
           "I：查看背包    U：输入名称使用物品\n"
           "P：选择存档位保存    Esc：暂停菜单\n\n"
           "【地图图例】\n"
           "猴=玩家  友=NPC  物=物品  宝=宝箱  敌=战斗  ！=关键剧情\n"
           "走到青色的“门”会切换房间。剧情和NPC选项出现后，直接按1/2/3。\n\n" +
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
    if (ctx.player.getHealth() <= 0) return "ending_fail";
    return endings.determineEndingId(ctx);
}

std::string endingText(const std::string& id, const EventSystem& events) {
    if (id == "ending_together_forever")
        return "【隐藏结局：双宿双飞】\n你与闪尾抓住同一根藤蔓，越过河谷，也越过了青木谷的边界。";
    if (id == "ending_forest_fire")
        return "【隐藏结局：放火烧山】\n火光照亮了整片森林，也照亮了你来不及后悔的脸。";
    if (id == "ending_second_banana")
        return "【坏结局：有了第一次就有第二次！】\n你一次次伸手，最终再也没能从香蕉诱惑里醒来。";
    if (id == "ending_gluttony")
        return "【坏结局：你犯下了暴食罪！】\n赫兹放下最后一根香蕉，抽取塔继续轰鸣。";
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
                         ProgressSystem& progress, UI::InteractiveGameUI& ui) {
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

    while (true) {
        map.ensureCurrentRoom(ctx);
        updateProfile(ctx, profile, collections);
        const bool ended = ctx.player.getHealth() <= 0 ||
            ctx.world.hasFlag("flag_final_choice") ||
            ctx.world.hasFlag("flag_hidden_ending_together_forever") ||
            ctx.world.hasFlag("flag_bad_ending_forest_fire") ||
            ctx.world.hasFlag("flag_bad_ending_second_banana") ||
            ctx.world.hasFlag("flag_bad_ending_gluttony");
        if (ended) {
            const std::string id = specialEndingId(ctx, endings);
            collections.unlockEnding(id, ctx.world);
            updateProfile(ctx, profile, collections);
            ui.showTextPage(L"本 轮 结 局", UI::fromUtf8(endingText(id, events)));
            return GameExit::Menu;
        }

        if (!ui.render(ctx, map, combat, objective(ctx)))
            return GameExit::Closed;

        if (combat.isInBattle()) {
            const std::optional<std::string> command =
                ui.readTypedCommand(L"战斗指令 > ");
            if (command) handleBattleCommand(*command, ctx, combat, events,
                                              progress, ui);
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
                outcome = events.triggerAvailableMainEvent(ctx);
                if (!outcome.success && outcome.message.empty())
                    outcome = result(false, "这个任务点暂时没有新的剧情。先查看右侧目标。");
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
            action == UI::ExploreAction::Choice3) {
            const int option = action == UI::ExploreAction::Choice1 ? 1 :
                               action == UI::ExploreAction::Choice2 ? 2 : 3;
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

#include "CombatSystem.h"
#include "CollectionSystem.h"
#include "ConsoleUI.h"
#include "EndingSystem.h"
#include "EventSystem.h"
#include "NPCSystem.h"
#include "Player.h"
#include "PlayerActions.h"
#include "ProgressSystem.h"
#include "Room.h"
#include "SaveManager.h"
#include "StatusView.h"
#include "WorldState.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
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

constexpr const char* kDefaultSavePath = "monkey_forest_save.txt";

void configureConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::string trim(const std::string& text) {
    std::size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }

    std::size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }

    return text.substr(first, last - first);
}

std::string lowerAscii(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

std::vector<std::string> splitWords(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> words;
    std::string word;
    while (input >> word) {
        words.push_back(word);
    }
    return words;
}

ActionResult makeLocalResult(bool success,
                             const std::string& message,
                             bool turnConsumed = false,
                             bool stageCompleted = false) {
    return {success, message, turnConsumed, stageCompleted};
}

bool parseInt(const std::string& text, int& value) {
    try {
        std::size_t consumed = 0;
        value = std::stoi(text, &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

void initializeNewWorld(WorldState& world) {
    world.setStage(1);
    world.setTurnCount(0);
    world.setResource(ResourceType::Food, 0);
    world.setResource(ResourceType::Water, 0);
    world.setResource(ResourceType::Morale, 50);
    world.setResource(ResourceType::MigrationSupply, 0);
}

std::string normalizeDirection(const std::string& direction) {
    const std::string word = lowerAscii(direction);
    if (word == "e" || word == "东") return "east";
    if (word == "w" || word == "西") return "west";
    if (word == "n" || word == "北") return "north";
    if (word == "s" || word == "南") return "south";
    if (word == "u" || word == "上") return "up";
    return word;
}

bool parseSkillType(const std::string& word, SkillType& skill) {
    const std::string key = lowerAscii(word);
    if (key == "gather" || key == "采集") {
        skill = SkillType::Gather;
        return true;
    }
    if (key == "climb" || key == "攀爬") {
        skill = SkillType::Climb;
        return true;
    }
    if (key == "combat" || key == "战斗") {
        skill = SkillType::Combat;
        return true;
    }
    if (key == "leadership" || key == "lead" || key == "领导") {
        skill = SkillType::Leadership;
        return true;
    }
    return false;
}

bool containsId(const std::vector<std::string>& ids, const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

const Room* currentRoom(const GameContext& ctx) {
    const auto room = ctx.rooms.find(ctx.player.getCurrentRoomId());
    return room == ctx.rooms.end() ? nullptr : &room->second;
}

bool npcIsHere(const GameContext& ctx, const std::string& npcId) {
    const Room* room = currentRoom(ctx);
    if (room == nullptr) return false;
    std::string canonical = npcId;
    if (npcId == "岩背" || npcId == "猴王" || npcId == "king") canonical = "npc_king";
    if (npcId == "闪尾" || npcId == "scout") canonical = "npc_scout";
    if (npcId == "叶婆婆" || npcId == "healer") canonical = "npc_healer";
    if (npcId == "豆豆" || npcId == "child") canonical = "npc_child";
    if (npcId == "赫兹" || npcId == "hertz") canonical = "npc_hertz";
    return containsId(room->getVisibleNPCIds(ctx), canonical);
}

std::string roomName(const GameContext& ctx, const std::string& roomId) {
    const auto room = ctx.rooms.find(roomId);
    return room == ctx.rooms.end() ? roomId : room->second.getName();
}

std::string requiredRoomForEvent(const std::string& eventId) {
    static const std::map<std::string, std::string> rooms = {
        {"event_tree_trial", "room_forest"},
        {"event_winter_shortage", "room_tree"},
        {"event_glowing_river", "room_river"},
        {"event_echo_tracking", "room_cave"},
        {"event_drought_choice", "room_river"},
        {"event_group_dispute", "room_tree"},
        {"event_base_infiltration", "room_base"},
        {"event_final_choice", "room_tree"},
    };

    const auto room = rooms.find(eventId);
    return room == rooms.end() ? "" : room->second;
}

std::string recommendedMainEventId(const GameContext& ctx) {
    const WorldState& world = ctx.world;

    if (world.hasFlag("flag_final_choice")) {
        return "";
    }

    switch (world.getStage()) {
    case 1:
        return "event_tree_trial";
    case 2:
        return "event_winter_shortage";
    case 3:
        return world.hasFlag("flag_event_glowing_river_done")
                   ? "event_echo_tracking"
                   : "event_glowing_river";
    case 4:
        return world.hasFlag("flag_event_drought_choice_done")
                   ? "event_group_dispute"
                   : "event_drought_choice";
    case 5:
        return "event_base_infiltration";
    case 6:
        return "event_final_choice";
    default:
        return "";
    }
}

std::string buildGuideText(const GameContext& ctx) {
    const std::string eventId = recommendedMainEventId(ctx);
    if (eventId.empty()) {
        return "主线已经完成。输入 ending 查看结局，或 quit 退出游戏。";
    }

    static const std::map<std::string, std::string> titles = {
        {"event_tree_trial", "树冠试炼"},
        {"event_winter_shortage", "寒冬缺粮"},
        {"event_glowing_river", "发光河水"},
        {"event_echo_tracking", "回声追踪"},
        {"event_drought_choice", "干旱抉择"},
        {"event_group_dispute", "猴群分歧"},
        {"event_base_infiltration", "潜入实验基地"},
        {"event_final_choice", "家园抉择"}
    };
    const std::string targetRoom = requiredRoomForEvent(eventId);
    std::ostringstream output;
    const auto title = titles.find(eventId);
    output << "当前主线：" << (title == titles.end() ? eventId : title->second) << "\n";
    if (!targetRoom.empty()) {
        output << "目标地点：" << roomName(ctx, targetRoom) << "（"
               << targetRoom << "）\n";
        if (ctx.player.getCurrentRoomId() != targetRoom) {
            output << "先移动到目标地点，再输入 investigate。";
        } else {
            output << "你已经在目标地点，输入 investigate 触发主线事件。";
        }
    } else {
        output << "输入 investigate 继续主线。";
    }
    return output.str();
}

std::string pendingEnemyId(const WorldState& world) {
    if (world.hasFlag("flag_pending_battle_bees") &&
        !world.hasFlag("flag_bees_defeated")) {
        return "enemy_bees";
    }
    if (world.hasFlag("flag_pending_battle_robot") &&
        !world.hasFlag("flag_robot_defeated")) {
        return "enemy_robot";
    }
    if (world.hasFlag("flag_pending_battle_hertz") &&
        !world.hasFlag("flag_hertz_defeated")) {
        return "enemy_hertz";
    }
    return "";
}

bool isPendingEnemy(const WorldState& world, const std::string& enemyId) {
    return pendingEnemyId(world) == enemyId;
}

void printHelp() {
    std::cout
        << "===== 命令表 =====\n"
        << "look                         查看当前场景\n"
        << "guide                        查看下一步主线建议\n"
        << "go east/west/north/south/up  移动，也可用 go 东/西/南/北/上\n"
        << "investigate                  触发当前阶段主线事件\n"
        << "random                       尝试触发随机事件\n"
        << "1 / 2 / 3                    直接选择当前选项\n"
        << "talk 中文名/英文名            对话、接取并自动提交NPC任务\n"
        << "take / take 物品名           拾取全部物品或指定物品\n"
        << "use item_id                  使用物品\n"
        << "train combat/climb/gather/leadership 训练技能\n"
        << "rest                         休息恢复体力\n"
        << "fight                        进入当前剧情战斗\n"
        << "attack / guard / analyze / hack / escape  战斗指令\n"
        << "inventory                    查看背包\n"
        << "status                       查看状态\n"
        << "save / load                  存档或读档\n"
        << "map                          查看地图\n"
        << "endings / achievements       查看结局或成就收集\n"
        << "quit                         退出游戏\n";
}

void applyAndPrint(const ActionResult& result,
                   GameContext& ctx,
                   ProgressSystem& progress,
                   EventSystem& events) {
    if (!result.message.empty()) {
        std::cout << result.message << "\n";
    }

    const int oldStage = ctx.world.getStage();
    progress.applyActionResult(result, ctx.world);

    if (result.success && result.stageCompleted &&
        ctx.world.getStage() != oldStage) {
        std::cout << "\n" << events.getStageIntroduction(ctx.world.getStage())
                  << "\n";
    }
}

void tryStartPendingBattle(GameContext& ctx, CombatSystem& combat) {
    if (combat.isInBattle()) {
        return;
    }

    const std::string enemyId = pendingEnemyId(ctx.world);
    if (enemyId.empty()) {
        return;
    }

    const ActionResult result = combat.startBattle(enemyId, ctx);
    if (result.success) {
        std::cout << "\n" << result.message << "\n";
    }
}

ActionResult takeItemOnce(const std::string& itemId, GameContext& ctx) {
    std::string canonicalId = itemId;
    if (itemId == "fruit" || itemId == "果实") canonicalId = "item_fruit";
    if (itemId == "herb" || itemId == "草药") canonicalId = "item_herb";
    if (itemId == "rope" || itemId == "藤索" || itemId == "藤蔓") canonicalId = "item_rope";
    if (itemId == "flint" || itemId == "燧石") canonicalId = "item_flint";
    if (itemId == "chip" || itemId == "晶片") canonicalId = "item_chip";
    const std::string flag = "flag_taken_" + ctx.player.getCurrentRoomId() +
                             "_" + canonicalId;
    if (ctx.world.hasFlag(flag)) {
        return makeLocalResult(false, "这个位置的该物品已经被拿走。");
    }

    ActionResult result = takeItem(canonicalId, ctx);
    if (result.success) {
        ctx.world.setFlag(flag);
    }
    return result;
}

ActionResult takeAllHere(GameContext& ctx) {
    const Room* room = currentRoom(ctx);
    if (room == nullptr) return makeLocalResult(false, "当前位置不存在。");
    const std::vector<std::string> items = room->getVisibleItemIds(ctx);
    if (items.empty()) return makeLocalResult(false, "这里没有可以拾取的物品。");
    bool success = false;
    std::ostringstream message;
    for (const std::string& item : items) {
        const ActionResult picked = takeItemOnce(item, ctx);
        if (!picked.message.empty()) message << picked.message << '\n';
        success = success || picked.success;
    }
    return makeLocalResult(success, message.str(), success);
}

void printEnding(const GameContext& ctx, const EndingSystem& endings,
                 const EventSystem& events) {
    if (ctx.player.getHealth() > 0 && !ctx.world.hasFlag("flag_final_choice")) {
        std::cout << "游戏尚未结束。继续推进主线，最终选择后会显示结局。\n";
        return;
    }

    const std::string endingId = ctx.player.getHealth() <= 0
                                     ? "ending_fail"
                                     : endings.determineEndingId(ctx);
    std::cout << "\n" << events.getEndingText(endingId) << "\n";
}

bool commandIs(const std::string& command,
               const std::string& english,
               const std::string& chinese = "") {
    return command == english || (!chinese.empty() && command == chinese);
}

}  // namespace

int main() {
#ifndef _WIN32
    std::cerr << "此坐标界面需要 Windows 控制台；非 Windows 环境可运行 UI 单元测试。\n";
    return 1;
#endif
    configureConsole();

    ConsoleUI ui;
    std::ostringstream capturedOutput;
    std::streambuf* originalOutput = std::cout.rdbuf(capturedOutput.rdbuf());

    Player player;
    WorldState world;
    initializeNewWorld(world);
    std::map<std::string, Room> rooms = createAllRooms();
    GameContext ctx{player, world, rooms};

    EventSystem events;
    events.initializeEvents();
    NPCSystem npcs;
    npcs.initializeNPCs();
    CombatSystem combat;
    combat.initializeEnemies();
    ProgressSystem progress;
    SaveManager saveManager;
    EndingSystem endings;
    CollectionSystem collections;

    auto flushOutputToUI = [&]() {
        ui.appendLog(capturedOutput.str());
        capturedOutput.str("");
        capturedOutput.clear();
    };

    std::cout << "===== 吗喽森林：家园守卫战 =====\n";
    std::cout << events.getStageIntroduction(world.getStage()) << "\n\n";
    std::cout << lookAround(ctx) << "\n\n";
    std::cout << "输入 help 查看命令，输入 guide 查看下一步建议。\n";

    std::string line;
    while (true) {
        flushOutputToUI();
        if (ctx.world.getStage() > 1) {
            std::cout << "【第一年试玩完成】\n"
                      << "你完成了树冠试炼，正式获得独自探索青木谷的资格。\n"
                      << "第二年的寒冬与星猿异动将在完整版继续。\n"
                      << "按 Enter 结束试玩。\n";
            flushOutputToUI();
            ui.render(ctx, combat, "第一年主线已完成：树冠试炼");
            ui.readCommand();
            break;
        }
        if (ctx.player.getHealth() <= 0 || ctx.world.hasFlag("flag_final_choice")) {
            printEnding(ctx, endings, events);
            flushOutputToUI();
            ui.render(ctx, combat, "本轮游戏已经结束");
            ui.readCommand();
            break;
        }

        ui.render(ctx, combat, buildGuideText(ctx));
        line = ui.readCommand();
        if (ui.inputClosed()) {
            break;
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> words = splitWords(line);
        const std::string command = lowerAscii(words.front());

        int bareOption = 0;
        if (words.size() == 1 && parseInt(command, bareOption)) {
            ActionResult choice;
            if (ctx.world.hasFlag("flag_pending_scout_wander_choice")) {
                choice = combat.chooseEscapeEndingOption(bareOption, ctx);
            } else if (combat.isInBattle() &&
                       (combat.getBattleState().awaitingBananaChoice ||
                        combat.getBattleState().bananaGreedLoop)) {
                choice = combat.performBattleAction("banana", command, ctx);
            } else {
                choice = npcs.chooseDialogueOption(bareOption, ctx);
                if (!choice.success &&
                    choice.message.find("当前没有等待选择的对话") != std::string::npos) {
                    choice = events.chooseEventOption("", bareOption, ctx);
                }
            }
            applyAndPrint(choice, ctx, progress, events);
            tryStartPendingBattle(ctx, combat);
            continue;
        }

        if (commandIs(command, "help", "帮助") || command == "?") {
            printHelp();
            continue;
        }

        if (commandIs(command, "quit", "退出") || command == "exit") {
            std::cout << "游戏已退出。记得用 save 保存进度。\n";
            flushOutputToUI();
            ui.render(ctx, combat, buildGuideText(ctx));
            break;
        }

        if (commandIs(command, "status", "状态")) {
            std::cout << buildStatusText(ctx);
            continue;
        }

        if (commandIs(command, "inventory", "背包") || command == "bag") {
            std::cout << showInventory(ctx.player) << "\n";
            continue;
        }

        if (commandIs(command, "guide", "指引")) {
            std::cout << buildGuideText(ctx) << "\n";
            continue;
        }

        if (commandIs(command, "map", "地图")) {
            std::cout << showMap(ctx) << "\n";
            continue;
        }

        if (command == "endings" || command == "结局收集") {
            collections.syncLegacyFlags(ctx.world);
            std::cout << collections.getEndingCollectionText(ctx.world);
            continue;
        }

        if (command == "achievements" || command == "成就" ||
            command == "成就收集") {
            collections.syncLegacyFlags(ctx.world);
            std::cout << collections.getAchievementCollectionText(ctx.world);
            continue;
        }

        if (commandIs(command, "save", "存档")) {
            if (combat.isInBattle()) {
                std::cout << "战斗中暂时不能存档，请先结束战斗。\n";
                continue;
            }
            const std::string path =
                words.size() >= 2 ? words[1] : kDefaultSavePath;
            std::cout << (saveManager.saveGame(path, ctx)
                              ? "存档成功：" + path
                              : "存档失败，请检查路径是否可写。")
                      << "\n";
            continue;
        }

        if (commandIs(command, "load", "读档")) {
            if (combat.isInBattle()) {
                std::cout << "战斗中暂时不能读档，请先结束战斗。\n";
                continue;
            }
            const std::string path =
                words.size() >= 2 ? words[1] : kDefaultSavePath;
            if (saveManager.loadGame(path, ctx)) {
                collections.syncLegacyFlags(ctx.world);
                std::cout << "读档成功：" << path << "\n";
                std::cout << events.getStageIntroduction(ctx.world.getStage())
                          << "\n";
                std::cout << lookAround(ctx) << "\n";
                tryStartPendingBattle(ctx, combat);
            } else {
                std::cout << "读档失败，没有找到存档或存档内容不正确。\n";
            }
            continue;
        }

        if (commandIs(command, "ending", "结局") || command == "end") {
            printEnding(ctx, endings, events);
            continue;
        }

        if (combat.isInBattle()) {
            if (command == "attack" || command == "攻击" ||
                command == "guard" || command == "防御" ||
                command == "steal" || command == "偷窃" || command == "偷" ||
                command == "analyze" || command == "分析" ||
                command == "hack" || command == "破解" ||
                command == "escape" || command == "逃跑" ||
                command == "撤退" || command == "use" ||
                command == "使用") {
                const std::string target = words.size() >= 2 ? words[1] : "";
                const ActionResult result =
                    combat.performBattleAction(command, target, ctx);
                applyAndPrint(result, ctx, progress, events);
                if (!combat.isInBattle() && ctx.player.getHealth() > 0) {
                    std::cout << "战斗结束后，如果剧情要求，请再次输入 choose 完成事件。\n";
                }
            } else {
                std::cout << "当前正在战斗，请使用攻击、防御、偷窃、分析、破解、使用或逃跑。\n";
            }
            continue;
        }

        ActionResult result;
        bool hasActionResult = true;
        bool moved = false;

        if (commandIs(command, "look", "查看") || command == "l") {
            std::cout << lookAround(ctx) << "\n";
            hasActionResult = false;
        } else if (commandIs(command, "go", "移动") || command == "走") {
            if (words.size() < 2) {
                result = makeLocalResult(false, "请输入移动方向，例如 go east。");
            } else {
                result = movePlayer(ctx, normalizeDirection(words[1]));
                moved = result.success;
            }
        } else if (commandIs(command, "take", "拾取")) {
            if (words.size() < 2) {
                result = takeAllHere(ctx);
            } else {
                result = takeItemOnce(words[1], ctx);
            }
        } else if (commandIs(command, "use", "使用")) {
            if (words.size() < 2) {
                result = makeLocalResult(false, "请输入要使用的物品ID。");
            } else {
                result = useItem(words[1], ctx);
            }
        } else if (commandIs(command, "rest", "休息")) {
            result = rest(ctx);
        } else if (commandIs(command, "train", "训练")) {
            if (words.size() < 2) {
                result = makeLocalResult(
                    false,
                    "请输入训练类型：gather、climb、combat、leadership。");
            } else {
                SkillType skill = SkillType::Gather;
                if (!parseSkillType(words[1], skill)) {
                    result = makeLocalResult(false, "无法识别这个技能。");
                } else {
                    result = trainSkill(skill, ctx);
                }
            }
        } else if (commandIs(command, "talk", "对话")) {
            if (words.size() < 2) {
                const Room* room = currentRoom(ctx);
                const std::vector<std::string> visible =
                    room == nullptr ? std::vector<std::string>{}
                                    : room->getVisibleNPCIds(ctx);
                if (visible.size() == 1) {
                    result = npcs.talkToNPC(visible.front(), ctx);
                } else {
                    result = makeLocalResult(
                        false, visible.empty()
                                   ? "当前场景没有可以交谈的角色。"
                                   : "这里有多名角色，请输入 talk 名字。");
                }
            } else if (!npcIsHere(ctx, words[1])) {
                result = makeLocalResult(false, "当前房间没有这个 NPC。");
            } else {
                result = npcs.talkToNPC(words[1], ctx);
            }
        } else if (commandIs(command, "investigate", "调查")) {
            const std::string eventId =
                words.size() >= 2 ? words[1] : recommendedMainEventId(ctx);
            if (eventId.empty()) {
                result = makeLocalResult(false, "当前没有可调查的主线事件。");
            } else {
                result = events.triggerEvent(eventId, ctx);
            }
        } else if (commandIs(command, "random", "随机")) {
            result = events.triggerEvent("random", ctx);
        } else if (commandIs(command, "choose", "选择")) {
            if (words.size() < 2) {
                result = makeLocalResult(false, "请输入选项编号，例如 choose 1。");
            } else {
                int option = 0;
                std::string eventId;
                if (parseInt(words[1], option)) {
                    eventId = "";
                } else if (words.size() >= 3 && parseInt(words[2], option)) {
                    eventId = words[1];
                } else {
                    result = makeLocalResult(false, "选项格式应为 choose 1 或 choose event_id 1。");
                }

                if (option > 0) {
                    result = events.chooseEventOption(eventId, option, ctx);
                }
            }
        } else if (commandIs(command, "fight", "战斗")) {
            const std::string enemyId =
                words.size() >= 2 ? words[1] : pendingEnemyId(ctx.world);
            if (enemyId.empty()) {
                result = makeLocalResult(false, "当前没有必须进入的剧情战斗。");
            } else if (!isPendingEnemy(ctx.world, enemyId)) {
                result = makeLocalResult(false, "这个敌人还没有通过剧情触发。");
            } else {
                result = combat.startBattle(enemyId, ctx);
            }
        } else {
            result = makeLocalResult(false, "无法识别命令。输入 help 查看命令表。");
        }

        if (hasActionResult) {
            applyAndPrint(result, ctx, progress, events);
            if (moved) {
                std::cout << lookAround(ctx) << "\n";
                const std::string eventId = recommendedMainEventId(ctx);
                if (!eventId.empty() && events.canTriggerEvent(eventId, ctx)) {
                    std::cout << "\n【自动触发主线】\n";
                    applyAndPrint(events.triggerAvailableMainEvent(ctx),
                                  ctx, progress, events);
                }
            }
            tryStartPendingBattle(ctx, combat);
        }
    }

    flushOutputToUI();
    std::cout.rdbuf(originalOutput);
    ui.restoreCursor();
    return 0;
}

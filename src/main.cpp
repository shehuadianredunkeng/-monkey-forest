#include "CombatSystem.h"
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
    return room != nullptr && containsId(room->getNPCIds(), npcId);
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

    const std::string targetRoom = requiredRoomForEvent(eventId);
    std::ostringstream output;
    output << "当前主线：" << eventId << "\n";
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
        << "choose 1/2/3                 选择当前事件选项\n"
        << "talk npc_id                  与当前房间 NPC 对话\n"
        << "quest npc_id                 查看 NPC 任务\n"
        << "finish npc_id                提交 NPC 任务\n"
        << "take item_id                 拾取物品\n"
        << "use item_id                  使用物品\n"
        << "train combat/climb/gather/leadership 训练技能\n"
        << "rest                         休息恢复体力\n"
        << "fight                        进入当前剧情战斗\n"
        << "attack / guard / analyze / hack / escape  战斗指令\n"
        << "inventory                    查看背包\n"
        << "status                       查看状态\n"
        << "save / load                  存档或读档\n"
        << "ending                       查看当前结局\n"
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
    const std::vector<Item> before = ctx.player.getInventory().getItems();
    ActionResult result = takeItem(itemId, ctx);
    if (result.success) {
        for (const auto& item : ctx.player.getInventory().getItems()) {
            const auto previous = std::find_if(
                before.begin(), before.end(),
                [&item](const Item& old) { return old.getId() == item.getId(); });
            const int oldCount = previous == before.end() ? 0 : previous->getCount();
            if (item.getCount() > oldCount) {
                ctx.world.setFlag("flag_taken_" + ctx.player.getCurrentRoomId() +
                                  "_" + item.getId());
            }
        }
    }
    return result;
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
    configureConsole();

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

    std::cout << "===== 吗喽森林：家园守卫战 =====\n";
    std::cout << events.getStageIntroduction(world.getStage()) << "\n\n";
    std::cout << lookAround(ctx) << "\n\n";
    std::cout << "输入 help 查看命令，输入 guide 查看下一步建议。\n";

    std::string line;
    while (true) {
        if (ctx.player.getHealth() <= 0 || ctx.world.hasFlag("flag_final_choice")) {
            printEnding(ctx, endings, events);
            break;
        }

        std::cout << "\n> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> words = splitWords(line);
        const std::string command = lowerAscii(words.front());

        if (commandIs(command, "help", "帮助") || command == "?") {
            printHelp();
            continue;
        }

        if (commandIs(command, "quit", "退出") || command == "exit") {
            std::cout << "游戏已退出。记得用 save 保存进度。\n";
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
                std::cout << "当前正在战斗，请使用 attack、guard、analyze、hack、use 或 escape。\n";
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
            if (words.size() > 2) {
                result = makeLocalResult(false, "一条输入只能执行一条命令；take 后可省略目标或指定一个物品。");
            } else {
                result = takeItemOnce(words.size() == 1 ? "" : words[1], ctx);
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
                result = makeLocalResult(false, "请输入 NPC ID。");
            } else if (!npcIsHere(ctx, words[1])) {
                result = makeLocalResult(false, "当前房间没有这个 NPC。");
            } else {
                result = npcs.talkToNPC(words[1], ctx);
            }
        } else if (commandIs(command, "quest", "任务")) {
            if (words.size() < 2) {
                const Room* room = currentRoom(ctx);
                if (room == nullptr || room->getNPCIds().empty()) {
                    std::cout << "当前房间没有可询问任务的 NPC。\n";
                } else {
                    for (const std::string& npcId : room->getNPCIds()) {
                        std::cout << npcId << "：" << npcs.getNPCQuest(npcId, ctx)
                                  << "\n";
                    }
                }
                hasActionResult = false;
            } else if (!npcIsHere(ctx, words[1])) {
                result = makeLocalResult(false, "当前房间没有这个 NPC。");
            } else {
                std::cout << npcs.getNPCQuest(words[1], ctx) << "\n";
                hasActionResult = false;
            }
        } else if (commandIs(command, "finish", "提交") ||
                   command == "complete") {
            if (words.size() < 2) {
                result = makeLocalResult(false, "请输入要提交任务的 NPC ID。");
            } else if (!npcIsHere(ctx, words[1])) {
                result = makeLocalResult(false, "当前房间没有这个 NPC。");
            } else {
                result = npcs.completeNPCQuest(words[1], ctx);
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
            }
            tryStartPendingBattle(ctx, combat);
        }
    }

    return 0;
}

#include "Room.h"

#include "Player.h"
#include "WorldState.h"

#include <algorithm>
#include <sstream>

namespace {

std::string translateNPC(const std::string& id) {
    static const std::map<std::string, std::string> names = {
        {"npc_king", "猴王岩背"},
        {"npc_healer", "叶婆婆"},
        {"npc_child", "幼猴豆豆"},
        {"npc_scout", "闪尾"},
        {"npc_hertz", "星猿工程官赫兹"}
    };

    const auto it = names.find(id);
    if (it == names.end()) {
        return id;
    }

    // 目前主循环仍主要使用英文 ID 交互，因此场景中同时给出名称和可输入 ID。
    return it->second + "（" + id + "）";
}

std::string translateItem(const std::string& id) {
    static const std::map<std::string, std::string> names = {
        {"item_fruit", "果实"},
        {"item_rope", "藤索"},
        {"item_herb", "草药"},
        {"item_flint", "燧石"},
        {"item_chip", "星猿晶片"}
    };

    const auto it = names.find(id);
    if (it == names.end()) {
        return id;
    }

    return it->second + "（" + id + "）";
}

std::string joinNPCs(const std::vector<std::string>& ids) {
    if (ids.empty()) {
        return "无";
    }

    std::ostringstream output;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
            output << ", ";
        }
        output << translateNPC(ids[i]);
    }
    return output.str();
}

std::string joinItems(const std::vector<std::string>& ids) {
    if (ids.empty()) {
        return "无";
    }

    std::ostringstream output;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
            output << ", ";
        }
        output << translateItem(ids[i]);
    }
    return output.str();
}

bool childReturnedToTree(const WorldState& world) {
    // “随机事件已结束”不等于“豆豆已经回家”。
    // 只有真正完成 NPC 救援后才把豆豆从河谷移动到猴王树。
    return world.hasFlag("flag_child_saved") ||
           world.hasFlag("flag_child_rescued") ||
           world.hasFlag("flag_child_returned");
}

std::string describeExits(const GameContext& context, const Room& room) {
    std::ostringstream output;
    bool first = true;

    for (const auto& [direction, roomId] : room.getExits()) {
        // 果实森林到河谷的 up 是技能捷径。未达到条件时不在普通出口中重复展示。
        if (room.getId() == "room_forest" && direction == "up" &&
            context.player.getSkillLevel(SkillType::Climb) < 2) {
            continue;
        }

        if (!first) {
            output << ", ";
        }

        const auto roomIt = context.rooms.find(roomId);
        const std::string targetName =
            roomIt == context.rooms.end() ? roomId : roomIt->second.getName();

        if (direction == "up") {
            output << "up -> " << targetName << "（树冠捷径，不消耗体力）";
        } else {
            output << direction << " -> " << targetName;
        }

        first = false;
    }

    return first ? "无" : output.str();
}

std::string markRoom(const std::string& roomId,
                     const std::string& currentRoomId,
                     const std::string& roomName) {
    return roomId == currentRoomId ? "[*" + roomName + "]" : "[" + roomName + "]";
}

}  // namespace

Room::Room(std::string id,
           std::string name,
           std::string baseDescription,
           std::map<std::string, std::string> exits,
           std::vector<std::string> npcIds,
           std::vector<std::string> itemIds,
           std::string recommendedAction)
    : id_(std::move(id)),
      name_(std::move(name)),
      baseDescription_(std::move(baseDescription)),
      exits_(std::move(exits)),
      npcIds_(std::move(npcIds)),
      itemIds_(std::move(itemIds)),
      recommendedAction_(std::move(recommendedAction)) {}

const std::string& Room::getId() const { return id_; }
const std::string& Room::getName() const { return name_; }
const std::string& Room::getBaseDescription() const { return baseDescription_; }
const std::map<std::string, std::string>& Room::getExits() const { return exits_; }
const std::vector<std::string>& Room::getNPCIds() const { return npcIds_; }
const std::vector<std::string>& Room::getItemIds() const { return itemIds_; }
const std::string& Room::getRecommendedAction() const { return recommendedAction_; }

std::vector<std::string> Room::getVisibleItemIds(const GameContext& context) const {
    std::vector<std::string> visible;

    for (const std::string& itemId : itemIds_) {
        const std::string takenFlag = "flag_taken_" + id_ + "_" + itemId;
        if (!context.world.hasFlag(takenFlag)) {
            visible.push_back(itemId);
        }
    }

    return visible;
}

std::vector<std::string> Room::getVisibleNPCIds(const GameContext& context) const {
    std::vector<std::string> visible = npcIds_;
    const bool childReturned = childReturnedToTree(context.world);

    if (id_ == "room_river" && childReturned) {
        visible.erase(
            std::remove(visible.begin(), visible.end(), "npc_child"),
            visible.end());
    }

    if (id_ == "room_tree" && childReturned &&
        std::find(visible.begin(), visible.end(), "npc_child") == visible.end()) {
        visible.push_back("npc_child");
    }

    return visible;
}

std::string Room::getDynamicRecommendation(const GameContext& context) const {
    const WorldState& world = context.world;
    const std::string currentRoomId = context.player.getCurrentRoomId();

    if (world.hasFlag("flag_final_choice")) {
        return "提示：主线已经完成，可输入 ending 查看结局。";
    }

    std::string targetRoomId;
    std::string objective;

    switch (world.getStage()) {
    case 1:
        targetRoomId = "room_forest";
        objective = "完成树冠试炼";
        break;
    case 2:
        targetRoomId = "room_tree";
        objective = "处理寒冬缺粮危机";
        break;
    case 3:
        if (world.hasFlag("flag_event_glowing_river_done")) {
            targetRoomId = "room_cave";
            objective = "追查回声山洞中的异常";
        } else {
            targetRoomId = "room_river";
            objective = "调查清泉河谷的发光河水";
        }
        break;
    case 4:
        if (world.hasFlag("flag_event_drought_choice_done")) {
            targetRoomId = "room_tree";
            objective = "处理猴群内部的分歧";
        } else {
            targetRoomId = "room_river";
            objective = "处理河谷干旱危机";
        }
        break;
    case 5:
        targetRoomId = "room_base";
        objective = "潜入废弃实验基地";
        break;
    case 6:
        targetRoomId = "room_tree";
        objective = "返回猴王树作出最终选择";
        break;
    default:
        return "提示：当前没有新的主线目标，可输入 guide 查看游戏进度。";
    }

    std::string result;
    if (currentRoomId == targetRoomId) {
        result = "提示：当前主线目标：" + objective +
                 "。输入 investigate 继续主线。";
    } else {
        std::string targetName = targetRoomId;
        const auto target = context.rooms.find(targetRoomId);
        if (target != context.rooms.end()) {
            targetName = target->second.getName();
        }
        result = "提示：当前主线目标：" + objective +
                 "。请前往" + targetName +
                 "，可输入 guide 查看下一步路线。";
    }

    if (!getVisibleItemIds(context).empty()) {
        result += " 当前房间还有可拾取物品，可输入 take 或 take <物品>。";
    }

    return result;
}

std::map<std::string, Room> createAllRooms() {
    return {
        {"room_tree", Room{"room_tree", "猴王树",
            "巨大的猴王树守望着整片家园，枝叶间传来同伴的呼唤。",
            {{"east", "room_forest"}},
            {"npc_king", "npc_healer"}, {},
            "提示：先与猴王岩背交谈。"}},

        {"room_forest", Room{"room_forest", "果实森林",
            "成熟果实散发清甜气味，树冠间有一条通往河谷的高处捷径。",
            {{"west", "room_tree"}, {"east", "room_river"},
             {"south", "room_cave"}, {"up", "room_river"}},
            {"npc_scout"}, {"item_fruit", "item_rope"},
            "提示：收集资源并调查森林。"}},

        {"room_river", Room{"room_river", "清泉河谷",
            "河谷的水流被银色管道截断，只剩浅浅的水洼。",
            {{"west", "room_forest"}, {"east", "room_base"}},
            {"npc_child"}, {"item_herb"},
            "提示：调查异常管道。"}},

        {"room_cave", Room{"room_cave", "回声山洞",
            "潮湿的洞壁把每一步脚步声放大，深处隐约传来机械回响。",
            {{"north", "room_forest"}, {"east", "room_base"}},
            {}, {"item_flint", "item_chip"},
            "提示：分析回声线索。"}},

        {"room_base", Room{"room_base", "废弃实验基地",
            "锈蚀的金属门半掩着，冷光从基地深处的控制台泄出。人员撤离后，自动防御系统仍在运行。",
            {{"west", "room_river"}, {"north", "room_cave"}},
            {"npc_hertz"}, {},
            "提示：调查基地入口。"}}
    };
}

ActionResult movePlayer(GameContext& context, const std::string& direction) {
    const auto current = context.rooms.find(context.player.getCurrentRoomId());
    if (current == context.rooms.end()) {
        return {false, "当前位置无效，无法移动。", false, false};
    }

    const auto exit = current->second.getExits().find(direction);
    if (exit == current->second.getExits().end()) {
        return {false, "这个方向没有可用出口。", false, false};
    }

    const auto target = context.rooms.find(exit->second);
    if (target == context.rooms.end()) {
        return {false, "出口指向未知区域，无法移动。", false, false};
    }

    if (target->second.getId() == "room_base" &&
        !context.world.hasFlag("flag_base_open")) {
        return {false, "基地门禁尚未解锁，需要先完成前置主线。", false, false};
    }

    const bool forestUp =
        current->second.getId() == "room_forest" && direction == "up";

    const bool forestShortcut =
        forestUp &&
        context.player.getSkillLevel(SkillType::Climb) >= 2;

    // 攀爬不足2级时，up仍按普通路线前往河谷并消耗体力；
    // 达到2级后才作为树冠捷径，不消耗体力。
    if (!forestShortcut && context.player.getStamina() <= 0) {
        return {false, "体力不足，请先休息后再移动。", false, false};
    }

    if (!forestShortcut) {
        context.player.changeStamina(-1);
    }

    context.player.setCurrentRoomId(target->second.getId());

    return {
        true,
        forestShortcut
            ? "你通过树冠捷径到达了" + target->second.getName() + "，没有消耗体力。"
            : "你到达了" + target->second.getName() + "。消耗 1 点体力。",
        true,
        false
    };
}

std::string lookAround(const GameContext& context) {
    const auto roomIt = context.rooms.find(context.player.getCurrentRoomId());
    if (roomIt == context.rooms.end()) {
        return "当前位置无效，请返回猴王树重新开始。";
    }

    const Room& room = roomIt->second;
    const std::vector<std::string> visibleNPCs = room.getVisibleNPCIds(context);
    const std::vector<std::string> visibleItems = room.getVisibleItemIds(context);

    std::ostringstream output;
    output << "【" << room.getName() << "】\n";
    output << room.getBaseDescription() << "\n";
    output << "出口：" << describeExits(context, room) << "\n";
    output << "NPC：" << joinNPCs(visibleNPCs) << "\n";
    output << "物品：" << joinItems(visibleItems) << "\n";
    output << room.getDynamicRecommendation(context);
    return output.str();
}

std::string showMap(const GameContext& context) {
    const std::string current = context.player.getCurrentRoomId();

    const std::string tree = markRoom("room_tree", current, "猴王树");
    const std::string forest = markRoom("room_forest", current, "果实森林");
    const std::string river = markRoom("room_river", current, "清泉河谷");
    const std::string cave = markRoom("room_cave", current, "回声山洞");
    const std::string base = markRoom("room_base", current, "实验基地");

    std::ostringstream output;
    output << "===== 青木谷地图 =====\n";
    output << tree << " -- " << forest << " -- " << river << " -- " << base << "\n";
    output << "               |                         /\n";
    output << "          " << cave << " ----------------\n";
    output << "\n* 表示当前位置。\n";

    if (context.player.getSkillLevel(SkillType::Climb) >= 2) {
        output << "已解锁：果实森林 -> 清泉河谷的树冠捷径（go up）。\n";
    } else {
        output << "未解锁：树冠捷径需要攀爬技能2级。\n";
    }

    if (!context.world.hasFlag("flag_base_open")) {
        output << "实验基地：尚未开放。";
    } else {
        output << "实验基地：已开放。";
    }

    return output.str();
}

std::string getCommandHelp() {
    return "可用命令：look 查看场景；map 查看地图；go <direction> 移动；"
           "talk <npc_id> 与 NPC 对话；take <item_id> 拾取物品；"
           "investigate 调查；status 查看状态；help 查看帮助。";
}

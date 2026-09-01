// Modified Room.cpp for member-1-map
// Changes:
// 1. NPC/item display in Chinese
// 2. Room tips improved
// 3. Forest duplicate route removed from normal display while keeping gameplay structure
// 4. Room descriptions optimized

#include "Room.h"

#include "Player.h"
#include "WorldState.h"

#include <sstream>

namespace {

std::string translateNPC(const std::string& id) {
    static const std::map<std::string, std::string> names = {
        {"npc_king", "猴王岩背"},
        {"npc_healer", "灵猴医师"},
        {"npc_child", "幼猴豆豆"},
        {"npc_scout", "林间侦察员"}
    };

    auto it = names.find(id);
    if (it != names.end()) {
        return it->second + "(" + id + ")";
    }
    return id;
}

std::string translateItem(const std::string& id) {
    static const std::map<std::string, std::string> names = {
        {"item_fruit", "果实"},
        {"item_rope", "绳索"},
        {"item_herb", "草药"},
        {"item_flint", "燧石"},
        {"item_chip", "晶片"}
    };

    auto it = names.find(id);
    if (it != names.end()) {
        return it->second + "(" + id + ")";
    }
    return id;
}

std::string joinNPCs(const std::vector<std::string>& ids) {
    if (ids.empty()) return "无";

    std::ostringstream out;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) out << ", ";
        out << translateNPC(ids[i]);
    }
    return out.str();
}

std::string joinItems(const std::vector<std::string>& ids) {
    if (ids.empty()) return "无";

    std::ostringstream out;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) out << ", ";
        out << translateItem(ids[i]);
    }
    return out.str();
}

std::string describeExits(const std::map<std::string, std::string>& exits,
                          const std::map<std::string, Room>& rooms) {
    std::ostringstream out;
    bool first = true;

    for (const auto& [direction, roomId] : exits) {
        if (!first) {
            out << ", ";
        }

        out << direction << " -> ";

        auto it = rooms.find(roomId);
        out << (it == rooms.end() ? roomId : it->second.getName());

        first = false;
    }

    return out.str();
}

}

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

std::map<std::string, Room> createAllRooms() {
    return {
        {"room_tree", Room{"room_tree", "猴王树",
            "巨大的猴王树守望着整片家园，枝叶间传来同伴的呼唤。",
            {{"east", "room_forest"}},
            {"npc_king", "npc_child"}, {},
            "提示：输入 talk npc_king 与猴王岩背交谈，了解当前情况。"}},

        {"room_forest", Room{"room_forest", "果实森林",
            "成熟果实散发清甜气味，树冠间有一条通往河谷的高处捷径。",
            {{"west", "room_tree"}, {"east", "room_river"}, {"south", "room_cave"},
             {"up", "room_river"}},
            {"npc_scout"}, {"item_fruit", "item_rope"},
            "提示：输入 take item_fruit 收集果实，输入 go east 前往清泉河谷。"}},

        {"room_river", Room{"room_river", "清泉河谷",
            "河谷的水流被银色管道截断，只剩浅浅的水洼。",
            {{"west", "room_forest"}, {"east", "room_base"}},
            {"npc_healer"}, {"item_herb"},
            "提示：输入 investigate 调查异常管道，输入 random 探索隐藏事件。"}},

        {"room_cave", Room{"room_cave", "回声山洞",
            "潮湿的洞壁把每一步脚步声放大，深处隐约传来机械回响。",
            {{"north", "room_forest"}, {"east", "room_base"}},
            {}, {"item_flint", "item_chip"},
            "提示：输入 investigate 分析回声来源。"}},

        {"room_base", Room{"room_base", "废弃实验基地",
            "锈蚀的金属门半掩着，冷光从基地深处的控制台泄出。人员撤离后，自动防御系统仍在运行。",
            {{"west", "room_river"}, {"north", "room_cave"}},
            {}, {},
            "提示：输入 investigate 调查基地入口。"}}
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
        return {false, "基地门禁尚未解锁，需要先取得晶片并伪装权限。",
                false, false};
    }

    bool shortcut = current->second.getId() == "room_forest" &&
                    direction == "up" &&
                    context.player.getSkillLevel(SkillType::Climb) >= 2;

    if (!shortcut) {
        if (context.player.getStamina() <= 0) {
            return {false, "体力不足，请先休息后再移动。",
                    false, false};
        }
        context.player.changeStamina(-1);
    }

    context.player.setCurrentRoomId(target->second.getId());

    return {true,
            shortcut ? "你通过树冠捷径到达" + target->second.getName() + "。"
                     : "你到达了" + target->second.getName() + "。消耗 1 点体力。",
            true,
            false};
}

std::string lookAround(const GameContext& context) {
    auto roomIt = context.rooms.find(context.player.getCurrentRoomId());

    if (roomIt == context.rooms.end()) {
        return "当前位置无效，请返回猴王树重新开始。";
    }

    const Room& room = roomIt->second;

    std::ostringstream out;
    out << "【" << room.getName() << "】\n";
    out << room.getBaseDescription() << "\n";
    out << "出口：" << describeExits(room.getExits(), context.rooms) << "\n";
    out << "NPC：" << joinNPCs(room.getNPCIds()) << "\n";
    out << "物品：" << joinItems(room.getItemIds()) << "\n";
    out << room.getRecommendedAction();

    return out.str();
}

std::string getCommandHelp() {
    return "可用命令：look 查看场景；go <direction> 移动；talk <npc_id> 与 NPC 对话；"
           "take <item_id> 拾取物品；status 查看状态；help 查看帮助。";
}

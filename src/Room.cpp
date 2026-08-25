#include "Room.h"

#include "Player.h"

#include <sstream>

namespace {
std::string joinIds(const std::vector<std::string>& ids) {
    if (ids.empty()) {
        return "无";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << ids[index];
    }
    return output.str();
}

std::string describeExits(const std::map<std::string, std::string>& exits,
                          const std::map<std::string, Room>& rooms) {
    std::ostringstream output;
    bool first = true;
    for (const auto& [direction, roomId] : exits) {
        if (!first) {
            output << ", ";
        }
        output << direction << " -> ";
        const auto roomIt = rooms.find(roomId);
        output << (roomIt == rooms.end() ? roomId : roomIt->second.getName());
        first = false;
    }
    return output.str();
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
        {"room_tree", Room{"room_tree", "猴王树", "巨大的猴王树守望着整片家园，枝叶间传来同伴的呼唤。",
                            {{"east", "room_forest"}}, {"npc_king", "npc_child"}, {}, "推荐：go east 前往果实森林。"}},
        {"room_forest", Room{"room_forest", "果实森林", "成熟果实散发清甜气味，树冠间有一条通往河谷的高处捷径。",
                              {{"west", "room_tree"}, {"east", "room_river"}, {"south", "room_cave"}, {"up", "room_river"}},
                              {"npc_scout"}, {"item_fruit", "item_rope"}, "推荐：look 后收集果实，或继续探索。"}},
        {"room_river", Room{"room_river", "清泉河谷", "河谷的水流被银色管道截断，只剩浅浅的水洼。",
                             {{"west", "room_forest"}, {"east", "room_base"}}, {"npc_healer"}, {"item_herb"}, "推荐：调查河谷的异常管道。"}},
        {"room_cave", Room{"room_cave", "回声山洞", "潮湿的洞壁把每一步脚步声放大，深处隐约传来机械回响。",
                            {{"north", "room_forest"}, {"east", "room_base"}}, {}, {"item_flint"}, "推荐：仔细观察回声的方向。"}},
        {"room_base", Room{"room_base", "废弃实验基地", "锈蚀的金属门半掩着，冷光从基地深处的控制台泄出。",
                            {{"west", "room_river"}, {"north", "room_cave"}}, {}, {"item_chip"}, "推荐：调查入口并留意巡逻机器人。"}}
    };
}

ActionResult movePlayer(GameContext& context, const std::string& direction) {
    const auto currentRoomIt = context.rooms.find(context.player.getCurrentRoomId());
    if (currentRoomIt == context.rooms.end()) {
        return {false, "当前位置无效，无法移动。", false, false};
    }

    const auto exitIt = currentRoomIt->second.getExits().find(direction);
    if (exitIt == currentRoomIt->second.getExits().end()) {
        return {false, "这个方向没有可用出口。", false, false};
    }

    const auto targetRoomIt = context.rooms.find(exitIt->second);
    if (targetRoomIt == context.rooms.end()) {
        return {false, "出口指向未知区域，无法移动。", false, false};
    }

    const bool isFreeShortcut = currentRoomIt->second.getId() == "room_forest" &&
                                direction == "up" &&
                                targetRoomIt->second.getId() == "room_river" &&
                                context.player.getSkillLevel(SkillType::Climb) >= 2;
    if (!isFreeShortcut && context.player.getStamina() <= 0) {
        return {false, "体力不足，请先休息后再移动。", false, false};
    }

    if (!isFreeShortcut) {
        context.player.changeStamina(-1);
    }
    context.player.setCurrentRoomId(targetRoomIt->second.getId());

    const std::string costMessage = isFreeShortcut ? "树冠捷径未消耗体力。" : "消耗 1 点体力。";
    return {true, "你到达了" + targetRoomIt->second.getName() + "。" + costMessage, true, false};
}

std::string lookAround(const GameContext& context) {
    const auto roomIt = context.rooms.find(context.player.getCurrentRoomId());
    if (roomIt == context.rooms.end()) {
        return "当前位置无效，请返回猴王树重新开始。";
    }

    const Room& room = roomIt->second;
    std::ostringstream output;
    output << "【" << room.getName() << "】\n";
    output << room.getBaseDescription() << "\n";
    output << "出口：" << describeExits(room.getExits(), context.rooms) << "\n";
    output << "NPC：" << joinIds(room.getNPCIds()) << "\n";
    output << "物品：" << joinIds(room.getItemIds()) << "\n";
    output << room.getRecommendedAction();
    return output.str();
}

std::string getCommandHelp() {
    return "可用命令：look 查看场景；go <direction> 移动；talk <npc_id> 与 NPC 对话；"
           "take <item_id> 拾取物品；status 查看状态；help 查看帮助。";
}

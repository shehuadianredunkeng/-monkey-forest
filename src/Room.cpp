#include "Room.h"

#include "Player.h"
#include "WorldState.h"

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

std::string npcDisplayName(const std::string& id) {
    if (id == "npc_king") return "岩背（king）";
    if (id == "npc_scout") return "闪尾（scout）";
    if (id == "npc_healer") return "叶婆婆（healer）";
    if (id == "npc_child") return "豆豆（child）";
    if (id == "npc_hertz") return "赫兹（hertz）";
    return id;
}

std::string describeNPCs(const std::vector<std::string>& ids) {
    if (ids.empty()) return "无";
    std::ostringstream output;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (index > 0) output << "、";
        output << npcDisplayName(ids[index]);
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
                            {{"east", "room_forest"}}, {"npc_king", "npc_healer"}, {}, "推荐：先与猴王岩背交谈，再前往果实森林。"}},
        {"room_forest", Room{"room_forest", "果实森林", "成熟果实散发清甜气味，树冠间有一条通往河谷的高处捷径。",
                              {{"west", "room_tree"}, {"east", "room_river"}, {"south", "room_cave"}, {"up", "room_river"}},
                              {"npc_scout"}, {"item_fruit", "item_rope"}, "推荐：look 后收集果实，或继续探索。"}},
        {"room_river", Room{"room_river", "清泉河谷", "河谷的水流被银色管道截断，只剩浅浅的水洼。",
                             {{"west", "room_forest"}, {"east", "room_base"}}, {"npc_child"}, {"item_herb"}, "推荐：调查异常管道，并确认豆豆是否安全。"}},
        {"room_cave", Room{"room_cave", "回声山洞", "潮湿的洞壁把每一步脚步声放大，深处隐约传来机械回响。",
                            {{"north", "room_forest"}, {"east", "room_base"}}, {}, {"item_flint", "item_chip"}, "推荐：仔细观察回声的方向。"}},
        {"room_base", Room{"room_base", "星猿实验基地", "冷光从控制台泄出，工程官赫兹与自动防御系统仍在守护抽取塔。",
                            {{"west", "room_river"}, {"north", "room_cave"}}, {"npc_hertz"}, {}, "推荐：调查日志，再决定说服赫兹或与其战斗。"}}
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

    if (targetRoomIt->second.getId() == "room_base" && !context.world.hasFlag("flag_base_open")) {
        return {false, "基地门禁尚未解锁，需要先取得晶片并伪装权限。", false, false};
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
    output << "NPC：" << describeNPCs(room.getNPCIds()) << "\n";
    output << "物品：" << joinIds(room.getItemIds()) << "\n";
    output << room.getRecommendedAction();
    return output.str();
}

std::string getCommandHelp() {
    return "===== 帮助（help）=====\n"
           "查看（look）：查看当前场景\n"
           "移动（go）<方向>：前往相邻地点\n"
           "对话（talk）[中文名/英文名]：获取、推进并提交NPC任务；出现选项后直接输入数字\n"
           "拾取（take）[物品]：拾取物品，可使用中文名或短名称\n"
           "状态（status）：查看玩家状态\n"
           "背包（inventory）：查看随身物品\n"
           "攻击（attack）/防御（guard）/偷窃（steal）：基础战斗动作\n"
           "背包（inventory）：战斗中查看并使用物品\n"
           "特殊战斗会另外提示分析（analyze）、破解（hack）等选项\n"
           "逃跑（escape）：完成闪尾任务后解锁\n"
           "香蕉（banana）<1/2/3>：回应赫兹的香蕉诱惑\n"
           "帮助（help）：再次显示本页";
}

#include "PlayerActions.h"

#include "Item.h"
#include "Room.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace
{
constexpr int TRAIN_STAMINA_COST = 20;
constexpr int REST_STAMINA_RECOVERY = 30;
constexpr int HERB_HEALTH_RECOVERY = 25;
constexpr int FRUIT_STAMINA_RECOVERY = 15;

ActionResult makeResult(bool success, std::string message, bool turnConsumed)
{
    return ActionResult{success, std::move(message), turnConsumed, false};
}

std::optional<Item> createKnownItem(const std::string& itemId)
{
    if (itemId == "item_fruit")
    {
        return Item(itemId, "果实");
    }
    if (itemId == "item_herb")
    {
        return Item(itemId, "草药");
    }
    if (itemId == "item_rope")
    {
        return Item(itemId, "绳索", true, 1);
    }
    if (itemId == "item_flint")
    {
        return Item(itemId, "燧石", true, 1);
    }
    if (itemId == "item_chip")
    {
        return Item(itemId, "芯片", true, 1);
    }
    return std::nullopt;
}

std::string skillName(SkillType type)
{
    switch (type)
    {
    case SkillType::Gather:
        return "采集";
    case SkillType::Climb:
        return "攀爬";
    case SkillType::Combat:
        return "战斗";
    case SkillType::Leadership:
        return "领导";
    }
    return "未知";
}
} // namespace

ActionResult takeItem(const std::string& itemId, GameContext& ctx)
{
    const auto room = ctx.rooms.find(ctx.player.getCurrentRoomId());
    if (room == ctx.rooms.end())
    {
        return makeResult(false, "当前位置不存在，无法拾取物品。", false);
    }

    const auto& roomItemIds = room->second.getItemIds();
    if (std::find(roomItemIds.cbegin(), roomItemIds.cend(), itemId) ==
        roomItemIds.cend())
    {
        return makeResult(false, "当前房间没有该物品。", false);
    }

    const std::optional<Item> item = createKnownItem(itemId);
    if (!item.has_value())
    {
        return makeResult(false, "无法识别该物品。", false);
    }

    if (!ctx.player.addItem(*item))
    {
        return makeResult(false, "背包已满，无法拾取该物品。", false);
    }

    return makeResult(true, "你拾取了" + item->getName() + "。", true);
}

ActionResult useItem(const std::string& itemId, GameContext& ctx)
{
    if (!ctx.player.hasItem(itemId))
    {
        return makeResult(false, "背包中没有该物品。", false);
    }

    if (itemId == "item_herb")
    {
        if (ctx.player.getHealth() >= 100)
        {
            return makeResult(false, "生命值已满，不需要使用草药。", false);
        }
        ctx.player.changeHealth(HERB_HEALTH_RECOVERY);
        ctx.player.removeItem(itemId);
        return makeResult(true, "你使用草药恢复了生命。", true);
    }

    if (itemId == "item_fruit")
    {
        if (ctx.player.getStamina() >= 100)
        {
            return makeResult(false, "体力已满，不需要食用果实。", false);
        }
        ctx.player.changeStamina(FRUIT_STAMINA_RECOVERY);
        ctx.player.removeItem(itemId);
        return makeResult(true, "你食用果实恢复了体力。", true);
    }

    if (itemId == "item_rope" || itemId == "item_flint" ||
        itemId == "item_chip")
    {
        return makeResult(false, "该物品需要在特定事件中使用。", false);
    }

    return makeResult(false, "该物品当前无法使用。", false);
}

ActionResult trainSkill(SkillType type, GameContext& ctx)
{
    if (ctx.player.getSkillLevel(type) >= 3)
    {
        return makeResult(false, skillName(type) + "技能已经达到最高等级。", false);
    }

    if (ctx.player.getStamina() < TRAIN_STAMINA_COST)
    {
        return makeResult(false, "体力不足，无法训练技能。", false);
    }

    ctx.player.changeStamina(-TRAIN_STAMINA_COST);
    ctx.player.changeSkillLevel(type, 1);
    return makeResult(true, skillName(type) + "技能提升了一级。", true);
}

ActionResult rest(GameContext& ctx)
{
    if (ctx.player.getStamina() >= 100)
    {
        return makeResult(false, "体力已满，不需要休息。", false);
    }

    ctx.player.changeStamina(REST_STAMINA_RECOVERY);
    return makeResult(true, "你休息了一会儿，恢复了体力。", true);
}

std::string showInventory(const Player& player)
{
    const auto& items = player.getInventory().getItems();
    if (items.empty())
    {
        return "背包为空。";
    }

    std::ostringstream output;
    output << "背包：";
    for (const Item& item : items)
    {
        output << "\n- " << item.getName() << " x" << item.getCount();
    }
    return output.str();
}

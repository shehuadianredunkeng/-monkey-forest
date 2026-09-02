#include "PlayerActions.h"

#include "Item.h"
#include "Room.h"
#include "WorldState.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

struct ItemInfo
{
    const char* id;
    const char* name;
    const char* shortName;
    const char* chineseAlias;
    bool important;
};

const ItemInfo* findItemInfo(const std::string& target)
{
    static constexpr ItemInfo items[] = {
        {"item_fruit", "果实", "fruit", "果实", false},
        {"item_herb", "草药", "herb", "草药", false},
        {"item_rope", "藤索", "rope", "藤索", true},
        {"item_flint", "燧石", "flint", "燧石", true},
        {"item_chip", "星猿晶片", "chip", "晶片", true},
    };
    for (const auto& item : items)
    {
        if (target == item.id || target == item.name ||
            target == item.shortName || target == item.chineseAlias)
        {
            return &item;
        }
    }
    return nullptr;
}

std::string joinNames(const std::vector<std::string>& names)
{
    std::string text;
    for (const auto& name : names)
    {
        if (!text.empty())
        {
            text += "、";
        }
        text += name;
    }
    return text;
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
    const ItemInfo* target = itemId.empty() ? nullptr : findItemInfo(itemId);
    if (!itemId.empty() && target == nullptr)
    {
        return makeResult(false, "无法识别该物品。", false);
    }
    if (target != nullptr &&
        std::find(roomItemIds.cbegin(), roomItemIds.cend(), target->id) == roomItemIds.cend())
    {
        return makeResult(false, "当前房间没有该物品。", false);
    }

    std::vector<std::string> obtained;
    std::vector<std::string> blocked;
    bool unknownItem = false;
    for (const auto& roomItemId : roomItemIds)
    {
        if (target != nullptr && roomItemId != target->id)
        {
            continue;
        }
        // Use the integration main's existing per-room pickup flag convention.
        // Only the main marks successful pickups; this action never writes WorldState.
        if (ctx.world.hasFlag("flag_taken_" + ctx.player.getCurrentRoomId() + "_" + roomItemId))
        {
            if (target != nullptr)
            {
                return makeResult(false, "这个位置的该物品已经被拿走。", false);
            }
            continue;
        }
        const auto* info = findItemInfo(roomItemId);
        if (info == nullptr)
        {
            unknownItem = true;
            continue;
        }
        if (ctx.player.addItem(Item(info->id, info->name, info->important, 1)))
        {
            obtained.emplace_back(info->name);
        }
        else
        {
            blocked.emplace_back(info->name);
        }
    }

    std::string message;
    if (!obtained.empty())
    {
        message = "你拾取了：" + joinNames(obtained) + "。";
    }
    else if (!blocked.empty())
    {
        message = "背包已满，无法拾取这里的物品。";
    }
    else if (!unknownItem)
    {
        message = "这里没有可以拾取的物品。";
    }
    if (!obtained.empty() && !blocked.empty())
    {
        message += "\n背包空间不足，未能拾取：" + joinNames(blocked) + "。";
    }
    if (unknownItem)
    {
        if (!message.empty())
        {
            message += '\n';
        }
        message += "部分物品无法识别，未能拾取。";
    }
    const bool success = !obtained.empty();
    return makeResult(success, std::move(message), success);
}

ActionResult useItem(const std::string& itemId, GameContext& ctx)
{
    const auto* info = findItemInfo(itemId);
    const std::string canonicalId = info == nullptr ? itemId : info->id;
    if (!ctx.player.hasItem(canonicalId))
    {
        return makeResult(false, "背包中没有该物品。", false);
    }

    if (canonicalId == "item_herb")
    {
        if (ctx.player.getHealth() >= 100)
        {
            return makeResult(false, "生命值已满，不需要使用草药。", false);
        }
        if (!ctx.player.removeItem(canonicalId))
        {
            return makeResult(false, "该物品受到保护，无法消耗。", false);
        }
        ctx.player.changeHealth(HERB_HEALTH_RECOVERY);
        return makeResult(true, "你使用草药恢复了生命。", true);
    }

    if (canonicalId == "item_fruit")
    {
        if (ctx.player.getStamina() >= 100)
        {
            return makeResult(false, "体力已满，不需要食用果实。", false);
        }
        if (!ctx.player.removeItem(canonicalId))
        {
            return makeResult(false, "该物品受到保护，无法消耗。", false);
        }
        ctx.player.changeStamina(FRUIT_STAMINA_RECOVERY);
        return makeResult(true, "你食用果实恢复了体力。", true);
    }

    if (canonicalId == "item_rope" || canonicalId == "item_flint" ||
        canonicalId == "item_chip")
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
        return "背包 0/8\n背包为空。";
    }

    std::ostringstream output;
    output << "背包 " << items.size() << "/8";
    for (const Item& item : items)
    {
        const auto* info = findItemInfo(item.getId());
        output << "\n- " << (info == nullptr ? item.getName() : info->name)
               << " x" << item.getCount();
        if (info != nullptr)
        {
            output << " [" << info->shortName << " / " << info->chineseAlias << "]";
        }
    }
    return output.str();
}

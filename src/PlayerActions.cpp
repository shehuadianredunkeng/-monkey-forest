#include "PlayerActions.h"

#include "Item.h"
#include "Room.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

using namespace std;

namespace
{
constexpr int HERB_HEALTH_RECOVERY = 25;
constexpr int FRUIT_STAMINA_RECOVERY = 15;
constexpr int HONEY_HEALTH_RECOVERY = 15;
constexpr int HONEY_STAMINA_RECOVERY = 10;
constexpr int WILD_SUPPLY_HEALTH_RECOVERY = 10;
constexpr int WILD_SUPPLY_STAMINA_RECOVERY = 20;

ActionResult makeResult(bool success, string message, bool turnConsumed)
{
    return ActionResult{success, move(message), turnConsumed, false};
}

struct ItemInfo
{
    const char* id;
    const char* name;
    const char* shortName;
    const char* chineseAlias;
    bool important;
};

// 玩家可以输入物品名，也可以输入界面上显示的英文简称。
const ItemInfo* findItemInfo(const string& target)
{
    static constexpr ItemInfo items[] = {
        {"item_fruit", "果实", "fruit", "果实", false},
        {"item_herb", "草药", "herb", "草药", false},
        {"item_rope", "藤索", "rope", "藤索", true},
        {"item_flint", "燧石", "flint", "燧石", true},
        {"item_chip", "星猿晶片", "chip", "晶片", true},
        {"item_honey", "蜂蜜", "honey", "蜂蜜", false},
        {"item_wild_supply", "野外补给", "supply", "补给", false},
        {"item_material_fragment", "材料碎片", "material", "材料", false},
        {"item_book", "星猿研究手册", "book", "手册", false},
        {"item_spring_token", "春花", "spring", "春花", true},
        {"item_summer_token", "蝉蜕", "summer", "蝉蜕", true},
        {"item_autumn_token", "秋叶", "autumn", "秋叶", true},
        {"item_winter_token", "落雪", "winter", "落雪", true},
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

} // namespace

ActionResult takeItem(const string& itemId, GameContext& ctx)
{
    // 先确认玩家所在房间有效，再检查房间里有没有目标物品。
    const auto room = ctx.rooms.find(ctx.player.getCurrentRoomId());
    if (room == ctx.rooms.end())
    {
        return makeResult(false, "当前位置不存在，无法拾取物品。", false);
    }

    // 输入可以是内部ID、英文简称或中文名称。
    const ItemInfo* item = findItemInfo(itemId);
    if (item == nullptr)
    {
        return makeResult(false, "无法识别该物品。", false);
    }

    const auto& roomItems = room->second.getItemIds();
    if (find(roomItems.begin(), roomItems.end(), item->id) == roomItems.end())
    {
        return makeResult(false, "当前房间没有该物品。", false);
    }

    // 背包负责处理同类叠加和槽位是否已满。
    if (!ctx.player.addItem(Item(item->id, item->name, item->important, 1)))
    {
        return makeResult(false, "背包已满，无法拾取该物品。", false);
    }
    return makeResult(true, "你拾取了：" + string(item->name) + "。", true);
}

ActionResult useItem(const string& itemId, GameContext& ctx)
{
    // 后面的判断都使用统一ID，中文名和英文简称只在这里转换一次。
    const auto* info = findItemInfo(itemId);
    const string canonicalId = info == nullptr ? itemId : info->id;
    if (!ctx.player.hasItem(canonicalId))
    {
        return makeResult(false, "背包中没有该物品。", false);
    }

    // 普通补给品使用后从背包减去一件。
    if (canonicalId == "item_herb")
    {
        if (ctx.player.getHealth() >= 100)
        {
            return makeResult(false, "生命值已满，不需要使用草药。", false);
        }
        ctx.player.removeItem(canonicalId);
        ctx.player.changeHealth(HERB_HEALTH_RECOVERY);
        return makeResult(true, "你使用草药恢复了生命。", true);
    }

    if (canonicalId == "item_fruit")
    {
        if (ctx.player.getStamina() >= 100)
        {
            return makeResult(false, "体力已满，不需要食用果实。", false);
        }
        ctx.player.removeItem(canonicalId);
        ctx.player.changeStamina(FRUIT_STAMINA_RECOVERY);
        return makeResult(true, "你食用果实恢复了体力。", true);
    }

    if (canonicalId == "item_honey")
    {
        if (ctx.player.getHealth() >= 100 && ctx.player.getStamina() >= 100)
        {
            return makeResult(false,
                              "你尝了尝蜂蜜，现在生命和体力都很充足，该物品暂时无效。",
                              false);
        }
        ctx.player.removeItem(canonicalId);
        ctx.player.changeHealth(HONEY_HEALTH_RECOVERY);
        ctx.player.changeStamina(HONEY_STAMINA_RECOVERY);
        return makeResult(true, "你吃下蜂蜜，生命+15，体力+10。", true);
    }

    if (canonicalId == "item_wild_supply")
    {
        if (ctx.player.getHealth() >= 100 && ctx.player.getStamina() >= 100)
        {
            return makeResult(false,
                              "生命和体力都已充足，野外补给现在没有效果。",
                              false);
        }
        ctx.player.removeItem(canonicalId);
        ctx.player.changeHealth(WILD_SUPPLY_HEALTH_RECOVERY);
        ctx.player.changeStamina(WILD_SUPPLY_STAMINA_RECOVERY);
        return makeResult(true, "你拆开野外补给，生命+10，体力+20。", true);
    }

    // 四季信物留到最终阶段统一判定，不能提前消耗。
    if (canonicalId == "item_spring_token" ||
        canonicalId == "item_summer_token" ||
        canonicalId == "item_autumn_token" ||
        canonicalId == "item_winter_token")
    {
        return makeResult(false,
                          "这件四季信物会在最终阶段自动回应森林，现在直接使用没有效果。",
                          false);
    }

    // 藤索、燧石和晶片由对应剧情事件读取。
    if (canonicalId == "item_rope" || canonicalId == "item_flint" ||
        canonicalId == "item_chip")
    {
        return makeResult(false, "该物品需要在特定事件中使用。", false);
    }

    if (canonicalId == "item_material_fragment" || canonicalId == "item_book")
    {
        return makeResult(false,
                          "该物品在获得时已经提供属性奖励，可留作收藏，现在使用没有额外效果。",
                          false);
    }

    return makeResult(false, "该物品当前无法使用。", false);
}

string showInventory(const Player& player)
{
    const auto& items = player.getInventory().getItems();
    if (items.empty())
    {
        return "背包 0/" + to_string(Inventory::MAX_SLOTS) + "\n背包为空。";
    }

    // 显示名称来自上面的物品表，不把item_fruit这类内部ID给玩家看。
    ostringstream output;
    output << "背包 " << items.size() << "/" << Inventory::MAX_SLOTS;
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

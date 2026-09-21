#include "Inventory.h"
#include "Item.h"
#include "Player.h"
#include "PlayerActions.h"
#include "Room.h"
#include "TestFramework.h"
#include "WorldState.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace
{
void testItemAndInventory()
{
    // 先检查物品本身，再检查叠加、关键物品和容量限制。
    Item fruit("item_fruit", "果实", false, 2);
    expect(fruit.getId() == "item_fruit", "Item ID mismatch");
    expect(fruit.getName() == "果实", "Item name mismatch");
    expect(fruit.getCount() == 2, "Item count mismatch");
    expect(!fruit.isImportant(), "Fruit should not be important");

    fruit.addCount(3);
    fruit.reduceCount(20);
    expect(fruit.getCount() == 0, "Item count must not become negative");

    Inventory inventory;
    expect(inventory.addItem(Item("item_fruit", "果实", false, 2)),
           "First item should be added");
    expect(inventory.addItem(Item("item_fruit", "果实", false, 3)),
           "Same ID should stack");
    expect(inventory.getItems().size() == 1, "Stack should occupy one slot");
    expect(inventory.getItems().front().getCount() == 5, "Stack count mismatch");
    expect(inventory.removeItem("item_fruit"), "Ordinary item should be removable");
    expect(inventory.getItems().front().getCount() == 4,
           "Removing should consume one item");

    expect(inventory.addItem(Item("item_chip", "芯片", true, 1)),
           "Important item should be addable");
    expect(!inventory.removeItem("item_chip"),
           "Important item must be protected from deletion");

    Inventory full;
    for (int i = 0; i < 12; ++i)
    {
        expect(full.addItem(Item("test_" + to_string(i), "测试物品")),
               "Each of twelve slots should be usable");
    }
    expect(full.addItem(Item("test_0", "测试物品", false, 2)),
           "Existing stacks should accept items when full");
    expect(!full.addItem(Item("test_12", "额外物品")),
           "Thirteenth distinct ID should be rejected");
}

void testPlayerState()
{
    Player player;
    expect(player.getHealth() == 100, "Initial health mismatch");
    expect(player.getStamina() == 60, "Initial stamina mismatch");
    expect(player.getStrength() == 1, "Initial strength mismatch");
    expect(player.getWisdom() == 1, "Initial wisdom mismatch");
    expect(player.getReputation() == 0, "Initial reputation mismatch");
    expect(player.getCurrentRoomId() == "room_tree", "Initial room mismatch");

    // 使用较大的改变量，确认各项属性不会越过上下限。
    player.changeHealth(-500);
    player.changeStamina(500);
    player.changeStrength(500);
    player.changeWisdom(-500);
    player.changeReputation(500);
    expect(player.getHealth() == 0, "Health lower bound mismatch");
    expect(player.getStamina() == 100, "Stamina upper bound mismatch");
    expect(player.getStrength() == 5, "Strength upper bound mismatch");
    expect(player.getWisdom() == 1, "Wisdom lower bound mismatch");
    expect(player.getReputation() == 100, "Reputation upper bound mismatch");

    player.changeHealth(500);
    player.changeStamina(-500);
    player.changeStrength(-500);
    player.changeWisdom(500);
    player.changeReputation(-500);
    expect(player.getHealth() == 100, "Health upper bound mismatch");
    expect(player.getStamina() == 0, "Stamina lower bound mismatch");
    expect(player.getStrength() == 1, "Strength lower bound mismatch");
    expect(player.getWisdom() == 5, "Wisdom upper bound mismatch");
    expect(player.getReputation() == 0, "Reputation lower bound mismatch");

    const SkillType skills[] = {SkillType::Climb,
                                SkillType::Combat,
                                SkillType::Leadership};
    for (SkillType skill : skills)
    {
        expect(player.getSkillLevel(skill) == 1, "Initial skill level mismatch");
        player.changeSkillLevel(skill, 100);
        expect(player.getSkillLevel(skill) ==
                   (skill == SkillType::Combat ? 5 : 3),
               "Skill upper bound mismatch");
        player.changeSkillLevel(skill, -100);
        expect(player.getSkillLevel(skill) == 1, "Skill lower bound mismatch");
    }

    expect(player.addItem(Item("item_fruit", "果实")),
           "Player should delegate item addition");
    expect(player.hasItem("item_fruit"), "Player should delegate item lookup");
    expect(player.removeItem("item_fruit"), "Player should delegate item removal");
    expect(player.getInventory().getItems().empty(), "Inventory accessor mismatch");

    player.setCurrentRoomId("room_forest");
    expect(player.getCurrentRoomId() == "room_forest", "Room setter mismatch");
}

void testPlayerActions()
{
    Player player;
    WorldState world;
    map<string, Room> rooms;
    rooms.emplace("room_tree",
                  Room("room_tree",
                       "树屋",
                       "树冠上的家。",
                       {},
                       {},
                       {"item_fruit", "item_herb", "item_chip"}));
    GameContext context{player, world, rooms};

    const ActionResult take = takeItem("item_fruit", context);
    expect(take.success && take.turnConsumed && !take.stageCompleted,
           "Successful take result mismatch");
    expect(player.hasItem("item_fruit"), "takeItem should add item");

    player.changeStamina(-60);
    const ActionResult use = useItem("item_fruit", context);
    expect(use.success && use.turnConsumed, "Fruit should be usable");
    expect(player.getStamina() == 15, "Fruit stamina recovery mismatch");
    expect(!player.hasItem("item_fruit"), "Used fruit should be consumed");

    expect(showInventory(player).find("背包为空。") != string::npos,
           "Empty inventory text mismatch");
    expect(!takeItem("item_rope", context).success,
           "Unavailable room item should not be taken");
}
struct PickupFixture
{
    // 给拾取相关测试准备一个只包含指定物品的房间。
    Player player;
    WorldState world;
    map<string, Room> rooms;
    GameContext context{player, world, rooms};

    explicit PickupFixture(vector<string> ids)
    {
        rooms.emplace("room_tree", Room("room_tree", "树屋", "", {}, {},
                                         move(ids)));
    }
};

int countOf(const Player& player, const string& id)
{
    for (const auto& item : player.getInventory().getItems())
    {
        if (item.getId() == id)
        {
            return item.getCount();
        }
    }
    return 0;
}

void testRemoveOneUntilSlotDisappears()
{
    Inventory bag;
    expect(bag.addItem(Item("item_herb", "草药", 3)), "Add herbs");
    expect(bag.removeItem("item_herb"), "Remove first herb");
    expect(bag.hasItem("item_herb") && bag.getItems().size() == 1 &&
               bag.getItems().front().getCount() == 2, "3 -> 2 must keep slot");
    expect(bag.removeItem("item_herb"), "Remove second herb");
    expect(bag.getItems().front().getCount() == 1, "2 -> 1 must keep slot");
    expect(bag.removeItem("item_herb"), "Remove final herb");
    expect(!bag.hasItem("item_herb") && bag.getItems().empty(),
           "1 -> 0 must erase slot");
    expect(!bag.removeItem("item_herb"), "Absent item must fail");
    expect(!bag.removeItem("不存在") && bag.getItems().empty(),
           "Missing item must not create a slot");
}

void testConsumableUseRemovesExactlyOne()
{
    PickupFixture f({});
    f.player.changeHealth(-70);
    expect(f.player.addItem(Item("item_herb", "草药", 2)), "Add herbs");
    auto result = useItem("item_herb", f.context);
    expect(result.success && result.turnConsumed && !result.stageCompleted,
           "Successful herb use is one action");
    expect(f.player.getHealth() == 55 && countOf(f.player, "item_herb") == 1,
           "First use must heal and consume one herb");
    result = useItem("item_herb", f.context);
    expect(result.success && f.player.getHealth() == 80 &&
               !f.player.hasItem("item_herb"), "Second use must erase herb slot");
    result = useItem("item_herb", f.context);
    expect(!result.success && !result.turnConsumed && f.player.getHealth() == 80,
           "Missing herb must not heal or consume a turn");
}

void testPlotItemsAreNotConsumed()
{
    PickupFixture f({});
    for (const auto* id : {"item_chip", "item_rope", "item_flint"})
    {
        expect(f.player.addItem(Item(id, "关键物品", true, 1)), "Add plot item");
        const auto result = useItem(id, f.context);
        expect(!result.success && !result.turnConsumed && countOf(f.player, id) == 1,
               "Context-only items must not be consumed");
    }
}

void testHoneyAndSeasonTokensAreRecognized()
{
    PickupFixture f({});
    f.player.changeHealth(-50);
    f.player.changeStamina(-40);
    expect(f.player.addItem(Item("item_honey", "蜂蜜", false, 1)),
           "Add honey");
    const auto honey = useItem("蜂蜜", f.context);
    expect(honey.success && f.player.getHealth() == 65 &&
               f.player.getStamina() == 30 &&
               !f.player.hasItem("item_honey"),
           "Honey alias must heal, restore stamina and consume one item");

    expect(f.player.addItem(Item("item_spring_token", "春花", true, 1)),
           "Add spring token");
    const auto token = useItem("春花", f.context);
    expect(!token.success && !token.turnConsumed &&
               token.message.find("没有效果") != string::npos &&
               f.player.hasItem("item_spring_token"),
           "Season token must be recognized, preserved and explain its lack of direct effect");
}

void testAllCombatRewardItemsAreRecognized()
{
    PickupFixture f({});
    f.player.changeHealth(-40);
    f.player.changeStamina(-50);
    expect(f.player.addItem(Item("item_wild_supply", "野外补给", false, 1)),
           "Add wild supply");
    const auto supply = useItem("野外补给", f.context);
    expect(supply.success && supply.turnConsumed &&
               f.player.getHealth() == 70 && f.player.getStamina() == 30 &&
               !f.player.hasItem("item_wild_supply"),
           "Wild supply must restore health/stamina and be consumed");

    expect(f.player.addItem(Item("item_material_fragment", "材料碎片", false, 1)),
           "Add material fragment");
    expect(f.player.addItem(Item("item_book", "星猿研究手册", false, 1)),
           "Add research book");
    const auto material = useItem("材料", f.context);
    const auto book = useItem("book", f.context);
    expect(!material.success && material.message.find("背包中没有") == string::npos &&
               f.player.hasItem("item_material_fragment"),
           "Material fragment must be recognized and preserved");
    expect(!book.success && book.message.find("背包中没有") == string::npos &&
               f.player.hasItem("item_book"),
           "Research book must be recognized and preserved");
}

void testSpecifiedPickupDoesNotTakeOtherItems()
{
    PickupFixture f({"item_fruit", "item_herb"});
    const auto result = takeItem("item_herb", f.context);
    expect(result.success && result.turnConsumed && f.player.hasItem("item_herb") &&
               !f.player.hasItem("item_fruit"), "Specified pickup must remain selective");
}

void testInventoryDisplayUsesCanonicalNamesAndSlots()
{
    Player player;
    player.addItem(Item("item_fruit", "item_fruit", 3));
    player.addItem(Item("item_herb", "旧名", 2));
    player.addItem(Item("item_chip", "芯片", true, 1));
    const auto text = showInventory(player);
    expect(text == "背包 3/12\n- 果实 x3 [fruit / 果实]\n- 草药 x2 [herb / 草药]\n- 星猿晶片 x1 [chip / 晶片]",
           "Bag must display canonical names, quantity, aliases and distinct slots");
    expect(text.find("item_") == string::npos, "Bag must not expose internal IDs");
    player.addItem(Item("item_rope", "绳索", true, 1));
    player.addItem(Item("item_flint", "旧燧石", true, 1));
    const auto all = showInventory(player);
    expect(all.find("藤索 x1 [rope / 藤索]") != string::npos &&
               all.find("燧石 x1 [flint / 燧石]") != string::npos,
           "Rope and flint must use the same display mapping");
    expect(player.getInventory().getItems().size() == 5 && countOf(player, "item_fruit") == 3,
           "Display must be read-only");
}

void testEmptyInventoryShowsZeroSlots()
{
    expect(showInventory(Player{}) == "背包 0/12\n背包为空。",
           "Empty inventory must show zero occupied slots in Chinese");
}
} // namespace

#ifndef MEMBER4_USE_REAL_WORLD
bool WorldState::hasFlag(const string&) const
{
    return false;
}
int WorldState::getStage() const
{
    return 1;
}
#endif

int main()
{
    const pair<const char*, void (*)()> tests[] = {
        {"existing item/inventory", testItemAndInventory},
        {"existing player state", testPlayerState},
        {"existing actions", testPlayerActions},
        {"remove stacked units", testRemoveOneUntilSlotDisappears},
        {"consume herbs", testConsumableUseRemovesExactlyOne},
        {"protect plot items", testPlotItemsAreNotConsumed},
        {"honey and season tokens", testHoneyAndSeasonTokensAreRecognized},
        {"all combat reward items", testAllCombatRewardItemsAreRecognized},
        {"specified pickup", testSpecifiedPickupDoesNotTakeOtherItems},
        {"inventory display", testInventoryDisplayUsesCanonicalNamesAndSlots},
        {"empty inventory display", testEmptyInventoryShowsZeroSlots},
    };
    int failed = 0;
    for (const auto& test : tests)
    {
        try
        {
            test.second();
        }
        catch (const exception& error)
        {
            ++failed;
            cerr << test.first << ": " << error.what() << '\n';
        }
    }
    cout << "member4: " << (sizeof(tests) / sizeof(tests[0]) - failed)
              << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

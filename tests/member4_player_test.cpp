#include "Inventory.h"
#include "Item.h"
#include "Player.h"
#include "PlayerActions.h"
#include "Room.h"
#include "WorldState.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void testItemAndInventory()
{
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
    for (int i = 0; i < 8; ++i)
    {
        expect(full.addItem(Item("test_" + std::to_string(i), "测试物品")),
               "Each of eight slots should be usable");
    }
    expect(full.isFull(), "Eight distinct IDs should fill the inventory");
    expect(full.addItem(Item("test_0", "测试物品", false, 2)),
           "Existing stacks should accept items when full");
    expect(!full.addItem(Item("test_8", "额外物品")),
           "Ninth distinct ID should be rejected");
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

    const SkillType skills[] = {SkillType::Gather,
                                SkillType::Climb,
                                SkillType::Combat,
                                SkillType::Leadership};
    for (SkillType skill : skills)
    {
        expect(player.getSkillLevel(skill) == 1, "Initial skill level mismatch");
        player.changeSkillLevel(skill, 100);
        expect(player.getSkillLevel(skill) == 3, "Skill upper bound mismatch");
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
    std::map<std::string, Room> rooms;
    rooms.emplace("room_tree",
                  Room("room_tree",
                       "树屋",
                       "树冠上的家。",
                       {},
                       {},
                       {"item_fruit", "item_herb", "item_chip"},
                       "观察四周"));
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

    player.changeStamina(100);
    const ActionResult train = trainSkill(SkillType::Gather, context);
    expect(train.success && train.turnConsumed, "Skill training should succeed");
    expect(player.getSkillLevel(SkillType::Gather) == 2,
           "Training should increase skill level");
    expect(player.getStamina() == 80, "Training stamina cost mismatch");

    player.changeStamina(-50);
    const ActionResult rested = rest(context);
    expect(rested.success && rested.turnConsumed, "Rest should succeed");
    expect(player.getStamina() == 60, "Rest recovery mismatch");

    expect(showInventory(player).find("背包为空。") != std::string::npos,
           "Empty inventory text mismatch");
    expect(!takeItem("item_rope", context).success,
           "Unavailable room item should not be taken");
}
struct PickupFixture
{
    Player player;
    WorldState world;
    std::map<std::string, Room> rooms;
    GameContext context{player, world, rooms};

    explicit PickupFixture(std::vector<std::string> ids)
    {
        rooms.emplace("room_tree", Room("room_tree", "树屋", "", {}, {},
                                         std::move(ids), ""));
    }
};

int countOf(const Player& player, const std::string& id)
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

void fillSlots(Player& player, int count)
{
    for (int i = 0; i < count; ++i)
    {
        expect(player.addItem(Item("test_" + std::to_string(i), "测试物品")),
               "Test setup could not fill inventory slot");
    }
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

void testUnremovableConsumableDoesNotGrantFreeEffect()
{
    PickupFixture f({});
    f.player.changeHealth(-50);
    expect(f.player.addItem(Item("item_herb", "任务草药", true, 2)), "Add protected herb");
    const auto result = useItem("item_herb", f.context);
    expect(!result.success && !result.turnConsumed && f.player.getHealth() == 50 &&
               countOf(f.player, "item_herb") == 2,
           "Failed removal must not grant an unconsumed healing effect");
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

void testSpecifiedPickupDoesNotTakeOtherItems()
{
    PickupFixture f({"item_fruit", "item_herb"});
    const auto result = takeItem("item_herb", f.context);
    expect(result.success && result.turnConsumed && f.player.hasItem("item_herb") &&
               !f.player.hasItem("item_fruit"), "Specified pickup must remain selective");
}

void testBatchPickupInRoomOrder()
{
    PickupFixture f({"item_fruit", "item_herb", "item_rope"});
    const auto result = takeItem("", f.context);
    expect(result.success && result.turnConsumed && !result.stageCompleted,
           "Batch pickup must return one successful action");
    const auto& items = f.player.getInventory().getItems();
    expect(items.size() == 3 && items[0].getId() == "item_fruit" &&
               items[1].getId() == "item_herb" && items[2].getId() == "item_rope",
           "Batch pickup must follow room order");
    expect(result.message.find("果实") != std::string::npos &&
               result.message.find("草药") != std::string::npos &&
               result.message.find("藤索") != std::string::npos,
           "Batch pickup must name every obtained item");
}

void testBatchPartialCapacity()
{
    PickupFixture f({"item_fruit", "item_herb", "item_rope"});
    fillSlots(f.player, 7);
    const auto result = takeItem("", f.context);
    expect(result.success && result.turnConsumed && f.player.hasItem("item_fruit") &&
               !f.player.hasItem("item_herb") && !f.player.hasItem("item_rope"),
           "Partial pickup must preserve successes without rollback");
    expect(result.message.find("果实") != std::string::npos &&
               result.message.find("空间不足") != std::string::npos &&
               result.message.find("草药") != std::string::npos &&
               result.message.find("藤索") != std::string::npos,
           "Partial pickup must explain obtained and blocked items");
}

void testFullBagStillStacksAfterFailedNewItem()
{
    PickupFixture f({"item_rope", "item_fruit"});
    fillSlots(f.player, 7);
    expect(f.player.addItem(Item("item_fruit", "果实", 3)), "Add existing fruit stack");
    const auto result = takeItem("", f.context);
    expect(result.success && result.turnConsumed && countOf(f.player, "item_fruit") == 4 &&
               !f.player.hasItem("item_rope") && f.player.getInventory().getItems().size() == 8,
           "Full bag must continue attempting later existing stacks");
}

void testBatchFullEmptyAndMissingRooms()
{
    PickupFixture f({"item_fruit", "item_herb"});
    fillSlots(f.player, 8);
    auto result = takeItem("", f.context);
    expect(!result.success && !result.turnConsumed && !result.stageCompleted &&
               !f.player.hasItem("item_fruit"), "No item obtained means no turn");
    expect(result.message.find("背包已满") != std::string::npos, "Explain full bag");
    PickupFixture empty({});
    result = takeItem("", empty.context);
    expect(!result.success && !result.turnConsumed &&
               result.message.find("没有可以拾取") != std::string::npos, "Explain empty room");
    empty.player.setCurrentRoomId("missing");
    result = takeItem("", empty.context);
    expect(!result.success && !result.turnConsumed && !result.message.empty(),
           "Missing room must fail safely");
}

void testUnknownRoomItemsDoNotBlockKnownItems()
{
    PickupFixture f({"item_unknown", "item_fruit"});
    const auto result = takeItem("", f.context);
    expect(result.success && f.player.hasItem("item_fruit") &&
               !f.player.hasItem("item_unknown"), "Unknown ID must not abort valid pickups");
    expect(result.message.find("无法识别") != std::string::npos, "Explain unknown item");
}

void testDisplayedAliasesWorkWithoutCommandChains()
{
    const std::string aliases[][3] = {
        {"item_fruit", "fruit", "果实"}, {"item_herb", "herb", "草药"},
        {"item_rope", "rope", "藤索"}, {"item_flint", "flint", "燧石"},
        {"item_chip", "chip", "晶片"},
    };
    for (const auto& entry : aliases)
    {
        for (const auto& alias : entry)
        {
            PickupFixture f({entry[0]});
            expect(takeItem(alias, f.context).success && countOf(f.player, entry[0]) == 1,
                   "Displayed alias must resolve to canonical inventory ID");
        }
    }
    PickupFixture f({"item_herb"});
    f.player.changeHealth(-60);
    expect(takeItem("herb", f.context).success && useItem("草药", f.context).success &&
               !f.player.hasItem("item_herb"), "Use must accept displayed aliases too");
    for (const auto* chain : {"herb; use herb", "herb && use herb", "herb, use herb"})
    {
        const auto result = takeItem(chain, f.context);
        expect(!result.success && !result.turnConsumed && !f.player.hasItem("item_herb"),
               "Item aliases must not interpret command chains");
    }
}

void testInventoryDisplayUsesCanonicalNamesAndSlots()
{
    Player player;
    player.addItem(Item("item_fruit", "item_fruit", 3));
    player.addItem(Item("item_herb", "旧名", 2));
    player.addItem(Item("item_chip", "芯片", true, 1));
    const auto text = showInventory(player);
    expect(text == "背包 3/8\n- 果实 x3 [fruit / 果实]\n- 草药 x2 [herb / 草药]\n- 星猿晶片 x1 [chip / 晶片]",
           "Bag must display canonical names, quantity, aliases and distinct slots");
    expect(text.find("item_") == std::string::npos, "Bag must not expose internal IDs");
    player.addItem(Item("item_rope", "绳索", true, 1));
    player.addItem(Item("item_flint", "旧燧石", true, 1));
    const auto all = showInventory(player);
    expect(all.find("藤索 x1 [rope / 藤索]") != std::string::npos &&
               all.find("燧石 x1 [flint / 燧石]") != std::string::npos,
           "Rope and flint must use the same display mapping");
    expect(player.getInventory().getItems().size() == 5 && countOf(player, "item_fruit") == 3,
           "Display must be read-only");
}

void testEmptyInventoryShowsZeroSlots()
{
    expect(showInventory(Player{}) == "背包 0/8\n背包为空。",
           "Empty inventory must show zero occupied slots in Chinese");
}
} // namespace

#ifndef MEMBER4_USE_REAL_WORLD
bool WorldState::hasFlag(const std::string&) const
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
    const std::pair<const char*, void (*)()> tests[] = {
        {"existing item/inventory", testItemAndInventory},
        {"existing player state", testPlayerState},
        {"existing actions", testPlayerActions},
        {"remove stacked units", testRemoveOneUntilSlotDisappears},
        {"consume herbs", testConsumableUseRemovesExactlyOne},
        {"protected consumable", testUnremovableConsumableDoesNotGrantFreeEffect},
        {"protect plot items", testPlotItemsAreNotConsumed},
        {"specified pickup", testSpecifiedPickupDoesNotTakeOtherItems},
        {"batch pickup", testBatchPickupInRoomOrder},
        {"partial pickup", testBatchPartialCapacity},
        {"full bag stacking", testFullBagStillStacksAfterFailedNewItem},
        {"full/empty/missing room", testBatchFullEmptyAndMissingRooms},
        {"unknown room item", testUnknownRoomItemsDoNotBlockKnownItems},
        {"aliases without chains", testDisplayedAliasesWorkWithoutCommandChains},
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
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr << test.first << ": " << error.what() << '\n';
        }
    }
    std::cout << "member4: " << (sizeof(tests) / sizeof(tests[0]) - failed)
              << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

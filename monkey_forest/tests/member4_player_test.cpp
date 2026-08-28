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

    expect(showInventory(player) == "背包为空。", "Empty inventory text mismatch");
    expect(!takeItem("item_rope", context).success,
           "Unavailable room item should not be taken");
}
} // namespace

int main()
{
    try
    {
        testItemAndInventory();
        testPlayerState();
        testPlayerActions();
    }
    catch (const std::exception& error)
    {
        std::cerr << "member4_player_test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

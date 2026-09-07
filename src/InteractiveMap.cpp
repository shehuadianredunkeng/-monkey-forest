#include "InteractiveMap.h"

#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <algorithm>

namespace {
using PointPair = std::pair<int, int>;

std::vector<std::string> terrain(int width, int height) {
    std::vector<std::string> result(static_cast<size_t>(height),
                                    std::string(static_cast<size_t>(width), '.'));
    for (int x = 0; x < width; ++x) {
        result.front()[x] = '#';
        result.back()[x] = '#';
    }
    for (int y = 0; y < height; ++y) {
        result[y].front() = '#';
        result[y].back() = '#';
    }
    return result;
}

void scatter(std::vector<std::string>& map, char tile,
             std::initializer_list<PointPair> points) {
    for (const auto& point : points) map[point.second][point.first] = tile;
}

std::string itemTakenFlag(const std::string& roomId, const std::string& itemId) {
    return "flag_taken_" + roomId + "_" + itemId;
}

std::string npcName(const std::string& id) {
    if (id == "npc_king") return "岩背";
    if (id == "npc_healer") return "叶婆婆";
    if (id == "npc_scout") return "闪尾";
    if (id == "npc_child") return "豆豆";
    if (id == "npc_hertz") return "赫兹";
    return id;
}

std::string itemName(const std::string& id) {
    if (id == "item_fruit") return "果实";
    if (id == "item_rope") return "藤索";
    if (id == "item_herb") return "草药";
    if (id == "item_flint") return "燧石";
    if (id == "item_chip") return "星猿晶片";
    return id;
}

std::string enemyName(const std::string& id) {
    if (id == "enemy_bees") return "暴躁蜂群";
    if (id == "enemy_robot") return "巡逻机器人";
    return id;
}
}

InteractiveMap::InteractiveMap() {
    Definition tree;
    tree.terrain = terrain(tree.width, tree.height);
    tree.terrain[7][35] = 'D';
    scatter(tree.terrain, '#', {{14, 3}, {14, 4}, {21, 9}, {22, 9},
                                {7, 11}, {8, 11}, {27, 6}});
    tree.start = {17, 11};
    tree.doors[{35, 7}] = "east";
    tree.npcs[{7, 5}] = "npc_king";
    tree.npcs[{11, 10}] = "npc_healer";
    tree.chests[{29, 4}] = "chest_tree";
    tree.eggs[{31, 11}] = "egg_tree_ring";
    definitions_["room_tree"] = tree;

    Definition forest;
    forest.terrain = terrain(forest.width, forest.height);
    forest.terrain[7][0] = 'D';
    forest.terrain[7][35] = 'D';
    forest.terrain[14][18] = 'D';
    scatter(forest.terrain, '#', {{8, 3}, {9, 3}, {15, 5}, {16, 5},
                                  {26, 3}, {27, 3}, {6, 11}, {7, 11},
                                  {28, 10}, {29, 10}});
    forest.start = {4, 7};
    forest.doors[{0, 7}] = "west";
    forest.doors[{35, 7}] = "east";
    forest.doors[{18, 14}] = "south";
    forest.npcs[{6, 5}] = "npc_scout";
    forest.items[{12, 4}] = "item_fruit";
    forest.items[{24, 10}] = "item_rope";
    forest.chests[{31, 4}] = "chest_forest";
    forest.enemies[{30, 11}] = "enemy_bees";
    forest.eggs[{10, 11}] = "egg_golden_banana";
    definitions_["room_forest"] = forest;

    Definition river;
    river.terrain = terrain(river.width, river.height);
    river.terrain[7][0] = 'D';
    river.terrain[7][35] = 'D';
    for (int y = 2; y <= 12; ++y)
        if (y != 7) for (int x = 16; x <= 19; ++x) river.terrain[y][x] = '~';
    river.start = {3, 7};
    river.doors[{0, 7}] = "west";
    river.doors[{35, 7}] = "east";
    river.npcs[{12, 10}] = "npc_child";
    river.items[{28, 4}] = "item_herb";
    river.chests[{7, 3}] = "chest_river";
    river.eggs[{29, 11}] = "egg_moon_pebble";
    definitions_["room_river"] = river;

    Definition cave;
    cave.terrain = terrain(cave.width, cave.height);
    cave.terrain[0][18] = 'D';
    cave.terrain[7][35] = 'D';
    scatter(cave.terrain, '#', {{7, 3}, {8, 3}, {9, 3}, {25, 3},
                                {26, 3}, {12, 8}, {13, 8}, {23, 11},
                                {24, 11}, {25, 11}});
    cave.start = {18, 2};
    cave.doors[{18, 0}] = "north";
    cave.doors[{35, 7}] = "east";
    cave.items[{9, 10}] = "item_flint";
    cave.items[{28, 4}] = "item_chip";
    cave.chests[{29, 10}] = "chest_cave";
    cave.eggs[{16, 7}] = "egg_stone_tablet";
    definitions_["room_cave"] = cave;

    Definition base;
    base.terrain = terrain(base.width, base.height);
    base.terrain[7][0] = 'D';
    base.terrain[0][18] = 'D';
    scatter(base.terrain, '#', {{8, 5}, {8, 6}, {8, 7}, {15, 3},
                                {16, 3}, {24, 10}, {25, 10}, {29, 4}});
    base.start = {3, 7};
    base.doors[{0, 7}] = "west";
    base.doors[{18, 0}] = "north";
    base.npcs[{27, 7}] = "npc_hertz";
    base.enemies[{12, 10}] = "enemy_robot";
    base.chests[{7, 4}] = "chest_base";
    base.eggs[{31, 11}] = "egg_debug_banana";
    definitions_["room_base"] = base;
}

const InteractiveMap::Definition* InteractiveMap::current() const {
    const auto found = definitions_.find(roomId_);
    return found == definitions_.end() ? nullptr : &found->second;
}

void InteractiveMap::resetForRoom(const GameContext& ctx,
                                  const std::string& enteredByDirection) {
    roomId_ = ctx.player.getCurrentRoomId();
    const Definition* map = current();
    if (map == nullptr) return;
    player_ = enteredByDirection.empty()
                  ? map->start
                  : spawnAfterTransition(*map, enteredByDirection);
    facing_ = {1, 0};
}

void InteractiveMap::ensureCurrentRoom(const GameContext& ctx) {
    if (roomId_ != ctx.player.getCurrentRoomId()) resetForRoom(ctx);
}

InteractiveMap::Point InteractiveMap::spawnAfterTransition(
    const Definition& target, const std::string& direction) const {
    if (direction == "east") {
        for (const auto& [point, exit] : target.doors)
            if (exit == "west") return {point.x + 1, point.y};
    } else if (direction == "west") {
        for (const auto& [point, exit] : target.doors)
            if (exit == "east") return {point.x - 1, point.y};
    } else if (direction == "south") {
        for (const auto& [point, exit] : target.doors)
            if (exit == "north") return {point.x, point.y + 1};
    } else if (direction == "north") {
        for (const auto& [point, exit] : target.doors)
            if (exit == "south") return {point.x, point.y - 1};
    }
    return target.start;
}

bool InteractiveMap::npcVisible(const std::string& npcId,
                                const GameContext& ctx) const {
    if (npcId == "npc_scout" &&
        (ctx.world.hasFlag("flag_scout_quest_complete") ||
         ctx.world.hasFlag("flag_scout_help") ||
         ctx.world.hasFlag("flag_scout_left"))) return false;
    if (npcId == "npc_child" && ctx.world.hasFlag("flag_child_rescued"))
        return false;
    return true;
}

bool InteractiveMap::itemVisible(const std::string& itemId,
                                 const GameContext& ctx) const {
    return !ctx.world.hasFlag(itemTakenFlag(roomId_, itemId));
}

bool InteractiveMap::enemyVisible(const std::string& enemyId,
                                  const GameContext& ctx) const {
    if (enemyId == "enemy_bees")
        return !ctx.world.hasFlag("flag_bees_defeated");
    if (enemyId == "enemy_robot")
        return ctx.world.hasFlag("flag_base_open") &&
               !ctx.world.hasFlag("flag_robot_defeated");
    return true;
}

InteractiveMap::Point InteractiveMap::questPoint(const GameContext& ctx) const {
    const int stage = ctx.world.getStage();
    if (stage == 1 && roomId_ == "room_forest") return {18, 5};
    if (stage == 2 && roomId_ == "room_tree") return {18, 6};
    if (stage == 3 && !ctx.world.hasFlag("flag_event_glowing_river_done") &&
        roomId_ == "room_river") return {24, 6};
    if (stage == 3 && ctx.world.hasFlag("flag_event_glowing_river_done") &&
        roomId_ == "room_cave") return {21, 7};
    if (stage == 4 && !ctx.world.hasFlag("flag_event_drought_choice_done") &&
        roomId_ == "room_river") return {23, 9};
    if (stage == 4 && ctx.world.hasFlag("flag_event_drought_choice_done") &&
        roomId_ == "room_tree") return {19, 7};
    if (stage == 5 && roomId_ == "room_base") return {18, 7};
    if (stage == 6 && roomId_ == "room_tree") return {18, 7};
    return {-1, -1};
}

bool InteractiveMap::questIsHere(const GameContext& ctx) const {
    const Point quest = questPoint(ctx);
    return quest.x >= 0 && !ctx.world.hasFlag("flag_final_choice");
}

MapMoveResult InteractiveMap::move(int dx, int dy, GameContext& ctx) {
    MapMoveResult outcome;
    const Definition* map = current();
    if (map == nullptr) {
        outcome.action = {false, "当前地图不存在。", false, false};
        return outcome;
    }
    facing_ = {dx, dy};
    const Point target{player_.x + dx, player_.y + dy};
    if (target.x < 0 || target.y < 0 || target.x >= map->width ||
        target.y >= map->height) return outcome;

    const auto door = map->doors.find(target);
    if (door != map->doors.end()) {
        outcome.action = movePlayer(ctx, door->second);
        if (outcome.action.success) {
            roomId_ = ctx.player.getCurrentRoomId();
            const Definition* destination = current();
            if (destination != nullptr)
                player_ = spawnAfterTransition(*destination, door->second);
            outcome.moved = true;
            outcome.roomChanged = true;
        }
        return outcome;
    }

    const char terrainTile = map->terrain[target.y][target.x];
    if (terrainTile == '#' || terrainTile == '~') return outcome;
    const MapInteraction occupied = interactionAt(target, ctx);
    if (occupied.kind != InteractionKind::None &&
        occupied.kind != InteractionKind::Quest) return outcome;
    player_ = target;
    outcome.moved = true;
    outcome.action = {true, "", false, false};
    return outcome;
}

MapInteraction InteractiveMap::interactionAt(Point point,
                                             const GameContext& ctx) const {
    const Definition* map = current();
    if (map == nullptr) return {};
    if (questIsHere(ctx) && point == questPoint(ctx))
        return {InteractionKind::Quest, "main_quest", "红色任务点：按Enter推进剧情"};
    const auto npc = map->npcs.find(point);
    if (npc != map->npcs.end() && npcVisible(npc->second, ctx))
        return {InteractionKind::Npc, npc->second,
                "与" + npcName(npc->second) + "交互"};
    const auto item = map->items.find(point);
    if (item != map->items.end() && itemVisible(item->second, ctx))
        return {InteractionKind::Item, item->second,
                "拾取" + itemName(item->second)};
    const auto chest = map->chests.find(point);
    if (chest != map->chests.end() &&
        !ctx.world.hasFlag("flag_opened_" + chest->second))
        return {InteractionKind::Chest, chest->second, "打开宝箱"};
    const auto enemy = map->enemies.find(point);
    if (enemy != map->enemies.end() && enemyVisible(enemy->second, ctx))
        return {InteractionKind::Enemy, enemy->second,
                "迎战" + enemyName(enemy->second)};
    const auto egg = map->eggs.find(point);
    if (egg != map->eggs.end() &&
        !ctx.world.hasFlag("flag_found_" + egg->second))
        return {InteractionKind::EasterEgg, egg->second, "调查闪光彩蛋"};
    return {};
}

MapInteraction InteractiveMap::interact(const GameContext& ctx) const {
    if (questIsHere(ctx) && player_ == questPoint(ctx))
        return interactionAt(player_, ctx);
    const Point ahead{player_.x + facing_.x, player_.y + facing_.y};
    MapInteraction result = interactionAt(ahead, ctx);
    if (result.kind != InteractionKind::None) return result;
    const Point around[] = {{player_.x + 1, player_.y},
                            {player_.x - 1, player_.y},
                            {player_.x, player_.y + 1},
                            {player_.x, player_.y - 1}};
    for (const Point point : around) {
        result = interactionAt(point, ctx);
        if (result.kind != InteractionKind::None) return result;
    }
    return {InteractionKind::None, "", "附近没有可互动目标"};
}

MapTileVisual InteractiveMap::visualAt(int x, int y,
                                       const GameContext& ctx) const {
    const Definition* map = current();
    if (map == nullptr || x < 0 || y < 0 || x >= map->width || y >= map->height)
        return {};
    const Point point{x, y};
    if (point == player_) return {L"猴", UI::Color::Player};
    if (questIsHere(ctx) && point == questPoint(ctx))
        return {L"！", UI::Color::Quest};
    const auto npc = map->npcs.find(point);
    if (npc != map->npcs.end() && npcVisible(npc->second, ctx))
        return {L"友", UI::Color::Npc};
    const auto item = map->items.find(point);
    if (item != map->items.end() && itemVisible(item->second, ctx))
        return {L"物", UI::Color::Item};
    const auto chest = map->chests.find(point);
    if (chest != map->chests.end() &&
        !ctx.world.hasFlag("flag_opened_" + chest->second))
        return {L"宝", UI::Color::Chest};
    const auto enemy = map->enemies.find(point);
    if (enemy != map->enemies.end() && enemyVisible(enemy->second, ctx))
        return {L"敌", UI::Color::Error};
    const auto egg = map->eggs.find(point);
    if (egg != map->eggs.end() &&
        !ctx.world.hasFlag("flag_found_" + egg->second))
        return {L"? ", UI::Color::Hint};
    if (map->doors.count(point)) return {L"门", UI::Color::Door};
    const char tile = map->terrain[y][x];
    if (tile == '#') return {L"##", UI::Color::Wall};
    if (tile == '~') return {L"~~", UI::Color::Water};
    return {L"·", UI::Color::Grass};
}

std::string InteractiveMap::nearbyHint(const GameContext& ctx) const {
    const MapInteraction target = interact(ctx);
    if (target.kind != InteractionKind::None)
        return target.hint + "（Enter/空格）";
    return "WASD/方向键移动；靠近彩色目标后按Enter或空格";
}

int InteractiveMap::width() const { const Definition* m = current(); return m ? m->width : 0; }
int InteractiveMap::height() const { const Definition* m = current(); return m ? m->height : 0; }
int InteractiveMap::playerX() const { return player_.x; }
int InteractiveMap::playerY() const { return player_.y; }
const std::string& InteractiveMap::roomId() const { return roomId_; }

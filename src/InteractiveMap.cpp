#include "InteractiveMap.h"

#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <algorithm>
#include <sstream>

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

void horizontal(std::vector<std::string>& map, int y, int from, int to,
                char tile = '#') {
    for (int x = from; x <= to; ++x) map[y][x] = tile;
}

void vertical(std::vector<std::string>& map, int x, int from, int to,
              char tile = '#') {
    for (int y = from; y <= to; ++y) map[y][x] = tile;
}

std::vector<std::string> treeTerrain(int width, int height) {
    auto map = terrain(width, height);
    // 盘根与树干围成开阔的王树广场，中间和东侧保留主通道。
    horizontal(map, 3, 4, 10);
    horizontal(map, 3, 25, 31);
    vertical(map, 14, 2, 5);
    vertical(map, 22, 9, 12);
    horizontal(map, 11, 5, 10);
    horizontal(map, 5, 27, 32);
    scatter(map, '#', {{4, 4}, {10, 4}, {6, 10}, {12, 10},
                       {26, 10}, {30, 11}, {32, 11}});
    return map;
}

std::vector<std::string> forestTerrain(int width, int height) {
    auto map = terrain(width, height);
    // 成片果树形成弯曲林道，三处出口之间始终保留可达路线。
    horizontal(map, 3, 7, 12);
    horizontal(map, 3, 24, 29);
    vertical(map, 16, 4, 6);
    vertical(map, 25, 7, 10);
    horizontal(map, 11, 5, 9);
    horizontal(map, 12, 27, 32);
    scatter(map, '#', {{8, 4}, {11, 4}, {15, 5}, {17, 5},
                       {23, 9}, {28, 10}, {6, 12}, {12, 9},
                       {30, 4}, {32, 8}});
    return map;
}

std::vector<std::string> riverTerrain(int width, int height) {
    auto map = terrain(width, height);
    // 河道南北贯穿地图，在中央石桥处留出东西通路。
    for (int y = 1; y < height - 1; ++y) {
        if (y == 7) continue;
        const int bend = y < 5 ? -1 : (y > 10 ? 1 : 0);
        for (int x = 16 + bend; x <= 19 + bend; ++x) map[y][x] = '~';
    }
    horizontal(map, 3, 4, 6);
    horizontal(map, 3, 8, 9);
    horizontal(map, 12, 25, 31);
    scatter(map, '#', {{6, 4}, {9, 4}, {11, 10}, {12, 11},
                       {27, 3}, {30, 4}, {24, 10}});
    return map;
}

std::vector<std::string> caveTerrain(int width, int height) {
    auto map = terrain(width, height);
    // 岩壁构成上下错落的洞道，仅北侧存在真实出口。
    horizontal(map, 3, 3, 12);
    horizontal(map, 3, 24, 32);
    vertical(map, 8, 8, 12);
    vertical(map, 27, 7, 11);
    horizontal(map, 8, 12, 20);
    horizontal(map, 12, 17, 24);
    scatter(map, '#', {{14, 4}, {21, 4}, {5, 9}, {23, 10},
                       {31, 9}, {12, 7}});
    return map;
}

std::vector<std::string> baseTerrain(int width, int height) {
    auto map = terrain(width, height);
    // 金属隔墙划分实验区、控制区与中央走廊。
    vertical(map, 9, 2, 5);
    vertical(map, 9, 9, 12);
    vertical(map, 24, 2, 5);
    vertical(map, 24, 9, 12);
    horizontal(map, 5, 10, 16);
    horizontal(map, 5, 20, 23);
    horizontal(map, 10, 13, 18);
    horizontal(map, 10, 22, 23);
    scatter(map, '#', {{15, 3}, {16, 3}, {20, 11}, {29, 4},
                       {30, 4}, {31, 10}});
    return map;
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
    if (id == "enemy_raider") return "流浪山魈";
    if (id == "enemy_drone") return "侦察无人机";
    if (id.find("enemy_season_guardian_") == 0) return "四季守望者";
    return id;
}

std::string baseEnemyId(const std::string& id) {
    const std::size_t separator = id.find('@');
    return separator == std::string::npos ? id : id.substr(0, separator);
}

std::string safeFlagPart(std::string text) {
    std::replace(text.begin(), text.end(), '@', '_');
    return text;
}

int seasonIndex(const GameContext& ctx) {
    return (ctx.world.getTurnCount() / 6) % 4;
}

const char* seasonId(int season) {
    static const char* ids[] = {"spring", "summer", "autumn", "winter"};
    return ids[season % 4];
}

const char* seasonChinese(int season) {
    static const char* names[] = {"春花", "蝉蜕", "秋叶", "落雪"};
    return names[season % 4];
}

std::string seasonRoom(int season) {
    static const char* rooms[] = {
        "room_forest", "room_river", "room_cave", "room_tree"};
    return rooms[season % 4];
}
}

InteractiveMap::InteractiveMap() {
    Definition tree;
    tree.terrain = treeTerrain(tree.width, tree.height);
    tree.terrain[7][35] = 'D';
    tree.start = {17, 11};
    tree.doors[{35, 7}] = "east";
    tree.npcs[{7, 5}] = "npc_king";
    tree.npcs[{11, 10}] = "npc_healer";
    tree.chests[{29, 4}] = "chest_tree";
    tree.eggs[{31, 11}] = "egg_tree_ring";
    definitions_["room_tree"] = tree;

    Definition forest;
    forest.terrain = forestTerrain(forest.width, forest.height);
    forest.terrain[7][0] = 'D';
    forest.terrain[7][35] = 'D';
    forest.terrain[14][18] = 'D';
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
    river.terrain = riverTerrain(river.width, river.height);
    river.terrain[7][0] = 'D';
    river.terrain[7][35] = 'D';
    river.start = {3, 7};
    river.doors[{0, 7}] = "west";
    river.doors[{35, 7}] = "east";
    river.npcs[{12, 10}] = "npc_child";
    river.items[{28, 4}] = "item_herb";
    river.chests[{7, 3}] = "chest_river";
    river.eggs[{29, 11}] = "egg_moon_pebble";
    definitions_["room_river"] = river;

    Definition cave;
    cave.terrain = caveTerrain(cave.width, cave.height);
    cave.terrain[0][18] = 'D';
    cave.start = {18, 2};
    cave.doors[{18, 0}] = "north";
    cave.items[{9, 10}] = "item_flint";
    cave.items[{28, 4}] = "item_chip";
    cave.chests[{29, 10}] = "chest_cave";
    cave.eggs[{16, 7}] = "egg_stone_tablet";
    definitions_["room_cave"] = cave;

    Definition base;
    base.terrain = baseTerrain(base.width, base.height);
    base.terrain[7][0] = 'D';
    base.start = {3, 7};
    base.doors[{0, 7}] = "west";
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
    if (enteredByDirection.empty()) {
        const std::string prefix = "flag_map_position_" + roomId_ + "_";
        for (const std::string& flag : ctx.world.getFlags()) {
            if (flag.rfind(prefix, 0) != 0) continue;
            std::istringstream encoded(flag.substr(prefix.size()));
            int x = 0;
            int y = 0;
            char separator = 0;
            if (encoded >> x >> separator >> y && separator == '_' &&
                x > 0 && y > 0 && x < map->width - 1 && y < map->height - 1 &&
                map->terrain[y][x] != '#' && map->terrain[y][x] != '~')
                player_ = {x, y};
        }
    }
    facing_ = {1, 0};
}

void InteractiveMap::ensureCurrentRoom(const GameContext& ctx) {
    if (roomId_ != ctx.player.getCurrentRoomId()) resetForRoom(ctx);
}

void InteractiveMap::storePosition(GameContext& ctx) const {
    for (const std::string& flag : ctx.world.getFlags())
        if (flag.rfind("flag_map_position_", 0) == 0)
            ctx.world.removeFlag(flag);
    ctx.world.setFlag("flag_map_position_" + roomId_ + "_" +
                      std::to_string(player_.x) + "_" +
                      std::to_string(player_.y));
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
    if (enemyId.find('@') != std::string::npos)
        return !ctx.world.hasFlag("flag_enemy_defeated_" +
                                  safeFlagPart(enemyId));
    if (enemyId == "enemy_bees")
        return !ctx.world.hasFlag("flag_bees_defeated");
    if (enemyId == "enemy_robot")
        return ctx.world.hasFlag("flag_base_open") &&
               !ctx.world.hasFlag("flag_robot_defeated");
    return true;
}

bool InteractiveMap::doorLocked(const std::string& direction,
                                const GameContext& ctx) const {
    const auto room = ctx.rooms.find(roomId_);
    if (room == ctx.rooms.end()) return false;
    const auto exit = room->second.getExits().find(direction);
    return exit != room->second.getExits().end() &&
           exit->second == "room_base" &&
           !ctx.world.hasFlag("flag_base_open");
}

std::vector<InteractiveMap::Point> InteractiveMap::dynamicPoints(
    const GameContext& ctx) const {
    const Definition* map = current();
    if (map == nullptr) return {};
    const Point candidates[] = {
        {4, 3}, {18, 3}, {30, 6}, {4, 12}, {18, 11},
        {31, 12}, {24, 4}, {10, 9}, {26, 12}, {14, 12}, {22, 6}};
    std::vector<Point> available;
    const Point quest = questPoint(ctx);
    for (const Point point : candidates) {
        if (map->terrain[point.y][point.x] == '#' ||
            map->terrain[point.y][point.x] == '~' || point == quest ||
            map->doors.count(point) || map->npcs.count(point) ||
            map->items.count(point) || map->chests.count(point) ||
            map->enemies.count(point) || map->eggs.count(point)) continue;
        available.push_back(point);
    }
    if (!available.empty()) {
        const std::size_t rotation = static_cast<std::size_t>(
            ctx.world.getTurnCount()) % available.size();
        std::rotate(available.begin(), available.begin() + rotation,
                    available.end());
    }
    return available;
}

MapInteraction InteractiveMap::companionAt(Point point,
                                           const GameContext& ctx) const {
    if (!ctx.world.hasFlag("flag_scout_help") ||
        ctx.world.hasFlag("flag_scout_left")) return {};
    const std::vector<Point> points = dynamicPoints(ctx);
    if (!points.empty() && point == points[0])
        return {InteractionKind::Npc, "npc_scout", "和同行的闪尾聊聊天"};
    return {};
}

MapInteraction InteractiveMap::seasonalGuardianAt(
    Point point, const GameContext& ctx) const {
    const int season = seasonIndex(ctx);
    if (roomId_ != seasonRoom(season) ||
        ctx.world.hasFlag(std::string("flag_season_relic_") +
                          seasonId(season))) return {};
    const std::vector<Point> points = dynamicPoints(ctx);
    if (points.size() < 3) return {};
    const std::string defeated = std::string("flag_season_guardian_defeated_") +
                                 seasonId(season);
    if (point == points[2] && ctx.world.hasFlag(defeated))
        return {InteractionKind::EasterEgg,
                std::string("season_relic_") + seasonId(season),
                std::string("拾取稀有信物·") + seasonChinese(season)};
    if (point == points[2])
        return {InteractionKind::LockedItem,
                std::string("season_relic_") + seasonId(season),
                std::string("稀有信物·") + seasonChinese(season) +
                    "被守望者保护着，需先挑战旁边的红色敌人"};
    if (point == points[1])
        return {InteractionKind::Enemy,
                std::string("enemy_season_guardian_") + seasonId(season) +
                    "@" + roomId_ + "@" +
                    std::to_string(ctx.world.getTurnCount() / 6),
                std::string("挑战守护") + seasonChinese(season) + "的四季守望者"};
    return {};
}

MapInteraction InteractiveMap::randomEventAt(Point point,
                                             const GameContext& ctx) const {
    const std::vector<Point> points = dynamicPoints(ctx);
    if (points.size() < 4 || !(point == points[3])) return {};
    const std::string id = "location_event_" + roomId_ + "_" +
                           std::to_string(ctx.world.getTurnCount());
    if (ctx.world.hasFlag("flag_resolved_" + id)) return {};
    return {InteractionKind::RandomEvent, id, "查看本回合新出现的地点异动"};
}

MapInteraction InteractiveMap::dynamicEnemyAt(Point point,
                                              const GameContext& ctx) const {
    const std::vector<Point> points = dynamicPoints(ctx);
    for (std::size_t slot = 4; slot < points.size() && slot < 6; ++slot) {
        if (!(point == points[slot])) continue;
        const bool mechanical = roomId_ == "room_cave" || roomId_ == "room_base";
        const std::string base = mechanical ? "enemy_drone" : "enemy_raider";
        const std::string id = base + "@" + roomId_ + "@" +
            std::to_string(ctx.world.getTurnCount()) + "@" +
            std::to_string(slot - 4);
        if (enemyVisible(id, ctx))
            return {InteractionKind::Enemy, id,
                    "迎战" + enemyName(base) + "（本回合游荡敌人）"};
    }
    return {};
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
    constexpr int ROOM_TRANSITION_STAMINA_COST = 3;
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
        if (ctx.player.getStamina() < ROOM_TRANSITION_STAMINA_COST) {
            outcome.action = {
                false,
                "体力不足：切换地图需要 3 点体力，请先休息或使用恢复物品。",
                false,
                false
            };
            return outcome;
        }

        const int staminaBeforeMove = ctx.player.getStamina();
        outcome.action = movePlayer(ctx, door->second);
        if (outcome.action.success) {
            // 旧的房间接口可能扣除 0 或 1 点体力；互动地图在这里统一
            // 校正为“每次成功切换地图固定消耗 3 点”，避免重复扣除。
            const int expectedStamina =
                staminaBeforeMove - ROOM_TRANSITION_STAMINA_COST;
            ctx.player.changeStamina(expectedStamina - ctx.player.getStamina());
            outcome.action.message = "切换地图成功，消耗 3 点体力。";
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
    MapInteraction dynamic = companionAt(point, ctx);
    if (dynamic.kind != InteractionKind::None) return dynamic;
    dynamic = seasonalGuardianAt(point, ctx);
    if (dynamic.kind != InteractionKind::None) return dynamic;
    dynamic = randomEventAt(point, ctx);
    if (dynamic.kind != InteractionKind::None) return dynamic;
    dynamic = dynamicEnemyAt(point, ctx);
    if (dynamic.kind != InteractionKind::None) return dynamic;
    const auto door = map->doors.find(point);
    if (door != map->doors.end() && doorLocked(door->second, ctx))
        return {InteractionKind::LockedDoor, door->second,
                "无法解锁：需推进至第四阶段、平息猴群分歧并取得基地线索"};
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
    MapInteraction dynamic = companionAt(point, ctx);
    if (dynamic.kind != InteractionKind::None)
        return {L"伴", UI::Color::Item};
    dynamic = seasonalGuardianAt(point, ctx);
    if (dynamic.kind == InteractionKind::EasterEgg ||
        dynamic.kind == InteractionKind::LockedItem)
        return {L"物", UI::Color::Item};
    if (dynamic.kind == InteractionKind::Enemy)
        return {L"敌", UI::Color::Error};
    dynamic = randomEventAt(point, ctx);
    if (dynamic.kind != InteractionKind::None)
        return {L"奇", UI::Color::Door};
    dynamic = dynamicEnemyAt(point, ctx);
    if (dynamic.kind != InteractionKind::None)
        return {L"敌", UI::Color::Error};
    const auto door = map->doors.find(point);
    if (door != map->doors.end())
        return doorLocked(door->second, ctx)
            ? MapTileVisual{L"锁", UI::Color::Error}
            : MapTileVisual{L"门", UI::Color::Door};
    const char tile = map->terrain[y][x];
    if (tile == '#') return {L"##", UI::Color::Wall};
    if (tile == '~') return {L"~~", UI::Color::Water};
    return {L"··", UI::Color::Grass};
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
char InteractiveMap::terrainAt(int x, int y) const {
    const Definition* map = current();
    if (map == nullptr || x < 0 || y < 0 || x >= map->width || y >= map->height)
        return '\0';
    return map->terrain[y][x];
}

#pragma once

#include "CommonTypes.h"
#include "UI/ConsoleRenderer.h"

#include <map>
#include <string>
#include <vector>

enum class InteractionKind {
    None,
    Npc,
    Item,
    Chest,
    Enemy,
    Quest,
    EasterEgg,
    LockedDoor,
    LockedItem,
    RandomEvent
};

struct MapInteraction {
    InteractionKind kind = InteractionKind::None;
    std::string id;
    std::string hint;
};

struct MapMoveResult {
    bool moved = false;
    bool roomChanged = false;
    ActionResult action;
};

struct MapTileVisual {
    std::wstring glyph = L"  ";
    UI::Color color = UI::Color::Normal;
};

class InteractiveMap {
public:
    InteractiveMap();

    void resetForRoom(const GameContext& ctx,
                      const std::string& enteredByDirection = "");
    void ensureCurrentRoom(const GameContext& ctx);
    void storePosition(GameContext& ctx) const;
    MapMoveResult move(int dx, int dy, GameContext& ctx);
    MapInteraction interact(const GameContext& ctx) const;

    int width() const;
    int height() const;
    int playerX() const;
    int playerY() const;
    const std::string& roomId() const;
    char terrainAt(int x, int y) const;
    MapTileVisual visualAt(int x, int y, const GameContext& ctx) const;
    std::string nearbyHint(const GameContext& ctx) const;

private:
    struct Point {
        int x = 0;
        int y = 0;
        bool operator<(const Point& other) const {
            return y == other.y ? x < other.x : y < other.y;
        }
        bool operator==(const Point& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct Definition {
        int width = 36;
        int height = 15;
        std::vector<std::string> terrain;
        Point start{3, 7};
        std::map<Point, std::string> doors;
        std::map<Point, std::string> npcs;
        std::map<Point, std::string> items;
        std::map<Point, std::string> chests;
        std::map<Point, std::string> enemies;
        std::map<Point, std::string> eggs;
    };

    std::map<std::string, Definition> definitions_;
    std::string roomId_;
    Point player_;
    Point facing_{1, 0};

    const Definition* current() const;
    Point questPoint(const GameContext& ctx) const;
    bool questIsHere(const GameContext& ctx) const;
    bool npcVisible(const std::string& npcId, const GameContext& ctx) const;
    bool itemVisible(const std::string& itemId, const GameContext& ctx) const;
    bool enemyVisible(const std::string& enemyId, const GameContext& ctx) const;
    bool doorLocked(const std::string& direction,
                    const GameContext& ctx) const;
    MapInteraction dynamicEnemyAt(Point point,
                                  const GameContext& ctx) const;
    MapInteraction seasonalGuardianAt(Point point,
                                      const GameContext& ctx) const;
    MapInteraction randomEventAt(Point point,
                                 const GameContext& ctx) const;
    MapInteraction companionAt(Point point,
                               const GameContext& ctx) const;
    std::vector<Point> dynamicPoints(const GameContext& ctx) const;
    MapInteraction interactionAt(Point point, const GameContext& ctx) const;
    Point spawnAfterTransition(const Definition& target,
                               const std::string& direction) const;
};

#pragma once

#include "CommonTypes.h"
#include "UI/ConsoleRenderer.h"

#include <map>
#include <string>
#include <vector>

using namespace std;

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
    string id;
    string hint;
};

struct MapMoveResult {
    bool moved = false;
    bool roomChanged = false;
    ActionResult action;
};

struct MapTileVisual {
    wstring glyph = L"  ";
    UI::Color color = UI::Color::Normal;
};

class InteractiveMap {
public:
    InteractiveMap();

    void resetForRoom(const GameContext& ctx,
                      const string& enteredByDirection = "");
    void ensureCurrentRoom(const GameContext& ctx);
    void storePosition(GameContext& ctx) const;
    MapMoveResult move(int dx, int dy, GameContext& ctx);
    MapInteraction interact(const GameContext& ctx) const;

    int width() const;
    int height() const;
    int playerX() const;
    int playerY() const;
    const string& roomId() const;
    char terrainAt(int x, int y) const;
    MapTileVisual visualAt(int x, int y, const GameContext& ctx) const;
    string nearbyHint(const GameContext& ctx) const;

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
        vector<string> terrain;
        Point start{3, 7};
        map<Point, string> doors;
        map<Point, string> npcs;
        map<Point, string> items;
        map<Point, string> chests;
        map<Point, string> enemies;
        map<Point, string> eggs;
    };

    map<string, Definition> definitions_;
    string roomId_;
    Point player_;
    Point facing_{1, 0};

    const Definition* current() const;
    Point questPoint(const GameContext& ctx) const;
    bool questIsHere(const GameContext& ctx) const;
    bool npcVisible(const string& npcId, const GameContext& ctx) const;
    bool itemVisible(const string& itemId, const GameContext& ctx) const;
    bool enemyVisible(const string& enemyId, const GameContext& ctx) const;
    bool doorLocked(const string& direction,
                    const GameContext& ctx) const;
    MapInteraction dynamicEnemyAt(Point point,
                                  const GameContext& ctx) const;
    MapInteraction seasonalGuardianAt(Point point,
                                      const GameContext& ctx) const;
    MapInteraction randomEventAt(Point point,
                                 const GameContext& ctx) const;
    MapInteraction companionAt(Point point,
                               const GameContext& ctx) const;
    vector<Point> dynamicPoints(const GameContext& ctx) const;
    MapInteraction interactionAt(Point point, const GameContext& ctx) const;
    Point spawnAfterTransition(const Definition& target,
                               const string& direction) const;
};

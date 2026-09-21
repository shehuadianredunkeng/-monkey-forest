#pragma once
#include <string>
#include <vector>
using namespace std;

class WorldState;

struct CollectionEntry {
    string id;
    string name;
    string description;
    bool hidden = false;//默认是影藏的
};
// 成就结局统一收集入口。新内容先注册，再在达成处调用 unlock
class CollectionSystem {
public:
    CollectionSystem();

    void registerEnding(const CollectionEntry& entry);
    void registerAchievement(const CollectionEntry& entry);

    bool unlockEnding(const string& endingId, WorldState& world) const;
    bool unlockAchievement(const string& achievementId,
                           WorldState& world) const;
    // 把现有旧旗标补录进新收集系统
    void syncLegacyFlags(WorldState& world) const;

    bool isEndingUnlocked(const string& endingId,
                          const WorldState& world) const;
    bool isAchievementUnlocked(const string& achievementId,
                               const WorldState& world) const;

    int unlockedEndingCount(const WorldState& world) const;
    string getEndingCollectionText(const WorldState& world,
                                        bool showConditions = false) const;
    string getAchievementCollectionText(const WorldState& world,
                                        bool showConditions = false) const;

    const vector<CollectionEntry>& endings() const;
    const vector<CollectionEntry>& achievements() const;

};

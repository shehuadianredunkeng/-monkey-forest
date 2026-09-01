#pragma once

#include <string>
#include <vector>

class WorldState;

struct CollectionEntry {
    std::string id;
    std::string name;
    std::string description;
    bool hidden = false;
};

// 成就、结局统一收集入口。新内容先注册，再在达成处调用 unlock。
class CollectionSystem {
public:
    CollectionSystem();

    void registerEnding(const CollectionEntry& entry);
    void registerAchievement(const CollectionEntry& entry);

    bool unlockEnding(const std::string& endingId, WorldState& world) const;
    bool unlockAchievement(const std::string& achievementId,
                           WorldState& world) const;
    // 兼容旧存档：把现有旧旗标补录进新收集系统。
    void syncLegacyFlags(WorldState& world) const;

    bool isEndingUnlocked(const std::string& endingId,
                          const WorldState& world) const;
    bool isAchievementUnlocked(const std::string& achievementId,
                               const WorldState& world) const;

    int unlockedEndingCount(const WorldState& world) const;
    int unlockedAchievementCount(const WorldState& world) const;
    std::string getEndingCollectionText(const WorldState& world) const;
    std::string getAchievementCollectionText(const WorldState& world) const;

    const std::vector<CollectionEntry>& endings() const;
    const std::vector<CollectionEntry>& achievements() const;

};

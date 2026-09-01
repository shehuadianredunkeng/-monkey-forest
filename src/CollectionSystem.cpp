#include "CollectionSystem.h"

#include "WorldState.h"

#include <algorithm>
#include <sstream>

namespace {
std::vector<CollectionEntry>& endingRegistry() {
    static std::vector<CollectionEntry> entries;
    return entries;
}

std::vector<CollectionEntry>& achievementRegistry() {
    static std::vector<CollectionEntry> entries;
    return entries;
}

std::string collectionFlag(const std::string& kind, const std::string& id) {
    return "flag_collection_" + kind + "_" + id;
}

void registerUnique(std::vector<CollectionEntry>& entries,
                    const CollectionEntry& entry) {
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [&](const CollectionEntry& current) {
                                        return current.id == entry.id;
                                    });
    if (found == entries.end() && !entry.id.empty() && !entry.name.empty())
        entries.push_back(entry);
}

bool contains(const std::vector<CollectionEntry>& entries,
              const std::string& id) {
    return std::any_of(entries.begin(), entries.end(),
                       [&](const CollectionEntry& entry) {
                           return entry.id == id;
                       });
}

std::string collectionText(const std::string& title,
                           const std::vector<CollectionEntry>& entries,
                           const std::string& kind,
                           const WorldState& world) {
    int unlocked = 0;
    for (const CollectionEntry& entry : entries)
        if (world.hasFlag(collectionFlag(kind, entry.id))) ++unlocked;

    std::ostringstream out;
    out << "【" << title << "】 " << unlocked << "/" << entries.size() << '\n';
    for (const CollectionEntry& entry : entries) {
        const bool found = world.hasFlag(collectionFlag(kind, entry.id));
        out << (found ? "[已解锁] " : "[未解锁] ");
        if (!found && entry.hidden) {
            out << "？？？\n";
        } else {
            out << entry.name;
            if (!entry.description.empty()) out << "：" << entry.description;
            out << '\n';
        }
    }
    return out.str();
}
}

CollectionSystem::CollectionSystem() {
    registerEnding({"ending_resist", "青木英雄", "正面反击守住家园", false});
    registerEnding({"ending_hack", "无声胜利", "破解设备智取星猿", false});
    registerEnding({"ending_migrate", "向南的新生", "带领猴群成功迁徙", false});
    registerEnding({"ending_fail", "失落之谷", "未能守住青木谷", false});
    registerEnding({"ending_forest_fire", "放火烧山", "火势吞没青木谷", true});
    registerEnding({"ending_second_banana", "有了第一次就有第二次！", "没能摆脱香蕉诱惑", true});
    registerEnding({"ending_gluttony", "你犯下了暴食罪！", "在香蕉诱惑中倒下", true});
    registerEnding({"ending_together_forever", "双宿双飞", "与闪尾闯荡天涯", true});

    registerAchievement({"achievement_monkey_borrow", "吗喽的事怎么能叫偷呢！", "", true});
    registerAchievement({"achievement_you_fight_back", "你倒是还手啊！", "", true});
    registerAchievement({"achievement_doudou_bond", "不要小瞧你与豆豆的羁绊啊！", "", true});
    registerAchievement({"achievement_no_rice", "巧妇难为无米之炊！", "", true});
    registerAchievement({"achievement_next_line_after_forest_fire", "放火烧山的下一句", "", true});
    registerAchievement({"achievement_no_monkey_at_tree", "猴王树查无此猴", "", true});
}

void CollectionSystem::registerEnding(const CollectionEntry& entry) {
    registerUnique(endingRegistry(), entry);
}

void CollectionSystem::registerAchievement(const CollectionEntry& entry) {
    registerUnique(achievementRegistry(), entry);
}

bool CollectionSystem::unlockEnding(const std::string& endingId,
                                    WorldState& world) const {
    if (!contains(endingRegistry(), endingId)) return false;
    world.setFlag(collectionFlag("ending", endingId));
    return true;
}

bool CollectionSystem::unlockAchievement(const std::string& achievementId,
                                         WorldState& world) const {
    if (!contains(achievementRegistry(), achievementId)) return false;
    world.setFlag(collectionFlag("achievement", achievementId));
    return true;
}

void CollectionSystem::syncLegacyFlags(WorldState& world) const {
    const std::pair<const char*, const char*> endingFlags[] = {
        {"flag_choice_resist", "ending_resist"},
        {"flag_choice_hack", "ending_hack"},
        {"flag_choice_migrate", "ending_migrate"},
        {"flag_ending_fail", "ending_fail"},
        {"flag_bad_ending_forest_fire", "ending_forest_fire"},
        {"flag_bad_ending_second_banana", "ending_second_banana"},
        {"flag_bad_ending_gluttony", "ending_gluttony"},
        {"flag_hidden_ending_together_forever", "ending_together_forever"}
    };
    for (const auto& mapping : endingFlags)
        if (world.hasFlag(mapping.first)) unlockEnding(mapping.second, world);

    const std::pair<const char*, const char*> achievementFlags[] = {
        {"flag_achievement_monkey_borrow", "achievement_monkey_borrow"},
        {"flag_achievement_you_fight_back", "achievement_you_fight_back"},
        {"flag_achievement_doudou_bond", "achievement_doudou_bond"},
        {"flag_achievement_no_rice", "achievement_no_rice"},
        {"flag_achievement_next_line_after_forest_fire", "achievement_next_line_after_forest_fire"},
        {"flag_achievement_no_monkey_at_tree", "achievement_no_monkey_at_tree"}
    };
    for (const auto& mapping : achievementFlags)
        if (world.hasFlag(mapping.first)) unlockAchievement(mapping.second, world);
}

bool CollectionSystem::isEndingUnlocked(const std::string& endingId,
                                        const WorldState& world) const {
    return world.hasFlag(collectionFlag("ending", endingId));
}

bool CollectionSystem::isAchievementUnlocked(const std::string& achievementId,
                                             const WorldState& world) const {
    return world.hasFlag(collectionFlag("achievement", achievementId));
}

int CollectionSystem::unlockedEndingCount(const WorldState& world) const {
    const auto& entries = endingRegistry();
    return static_cast<int>(std::count_if(entries.begin(), entries.end(),
        [&](const CollectionEntry& entry) { return isEndingUnlocked(entry.id, world); }));
}

int CollectionSystem::unlockedAchievementCount(const WorldState& world) const {
    const auto& entries = achievementRegistry();
    return static_cast<int>(std::count_if(entries.begin(), entries.end(),
        [&](const CollectionEntry& entry) { return isAchievementUnlocked(entry.id, world); }));
}

std::string CollectionSystem::getEndingCollectionText(const WorldState& world) const {
    return collectionText("结局收集", endingRegistry(), "ending", world);
}

std::string CollectionSystem::getAchievementCollectionText(const WorldState& world) const {
    return collectionText("成就收集", achievementRegistry(), "achievement", world);
}

const std::vector<CollectionEntry>& CollectionSystem::endings() const {
    return endingRegistry();
}
const std::vector<CollectionEntry>& CollectionSystem::achievements() const {
    return achievementRegistry();
}

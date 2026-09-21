#include "CollectionSystem.h"
#include "StoryText.h"
#include "WorldState.h"
#include <algorithm>
#include <sstream>
using namespace std;

namespace {
//函数内静态变量，整个程序只有一份
vector<CollectionEntry>& endingRegistry() {
    static vector<CollectionEntry> entries;
    return entries;
}

vector<CollectionEntry>& achievementRegistry() {
    static vector<CollectionEntry> entries;
    return entries;
}

string collectionFlag(const string& kind, const string& id) {
    return "flag_collection_" + kind + "_" + id;
}

void registerUnique(vector<CollectionEntry>& entries,
                    const CollectionEntry& entry) {
    const auto found = find_if(entries.begin(), entries.end(),
                                    [&](const CollectionEntry& current) {
                                        return current.id == entry.id;
                                    });
    if (found == entries.end() && !entry.id.empty() && !entry.name.empty())
        entries.push_back(entry);
}

bool contains(const vector<CollectionEntry>& entries,
              const string& id) {
    return any_of(entries.begin(), entries.end(),
                       [&](const CollectionEntry& entry) {
                           return entry.id == id;
                       });
}

string taggedValue(const string& description,
                        const vector<string>& tags) {
    for (const string& tag : tags) {
        const size_t start = description.find(tag);
        if (start == string::npos) continue;
        const size_t valueStart = start + tag.size();
        const size_t end = description.find(']', valueStart);
        return description.substr(valueStart,
                                  end == string::npos
                                      ? string::npos
                                      : end - valueStart);
    }
    return "";
}

string collectionText(const string& title,
                           const vector<CollectionEntry>& entries,
                           const string& kind,
                           const WorldState& world,
                           bool showConditions) {
    int unlocked = 0;
    for (const CollectionEntry& entry : entries)
        if (world.hasFlag(collectionFlag(kind, entry.id))) ++unlocked;

    ostringstream out;
    out << "【" << title << "】 " << unlocked << "/" << entries.size() << '\n';
    for (const CollectionEntry& entry : entries) {
        const bool found = world.hasFlag(collectionFlag(kind, entry.id));
        out << (found ? entry.name : "？？？");
        if (showConditions) {
            const string condition = taggedValue(
                entry.description, {"[解锁条件：", "[触发条件："});
            if (!condition.empty()) out << "  [达成条件：" << condition << ']';
        }
        if (found) {
            const string evaluation = taggedValue(
                entry.description, {"[评价："});
            if (!evaluation.empty()) out << "\n  评价：" << evaluation;
        }
        out << '\n';
    }
    return out.str();
}
}

CollectionSystem::CollectionSystem() {
    for (const EndingInfo& info : allEndingInfos()) {
        registerEnding({info.id, info.name,
                        "[" + info.conditionTag + "：" + info.condition +
                            "][评价：" + info.evaluation + "]",
                        info.hidden});
    }

    registerAchievement({"achievement_monkey_borrow", "吗喽的事怎么能叫偷呢！", "[解锁条件：在不同战斗中累计偷窃成功三次][评价：你已超越79%的灵巧玩家，敌人的口袋一致给出差评。]", true});
    registerAchievement({"achievement_you_fight_back", "你倒是还手啊！", "[解锁条件：与野蜂群战斗时连续防御三次][评价：你已超越67%的急性子，蜂群都怀疑你是不是掉线了。]", true});
    registerAchievement({"achievement_doudou_bond", "不要小瞧你与豆豆的羁绊啊！", "[解锁条件：救回豆豆后在战斗中触发1%的奇迹祝福][评价：你已超越99%的幸运玩家，这不是欧气，是豆豆亲自给你开的灯。]", true});
    registerAchievement({"achievement_no_rice", "巧妇难为无米之炊！", "[解锁条件：没有草药时尝试救治豆豆][评价：这项不难，难的是你居然空着背包就来当医生。]", true});
    registerAchievement({"achievement_next_line_after_forest_fire", "放火烧山的下一句", "[解锁条件：燧石火攻导致放火烧山结局][评价：吗喽们失望地看着你，消防队正在赶来的路上。]", true});
    registerAchievement({"achievement_no_monkey_at_tree", "猴王树查无此猴", "[解锁条件：接受闪尾远行邀请][评价：你已超越97%的归队路线，点名册从此多了一条未接来电。]", true});
    registerAchievement({"achievement_last_season", "最后的季节", "[解锁条件：集齐春花、蝉蜕、秋叶和落雪][评价：你已超越99%的收藏玩家，四季轮班给你盖了章。]", true});
    registerAchievement({"achievement_all_random_events", "青木谷奇遇录", "[解锁条件：完成全部三项随机事件][评价：你已超越89%的赶路玩家，主路没少走，岔路也一条没放过。]", true});
    registerAchievement({"achievement_pacifist_log", "一拳未出", "[解锁条件：未击败任何敌人便取得完整日志][评价：你已超越95%的和平玩家，拳头没出场，脑回路加班了。]", true});
    registerAchievement({"achievement_all_routes_ready", "三路皆通", "[解锁条件：同一轮中依次完成迁徙、反击与智取三条路线准备][评价：你已超越98%的规划玩家，选择困难是因为你真的全都能选。]", true});
    registerAchievement({"achievement_combat_god_candidate", "你是战神吗？", "[解锁条件：战斗技能达到3级（累计击败至少11名敌人）][评价：你已超越90%的林间斗士，江湖上流传着你的传说！]", true});
    registerAchievement({"achievement_combat_god", "你就是战神！", "[解锁条件：战斗技能达到5级（累计击败至少26名敌人）][评价：你已超越99%的战斗玩家，这回不用问了，你就是战神。]", true});
}

void CollectionSystem::registerEnding(const CollectionEntry& entry) {
    registerUnique(endingRegistry(), entry);
}

void CollectionSystem::registerAchievement(const CollectionEntry& entry) {
    registerUnique(achievementRegistry(), entry);
}

bool CollectionSystem::unlockEnding(const string& endingId,
                                    WorldState& world) const {
    if (!contains(endingRegistry(), endingId)) return false;
    world.setFlag(collectionFlag("ending", endingId));
    return true;
}

bool CollectionSystem::unlockAchievement(const string& achievementId,
                                         WorldState& world) const {
    if (!contains(achievementRegistry(), achievementId)) return false;
    world.setFlag(collectionFlag("achievement", achievementId));
    return true;
}

void CollectionSystem::syncLegacyFlags(WorldState& world) const {
    const pair<const char*, const char*> endingFlags[] = {
        {"flag_choice_resist", "ending_resist"},
        {"flag_choice_hack", "ending_hack"},
        {"flag_choice_migrate", "ending_migrate"},
        {"flag_ending_fail", "ending_fail"},
        {"flag_bad_ending_forest_fire", "ending_forest_fire"},
        {"flag_bad_ending_second_banana", "ending_second_banana"},
        {"flag_bad_ending_gluttony", "ending_gluttony"},
        {"flag_hidden_ending_together_forever", "ending_together_forever"},
        {"flag_hidden_ending_earth_gift", "ending_earth_gift"},
        {"flag_normal_ending_not_hero", "ending_not_hero"},
        {"flag_hidden_ending_spark", "ending_spark"},
        {"flag_bad_ending_coward", "ending_coward"}
    };
    for (const auto& mapping : endingFlags)
        if (world.hasFlag(mapping.first)) unlockEnding(mapping.second, world);

    const pair<const char*, const char*> achievementFlags[] = {
        {"flag_achievement_monkey_borrow", "achievement_monkey_borrow"},
        {"flag_achievement_you_fight_back", "achievement_you_fight_back"},
        {"flag_achievement_doudou_bond", "achievement_doudou_bond"},
        {"flag_achievement_no_rice", "achievement_no_rice"},
        {"flag_achievement_next_line_after_forest_fire", "achievement_next_line_after_forest_fire"},
        {"flag_achievement_no_monkey_at_tree", "achievement_no_monkey_at_tree"},
        {"flag_achievement_last_season", "achievement_last_season"},
        {"flag_achievement_combat_god_candidate", "achievement_combat_god_candidate"},
        {"flag_achievement_combat_god", "achievement_combat_god"}
    };
    for (const auto& mapping : achievementFlags)
        if (world.hasFlag(mapping.first)) unlockAchievement(mapping.second, world);

    if (world.hasFlag("flag_event_wildfire_done") &&
        world.hasFlag("flag_event_hidden_orchard_done") &&
        world.hasFlag("flag_event_drone_crash_done")) {
        unlockAchievement("achievement_all_random_events", world);
    }

    if (world.hasFlag("flag_complete_log") &&
        !world.hasFlag("flag_bees_defeated") &&
        !world.hasFlag("flag_robot_defeated") &&
        !world.hasFlag("flag_hertz_defeated")) {
        unlockAchievement("achievement_pacifist_log", world);
    }

    if (world.hasFlag("flag_route_resist_ready") &&
        world.hasFlag("flag_route_hack_ready") &&
        world.hasFlag("flag_route_migrate_ready")) {
        unlockAchievement("achievement_all_routes_ready", world);
    }
}

bool CollectionSystem::isEndingUnlocked(const string& endingId,
                                        const WorldState& world) const {
    return world.hasFlag(collectionFlag("ending", endingId));
}

bool CollectionSystem::isAchievementUnlocked(const string& achievementId,
                                             const WorldState& world) const {
    return world.hasFlag(collectionFlag("achievement", achievementId));
}

int CollectionSystem::unlockedEndingCount(const WorldState& world) const {
    const auto& entries = endingRegistry();
    return static_cast<int>(count_if(entries.begin(), entries.end(),
        [&](const CollectionEntry& entry) { return isEndingUnlocked(entry.id, world); }));
}

string CollectionSystem::getEndingCollectionText(const WorldState& world,
                                                       bool showConditions) const {
    return collectionText("结局收集", endingRegistry(), "ending", world,
                          showConditions);
}

string CollectionSystem::getAchievementCollectionText(
    const WorldState& world, bool showConditions) const {
    return collectionText("成就收集", achievementRegistry(), "achievement",
                          world, showConditions);
}

const vector<CollectionEntry>& CollectionSystem::endings() const {
    return endingRegistry();
}
const vector<CollectionEntry>& CollectionSystem::achievements() const {
    return achievementRegistry();
}

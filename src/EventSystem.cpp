#include "EventSystem.h"

#include <random>
#include <vector>

#include "EventFactory.h"
#include "GameContext.h"
#include "Item.h"
#include "Player.h"
#include "StoryText.h"
#include "WorldState.h"

namespace {

constexpr const char* kRandomRequestId = "random";

bool isBlankEventRequest(const std::string& eventId) {
    return eventId.empty() || eventId == "current";
}

bool grantChip(Player& player) {
    return player.hasItem("item_chip") ||
           player.addItem(Item("item_chip", "芯片", true, 1));
}

}  // namespace

void EventSystem::initializeEvents() {
    events_ = createAllEvents();
    activeEventId_.clear();
}

const Event* EventSystem::findEvent(const std::string& eventId) const {
    const auto it = events_.find(eventId);
    return it == events_.end() ? nullptr : &it->second;
}

std::string EventSystem::findPendingEventId(const WorldState& world) const {
    for (const auto& [id, event] : events_) {
        (void)id;
        if (world.hasFlag(event.pendingFlag())) {
            return event.eventId;
        }
    }
    return "";
}

bool EventSystem::hasOtherPendingEvent(const Event& event,
                                       const WorldState& world) const {
    for (const auto& [id, other] : events_) {
        (void)id;
        if (other.eventId != event.eventId &&
            world.hasFlag(other.pendingFlag())) {
            return true;
        }
    }
    return false;
}

int EventSystem::countCompletedRandomEvents(const WorldState& world) const {
    int count = 0;
    for (const auto& [id, event] : events_) {
        (void)id;
        if (event.kind == EventKind::Random &&
            world.hasFlag(event.completionFlag)) {
            ++count;
        }
    }
    return count;
}

const Event* EventSystem::chooseRandomEvent(const GameContext& ctx) const {
    std::vector<const Event*> candidates;
    for (const auto& [id, event] : events_) {
        (void)id;
        if (event.kind == EventKind::Random &&
            canTriggerEvent(event.eventId, ctx)) {
            candidates.push_back(&event);
        }
    }

    if (candidates.empty()) {
        return nullptr;
    }

    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<std::size_t> distribution(
        0, candidates.size() - 1);
    return candidates[distribution(generator)];
}

bool EventSystem::canTriggerEvent(const std::string& eventId,
                                  const GameContext& ctx) const {
    if (events_.empty()) {
        return false;
    }
    if (eventId == kRandomRequestId) {
        return chooseRandomEvent(ctx) != nullptr;
    }

    const Event* event = findEvent(eventId);
    if (event == nullptr) {
        return false;
    }
    if (ctx.world.getStage() < event->minStage ||
        ctx.world.getStage() > event->maxStage) {
        return false;
    }
    if (!event->requiredRoomId.empty() &&
        ctx.player.getCurrentRoomId() != event->requiredRoomId) {
        return false;
    }
    if (ctx.world.hasFlag(event->completionFlag)) {
        return false;
    }
    for (const std::string& flag : event->requiredFlags) {
        if (!ctx.world.hasFlag(flag)) {
            return false;
        }
    }
    if (hasOtherPendingEvent(*event, ctx.world)) {
        return false;
    }
    if (event->kind == EventKind::Random &&
        !ctx.world.hasFlag(event->pendingFlag()) &&
        countCompletedRandomEvents(ctx.world) >= 2) {
        return false;
    }
    return true;
}

ActionResult EventSystem::triggerEvent(const std::string& eventId,
                                       GameContext& ctx) {
    if (events_.empty()) {
        return makeResult(false,
                          "事件系统尚未初始化，请先调用 initializeEvents()。");
    }

    const Event* event = eventId == kRandomRequestId
                             ? chooseRandomEvent(ctx)
                             : findEvent(eventId);
    if (event == nullptr) {
        return makeResult(false,
                          eventId == kRandomRequestId
                              ? "当前没有满足条件的随机事件。"
                              : "不存在事件：" + eventId + "。");
    }
    if (!canTriggerEvent(event->eventId, ctx)) {
        return makeResult(false,
                          "当前阶段、地点或前置条件不允许触发【" +
                              event->title + "】。");
    }

    activeEventId_ = event->eventId;
    ctx.world.setFlag(event->pendingFlag());
    return makeResult(true, event->formatPrompt());
}

ActionResult EventSystem::chooseEventOption(const std::string& eventId,
                                            int option,
                                            GameContext& ctx) {
    if (events_.empty()) {
        return makeResult(false,
                          "事件系统尚未初始化，请先调用 initializeEvents()。");
    }

    std::string resolvedId = eventId;
    if (isBlankEventRequest(resolvedId)) {
        resolvedId = activeEventId_;
    }
    if (resolvedId.empty()) {
        resolvedId = findPendingEventId(ctx.world);
    }

    const Event* event = findEvent(resolvedId);
    if (event == nullptr) {
        return makeResult(false,
                          "当前没有等待选择的事件，请先使用 investigate。");
    }
    if (!ctx.world.hasFlag(event->pendingFlag())) {
        return makeResult(false,
                          "事件【" + event->title +
                              "】尚未触发，不能直接选择选项。");
    }
    if (option < 1 || option > static_cast<int>(event->choices.size())) {
        return makeResult(false,
                          "无效选项，请输入 1 到 " +
                              std::to_string(event->choices.size()) + "。");
    }

    activeEventId_ = event->eventId;
    return resolveChoice(*event, option, ctx);
}

std::string EventSystem::getStageIntroduction(int stage) const {
    return getStageIntroductionText(stage);
}

std::string EventSystem::getEndingText(const std::string& endingId) const {
    return getEndingTextById(endingId);
}

ActionResult EventSystem::resolveChoice(const Event& event,
                                        int option,
                                        GameContext& ctx) {
    Player& player = ctx.player;
    WorldState& world = ctx.world;

    if (event.eventId == "event_tree_trial") {
        if (option == 1) {
            if (!world.hasFlag("flag_bees_defeated")) {
                world.setFlag("flag_pending_battle_bees");
                return makeResult(
                    true,
                    "野蜂群从树洞中涌出，密集的翅声瞬间盖过林间风声！"
                    "击败野蜂后，请再次选择第一项带走果实。");
            }
            world.removeFlag("flag_pending_battle_bees");
            player.changeStamina(-8);
            player.changeReputation(5);
            world.changeResource(ResourceType::Food, 3);
            return completeEvent(
                event,
                "你击退野蜂，带回三份果实。体力-8，声望+5，公共食物+3。",
                ctx);
        }
        if (option == 2) {
            if (player.getStamina() < 10) {
                return makeResult(false, "体力至少需要10点才能攀上树冠。");
            }
            player.changeStamina(-10);
            player.changeReputation(4);
            if (player.getSkillLevel(SkillType::Climb) >= 2) {
                world.changeResource(ResourceType::Food, 4);
                return completeEvent(
                    event,
                    "你熟练地绕过蜂巢，带回四份果实。体力-10，声望+4，公共食物+4。",
                    ctx);
            }
            player.changeHealth(-5);
            world.changeResource(ResourceType::Food, 3);
            return completeEvent(
                event,
                "你采到三份果实，但被枯枝划伤。体力-10，生命-5，声望+4，公共食物+3。",
                ctx);
        }
        if (player.getStamina() < 8) {
            return makeResult(false, "体力至少需要8点才能完成采集。");
        }
        player.changeStamina(-8);
        player.changeWisdom(1);
        player.changeReputation(3);
        world.changeResource(ResourceType::Food, 2);
        return completeEvent(
            event,
            "你通过观察找到了安全路线。体力-8，智慧+1，声望+3，公共食物+2。",
            ctx);
    }

    if (event.eventId == "event_winter_shortage") {
        if (option == 1) {
            if (world.getResource(ResourceType::Food) < 2) {
                return makeResult(false, "公共食物不足2份，无法执行分粮方案。");
            }
            world.changeResource(ResourceType::Food, -2);
            world.changeResource(ResourceType::Morale, 10);
            player.changeReputation(8);
            return completeEvent(
                event,
                "幼猴们安全度过寒夜。公共食物-2，士气+10，声望+8。",
                ctx);
        }
        if (option == 2) {
            if (player.getStamina() < 15) {
                return makeResult(false, "体力至少需要15点才能冒雪采集。");
            }
            player.changeStamina(-15);
            player.changeHealth(-5);
            player.changeReputation(5);
            world.changeResource(ResourceType::Food, 3);
            return completeEvent(
                event,
                "你在雪中找到备用果实。体力-15，生命-5，声望+5，公共食物+3。",
                ctx);
        }
        player.changeStamina(8);
        player.changeReputation(-5);
        world.changeResource(ResourceType::Morale, -8);
        return completeEvent(
            event,
            "食物暂时保住了，但族群对你的决定感到失望。体力+8，声望-5，士气-8。",
            ctx);
    }

    if (event.eventId == "event_glowing_river") {
        if (option == 1) {
            if (player.getStrength() < 2 || player.getStamina() < 10) {
                return makeResult(false,
                                  "该方案需要力量至少2点且体力至少10点。");
            }
            player.changeStamina(-10);
            player.changeReputation(5);
            world.changeResource(ResourceType::Water, 4);
            world.setFlag("flag_water_fixed");
            return completeEvent(
                event,
                "你搬开岩石恢复了水流。体力-10，声望+5，公共水源+4。",
                ctx);
        }
        if (option == 2) {
            if (player.getStamina() < 10) {
                return makeResult(false, "体力至少需要10点才能追踪管线。");
            }
            const bool understoodDevice = player.getWisdom() >= 2;
            player.changeStamina(-10);
            if (understoodDevice) {
                player.changeWisdom(1);
            } else {
                player.changeHealth(-5);
            }
            world.changeResource(ResourceType::Water, 3);
            world.setFlag("flag_water_fixed");
            world.setFlag("flag_water_clue");
            return completeEvent(
                event,
                understoodDevice
                    ? "你关闭了临时抽水阀并记住管线方向。体力-10，智慧+1，公共水源+3。"
                    : "你勉强关闭抽水阀，却被设备灼伤。体力-10，生命-5，公共水源+3。",
                ctx);
        }
        if (player.getReputation() < 20) {
            return makeResult(false, "该方案需要声望至少20点。");
        }
        player.changeReputation(5);
        world.changeResource(ResourceType::Water, 5);
        world.changeResource(ResourceType::Morale, 5);
        world.setFlag("flag_water_fixed");
        return completeEvent(
            event,
            "猴群共同挖通新水道。声望+5，公共水源+5，士气+5。",
            ctx);
    }

    if (event.eventId == "event_echo_tracking") {
        if (option == 1) {
            if (player.getStamina() < 12) {
                return makeResult(false,
                                  "体力至少需要12点才能完成山洞追踪。");
            }
            if (!grantChip(player)) {
                return makeResult(false,
                                  "背包已满，无法安全取走星猿晶片。");
            }
            player.changeStamina(-12);
            if (player.getWisdom() < 2) {
                player.changeHealth(-5);
            } else {
                player.changeWisdom(1);
            }
        } else if (option == 2) {
            if (player.getSkillLevel(SkillType::Climb) < 2) {
                return makeResult(false, "该路线需要攀爬技能至少2级。");
            }
            if (!grantChip(player)) {
                return makeResult(false,
                                  "背包已满，无法安全取走星猿晶片。");
            }
            player.changeStamina(-6);
            player.changeReputation(4);
        } else {
            if (!world.hasFlag("flag_scout_help")) {
                return makeResult(false,
                                  "闪尾尚未答应协助，请先完成其任务或提高声望。");
            }
            if (!grantChip(player)) {
                return makeResult(false,
                                  "背包已满，无法安全取走星猿晶片。");
            }
            player.changeStamina(-4);
            player.changeReputation(5);
            world.setFlag("flag_new_home_found");
        }

        world.setFlag("flag_chip_found");
        world.setFlag("flag_base_clue_found");
        return completeEvent(
            event,
            "你取得星猿晶片，并发现实验基地入口。晶片已放入背包。",
            ctx);
    }

    if (event.eventId == "event_drought_choice") {
        if (option == 1) {
            if (player.getSkillLevel(SkillType::Combat) < 1) {
                return makeResult(false,
                                  "至少需要1级战斗技能才能组织守卫训练。");
            }
            player.changeStrength(1);
            player.changeReputation(5);
            world.changeResource(ResourceType::Morale, 5);
            world.setFlag("flag_route_resist_ready");
            return completeEvent(
                event,
                "你组织守卫训练。力量+1，声望+5，士气+5，反击准备完成。",
                ctx);
        }
        if (option == 2) {
            if (player.getWisdom() < 2) {
                return makeResult(false, "研究晶片需要智慧至少2点。");
            }
            player.changeWisdom(1);
            world.setFlag("flag_route_hack_ready");
            return completeEvent(
                event,
                "你破译了部分控制协议。智慧+1，智取路线准备完成。",
                ctx);
        }
        if (player.getStamina() < 10) {
            return makeResult(false, "寻找新家园需要体力至少10点。");
        }
        player.changeStamina(-10);
        player.changeReputation(5);
        world.changeResource(ResourceType::MigrationSupply, 4);
        world.setFlag("flag_new_home_found");
        world.setFlag("flag_route_migrate_ready");
        return completeEvent(
            event,
            "你在上游找到可居住的新河谷。体力-10，声望+5，迁徙物资+4。",
            ctx);
    }

    if (event.eventId == "event_group_dispute") {
        if (option == 1) {
            if (player.getSkillLevel(SkillType::Leadership) < 1 &&
                player.getReputation() < 20) {
                return makeResult(false,
                                  "调解需要领导技能至少1级或声望至少20点。");
            }
            player.changeReputation(10);
            world.changeResource(ResourceType::Morale, 10);
            world.setFlag("flag_scout_help");
        } else if (option == 2) {
            if (player.getSkillLevel(SkillType::Combat) < 1) {
                return makeResult(false,
                                  "说服族群反击需要战斗技能至少1级。");
            }
            player.changeReputation(6);
            world.changeResource(ResourceType::Morale, 6);
            world.setFlag("flag_route_resist_ready");
        } else {
            player.changeReputation(5);
            world.changeResource(ResourceType::MigrationSupply, 4);
            world.setFlag("flag_new_home_found");
            world.setFlag("flag_route_migrate_ready");
        }

        world.setFlag("flag_base_open");
        return completeEvent(
            event,
            "争执平息，猴群同意执行你的准备方案。实验基地路线已经开放。",
            ctx);
    }

    if (event.eventId == "event_base_infiltration") {
        if (option == 1) {
            if (!world.hasFlag("flag_robot_defeated")) {
                world.setFlag("flag_pending_battle_robot");
                return makeResult(
                    true,
                    "巡逻机的红色光束锁定了你，能源室的警报随即响起！"
                    "击败巡逻机后，请再次选择第一项取得完整日志。");
            }
            world.removeFlag("flag_pending_battle_robot");
            player.changeReputation(8);
            world.setFlag("flag_complete_log");
            return completeEvent(
                event,
                "巡逻机停止运转，你取得完整抽取日志。声望+8。",
                ctx);
        }
        if (option == 2) {
            if (!world.hasFlag("flag_chip_found") ||
                !player.hasItem("item_chip") ||
                player.getWisdom() < 3) {
                return makeResult(false,
                                  "伪装权限需要背包中的星猿晶片且智慧至少3点。");
            }
            player.changeWisdom(1);
            world.setFlag("flag_complete_log");
            return completeEvent(
                event,
                "晶片骗过了控制台，你下载了完整日志。智慧+1。",
                ctx);
        }
        if (!world.hasFlag("flag_scout_help") &&
            player.getReputation() < 30) {
            return makeResult(false,
                              "该方案需要闪尾协助或声望至少30点。");
        }
        player.changeReputation(8);
        world.setFlag("flag_scout_help");
        world.setFlag("flag_complete_log");
        return completeEvent(
            event,
            "闪尾引开巡逻机，你成功取得完整日志。声望+8。",
            ctx);
    }

    if (event.eventId == "event_final_choice") {
        if (option == 1) {
            if (player.getReputation() < 60 ||
                player.getSkillLevel(SkillType::Combat) < 2) {
                return makeResult(false,
                                  "反击路线需要声望至少60且战斗技能至少2级。");
            }
            if (!world.hasFlag("flag_hertz_defeated")) {
                world.setFlag("flag_pending_battle_hertz");
                return makeResult(
                    true,
                    "你发出反击的号令，带领猴群冲向赫兹，"
                    "守卫青木谷的最终决战就此开始！"
                    "击败赫兹后，请再次选择第一项完成反击路线。");
            }
            world.removeFlag("flag_pending_battle_hertz");
            world.setFlag("flag_choice_resist");
            world.setFlag("flag_final_choice");
            return completeEvent(
                event,
                "赫兹被击败，抽取设备停止运转。已锁定反击结局。",
                ctx);
        }
        if (option == 2) {
            if (player.getWisdom() < 4 ||
                !world.hasFlag("flag_complete_log")) {
                return makeResult(false,
                                  "智取路线需要智慧至少4点且拥有完整日志。");
            }
            world.setFlag("flag_system_hacked");
            world.setFlag("flag_choice_hack");
            world.setFlag("flag_final_choice");
            return completeEvent(
                event,
                "你改写了抽取程序，星猿设备全部停机。已锁定智取结局。",
                ctx);
        }
        if (world.getResource(ResourceType::MigrationSupply) < 8 ||
            player.getSkillLevel(SkillType::Leadership) < 2 ||
            !world.hasFlag("flag_new_home_found")) {
            return makeResult(
                false,
                "迁徙路线需要迁徙物资至少8、领导技能至少2级，并找到新家园。");
        }
        world.setFlag("flag_choice_migrate");
        world.setFlag("flag_final_choice");
        return completeEvent(
            event,
            "猴群带着物资离开青木谷。已锁定迁徙结局。",
            ctx);
    }

    if (event.eventId == "event_wildfire") {
        if (option == 1) {
            if (!player.hasItem("item_flint")) {
                return makeResult(false,
                                  "开辟隔火带需要物品 item_flint。");
            }
            player.changeStamina(-8);
            player.changeReputation(8);
            world.changeResource(ResourceType::Morale, 8);
            world.setFlag("flag_fire_controlled");
            return completeEvent(
                event,
                "隔火带挡住了火势。体力-8，声望+8，士气+8。",
                ctx);
        }
        if (option == 2) {
            if (world.getResource(ResourceType::Water) < 2 ||
                player.getStamina() < 12) {
                return makeResult(false,
                                  "组织灭火需要公共水源至少2且体力至少12点。");
            }
            player.changeStamina(-12);
            player.changeReputation(10);
            world.changeResource(ResourceType::Water, -2);
            world.changeResource(ResourceType::Morale, 10);
            world.setFlag("flag_fire_controlled");
            return completeEvent(
                event,
                "猴群合力扑灭山火。体力-12，水源-2，声望+10，士气+10。",
                ctx);
        }
        player.changeStamina(5);
        player.changeReputation(-5);
        world.changeResource(ResourceType::Food, -2);
        world.changeResource(ResourceType::Morale, -10);
        world.setFlag("flag_forest_burned");
        return completeEvent(
            event,
            "你保住了自己，但果林遭到破坏。体力+5，声望-5，食物-2，士气-10。",
            ctx);
    }

    if (event.eventId == "event_injured_child") {
        if (option == 1) {
            if (!player.hasItem("item_herb")) {
                return makeResult(false, "背包中没有 item_herb。");
            }
            player.removeItem("item_herb");
            player.changeReputation(6);
            world.changeResource(ResourceType::Morale, 4);
            world.setFlag("flag_child_found");
            return completeEvent(
                event,
                "草药止住了伤口。你已找到豆豆，请再与他交谈并送他回猴群。声望+6，士气+4。",
                ctx);
        }
        if (option == 2) {
            if (player.getStamina() < 12) {
                return makeResult(false, "背回豆豆需要体力至少12点。");
            }
            player.changeStamina(-12);
            player.changeHealth(-5);
            player.changeReputation(5);
            world.setFlag("flag_child_found");
            return completeEvent(
                event,
                "你带豆豆脱离危险。请再与他交谈完成护送。体力-12，生命-5，声望+5。",
                ctx);
        }
        player.changeReputation(4);
        world.changeResource(ResourceType::Morale, 3);
        world.setFlag("flag_healer_called");
        world.setFlag("flag_child_found");
        return completeEvent(
            event,
            "叶婆婆及时赶到。请再与豆豆交谈完成护送。声望+4，士气+3。",
            ctx);
    }

    if (event.eventId == "event_hidden_orchard") {
        if (option == 1) {
            player.changeReputation(6);
            world.changeResource(ResourceType::Food, 4);
            world.changeResource(ResourceType::Morale, 8);
            world.setFlag("flag_orchard_shared");
            return completeEvent(
                event,
                "猴群共享果园。公共食物+4，士气+8，声望+6。",
                ctx);
        }
        if (option == 2) {
            player.changeReputation(4);
            world.changeResource(ResourceType::Food, 3);
            world.changeResource(ResourceType::Morale, 3);
            world.setFlag("flag_orchard_protected");
            return completeEvent(
                event,
                "你只采摘成熟果实并保护幼树。公共食物+3，士气+3，声望+4。",
                ctx);
        }
        player.changeStamina(15);
        player.changeReputation(-5);
        world.changeResource(ResourceType::Morale, -8);
        world.setFlag("flag_orchard_hidden");
        return completeEvent(
            event,
            "你独自吃下果实。体力+15，声望-5，士气-8。",
            ctx);
    }

    if (event.eventId == "event_drone_crash") {
        if (option == 1) {
            if (player.getWisdom() < 2) {
                return makeResult(false, "拆解核心需要智慧至少2点。");
            }
            if (!grantChip(player)) {
                return makeResult(false,
                                  "背包已满，无法安全取走星猿晶片。");
            }
            player.changeWisdom(1);
            world.setFlag("flag_chip_found");
            world.setFlag("flag_drone_analyzed");
            return completeEvent(
                event,
                "你读出部分星猿数据并取得晶片。智慧+1，晶片已放入背包。",
                ctx);
        }
        if (option == 2) {
            if (!grantChip(player)) {
                return makeResult(false,
                                  "背包已满，无法安全取走星猿晶片。");
            }
            player.changeReputation(3);
            world.setFlag("flag_chip_found");
            world.setFlag("flag_drone_buried");
            return completeEvent(
                event,
                "你取走晶片并埋好残骸。声望+3，晶片已放入背包。",
                ctx);
        }
        if (player.getStamina() < 8) {
            return makeResult(false, "设置假痕迹需要体力至少8点。");
        }
        player.changeStamina(-8);
        player.changeReputation(5);
        world.setFlag("flag_drone_decoy");
        return completeEvent(
            event,
            "假痕迹把星猿巡逻队引向远处。体力-8，声望+5。",
            ctx);
    }

    return makeResult(false,
                      "事件缺少选项处理逻辑：" + event.eventId);
}

ActionResult EventSystem::completeEvent(const Event& event,
                                        const std::string& message,
                                        GameContext& ctx,
                                        bool turnConsumed) {
    ctx.world.setFlag(event.completionFlag);
    ctx.world.removeFlag(event.pendingFlag());
    if (activeEventId_ == event.eventId) {
        activeEventId_.clear();
    }
    return makeResult(true,
                      message,
                      turnConsumed,
                      event.completesStage);
}

ActionResult EventSystem::makeResult(bool success,
                                     const std::string& message,
                                     bool turnConsumed,
                                     bool stageCompleted) {
    return {success, message, turnConsumed, stageCompleted};
}

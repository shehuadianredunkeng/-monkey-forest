#include "EventSystem.h"

#include <random>
#include <vector>

#include "EventFactory.h"
#include "GameContext.h"
#include "Item.h"
#include "Player.h"
#include "Room.h"
#include "StoryText.h"
#include "WorldState.h"

namespace {

constexpr const char* kRandomRequestId = "random";

const char* const kMainEventOrder[] = {
    "event_tree_trial",
    "event_winter_shortage",
    "event_glowing_river",
    "event_echo_tracking",
    "event_drought_choice",
    "event_group_dispute",
    "event_base_infiltration",
    "event_final_choice"
};

bool isBlankEventRequest(const std::string& eventId) {
    return eventId.empty() || eventId == "current";
}

bool grantChip(Player& player) {
    return player.hasItem("item_chip") ||
           player.addItem(Item("item_chip", "星猿晶片", true, 1));
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
    if (ctx.world.hasFlag(event->pendingFlag())) {
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
    return true;
}

const Event* EventSystem::findRecommendedMainEvent(
    const GameContext& ctx,
    bool requireCurrentRoom) const {
    for (const char* eventId : kMainEventOrder) {
        const Event* event = findEvent(eventId);
        if (event == nullptr || event->kind != EventKind::Main) {
            continue;
        }
        if (ctx.world.getStage() < event->minStage ||
            ctx.world.getStage() > event->maxStage ||
            ctx.world.hasFlag(event->completionFlag)) {
            continue;
        }
        if (requireCurrentRoom && !event->requiredRoomId.empty() &&
            ctx.player.getCurrentRoomId() != event->requiredRoomId) {
            continue;
        }

        bool requirementsMet = true;
        for (const std::string& flag : event->requiredFlags) {
            if (!ctx.world.hasFlag(flag)) {
                requirementsMet = false;
                break;
            }
        }
        if (requirementsMet) {
            return event;
        }
    }
    return nullptr;
}

ActionResult EventSystem::triggerEvent(const std::string& eventId,
                                       GameContext& ctx) {
    if (events_.empty()) {
        return makeResult(false, "事件系统尚未准备完成，请重新开始游戏。");
    }

    std::string resolvedId = eventId;
    if (resolvedId == kRandomRequestId) {
        const std::string pendingId = findPendingEventId(ctx.world);
        if (!pendingId.empty()) {
            resolvedId = pendingId;
        }
    }

    const Event* event = resolvedId == kRandomRequestId
                             ? chooseRandomEvent(ctx)
                             : findEvent(resolvedId);
    if (event == nullptr) {
        return makeResult(false,
                          resolvedId == kRandomRequestId
                              ? "当前地点没有新的随机事件。请前往其他地点继续探索，"
                                "之后可再次输入“随机（random）”。"
                              : "没有找到对应事件，请输入“指引（guide）”查看当前主线。");
    }
    if (ctx.world.hasFlag(event->pendingFlag())) {
        activeEventId_ = event->eventId;
        return makeResult(true, event->formatPrompt());
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
        return makeResult(false, "事件系统尚未准备完成，请重新开始游戏。");
    }

    std::string resolvedId = eventId;
    if (isBlankEventRequest(resolvedId)) {
        const std::string pendingId = findPendingEventId(ctx.world);
        resolvedId = pendingId.empty() ? activeEventId_ : pendingId;
    }

    const Event* event = findEvent(resolvedId);
    if (event == nullptr) {
        return makeResult(false,
                          "当前没有等待选择的事件。请到目标地点触发主线，"
                          "或输入“调查（investigate）”重新查看事件。");
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

ActionResult EventSystem::triggerAvailableMainEvent(GameContext& ctx) {
    if (events_.empty()) {
        return makeResult(false, "事件系统尚未准备完成，请重新开始游戏。");
    }

    const std::string pendingId = findPendingEventId(ctx.world);
    if (!pendingId.empty()) {
        const Event* pending = findEvent(pendingId);
        if (pending != nullptr &&
            (pending->requiredRoomId.empty() ||
             pending->requiredRoomId == ctx.player.getCurrentRoomId())) {
            return triggerEvent(pendingId, ctx);
        }
        return makeResult(false, "");
    }

    const Event* event = findRecommendedMainEvent(ctx, true);
    if (event == nullptr || !canTriggerEvent(event->eventId, ctx)) {
        return makeResult(false, "");
    }
    return triggerEvent(event->eventId, ctx);
}

ActionResult EventSystem::resumePendingEventAfterBattle(GameContext& ctx) {
    const bool beesReady =
        ctx.world.hasFlag("flag_pending_battle_bees") &&
        ctx.world.hasFlag("flag_bees_defeated");
    const bool robotReady =
        ctx.world.hasFlag("flag_pending_battle_robot") &&
        ctx.world.hasFlag("flag_robot_defeated");
    const bool hertzReady =
        ctx.world.hasFlag("flag_pending_battle_hertz") &&
        ctx.world.hasFlag("flag_hertz_defeated");

    if (!beesReady && !robotReady && !hertzReady) {
        return makeResult(false, "");
    }
    return chooseEventOption("", 1, ctx);
}

std::string EventSystem::getCurrentObjective(const GameContext& ctx) const {
    if (events_.empty()) {
        return "主线系统尚未准备完成，请重新开始游戏。";
    }

    const std::string pendingId = findPendingEventId(ctx.world);
    if (!pendingId.empty()) {
        const Event* pending = findEvent(pendingId);
        if (pending != nullptr) {
            return "当前事件：【" + pending->title +
                   "】正在等待选择。输入“调查（investigate）”可重新查看，"
                   "再输入“选择（choose）+编号”继续。";
        }
    }

    if (ctx.world.hasFlag("flag_final_choice")) {
        return "六阶段主线已经完成，猴群的命运已由你的最终选择决定。";
    }

    const Event* event = findRecommendedMainEvent(ctx, false);
    if (event == nullptr) {
        return "当前阶段的前置条件尚未完成，请继续探索、与同伴对话或检查已有任务。";
    }

    std::string targetName = event->requiredRoomId;
    const auto room = ctx.rooms.find(event->requiredRoomId);
    if (room != ctx.rooms.end()) {
        targetName = room->second.getName();
    }

    std::string text =
        "当前主线：【" + event->title + "】\n目标地点：" + targetName;
    if (ctx.player.getCurrentRoomId() == event->requiredRoomId) {
        text += "\n你已经到达目标地点，事件会自动展示；"
                "也可以输入“调查（investigate）”重新查看。";
    } else {
        text += "\n请先前往目标地点，进入房间后事件会自动展示。";
    }

    bool hasRandomHere = false;
    for (const auto& [id, candidate] : events_) {
        (void)id;
        if (candidate.kind == EventKind::Random &&
            canTriggerEvent(candidate.eventId, ctx)) {
            hasRandomHere = true;
            break;
        }
    }
    if (hasRandomHere) {
        text += "\n支线提示：附近似乎有异常，可输入“随机（random）”进行探索。";
    }
    return text;
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
                    "战斗即将开始。请先击退野蜂，再继续处理树冠试炼。");
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
            player.changeStamina(-10);
            player.changeWisdom(1);
            world.changeResource(ResourceType::Water, 3);
            world.setFlag("flag_water_fixed");
            world.setFlag("flag_water_clue");
            return completeEvent(
                event,
                "你顺着管线的震动和水声推断出它正在持续抽走地下水，"
                "并关闭了临时抽水阀。体力-10，智慧+1，公共水源+3。",
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
            player.changeWisdom(1);
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
            option == 1
                ? "你把石壁刻痕与三段回声联系起来，读懂了前人留下的方向规律。"
                  "智慧+1。你取得星猿晶片，并发现实验基地入口。"
                : "你取得星猿晶片，并发现实验基地入口。晶片已放入背包。",
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
            const bool firstResearch =
                !world.hasFlag("flag_drone_analyzed");
            if (firstResearch) {
                player.changeWisdom(1);
            }
            world.setFlag("flag_route_hack_ready");
            return completeEvent(
                event,
                firstResearch
                    ? "你反复观察晶片纹路与响应方式，破译了部分控制协议。"
                      "智慧+1，智取路线准备完成。"
                    : "你已经从侦察机核心理解过这套技术，本次没有重复获得智慧，"
                      "但智取路线准备已经完成。",
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
        std::string routeResult;
        if (option == 1) {
            if (player.getSkillLevel(SkillType::Leadership) < 1 &&
                player.getReputation() < 20) {
                return makeResult(false,
                                  "调解需要领导技能至少1级或声望至少20点。");
            }
            player.changeReputation(10);
            world.changeResource(ResourceType::Morale, 10);
            world.setFlag("flag_scout_help");
            routeResult =
                "你让双方暂时放下争执，并约定依据基地证据再作决定。"
                "声望+10，士气+10，闪尾同意协助潜入。";
        } else if (option == 2) {
            if (player.getSkillLevel(SkillType::Combat) < 1) {
                return makeResult(false,
                                  "说服族群反击需要战斗技能至少1级。");
            }
            player.changeReputation(6);
            world.changeResource(ResourceType::Morale, 6);
            world.setFlag("flag_route_resist_ready");
            routeResult =
                "你用守卫训练的成果说服族群继续反击准备。"
                "声望+6，士气+6，反击路线准备完成。";
        } else {
            player.changeReputation(5);
            world.changeResource(ResourceType::MigrationSupply, 4);
            world.setFlag("flag_new_home_found");
            world.setFlag("flag_route_migrate_ready");
            routeResult =
                "你说服族群把后路变成真正的生存方案。"
                "声望+5，迁徙物资+4，迁徙路线准备完成。";
        }

        world.setFlag("flag_base_open");
        return completeEvent(
            event,
            routeResult + "实验基地路线已经开放。",
            ctx);
    }

    if (event.eventId == "event_base_infiltration") {
        if (option == 1) {
            if (!world.hasFlag("flag_robot_defeated")) {
                world.setFlag("flag_pending_battle_robot");
                return makeResult(
                    true,
                    "巡逻机的红色光束锁定了你，能源室的警报随即响起！"
                    "战斗即将开始。请先击败巡逻机，再继续取得完整日志。");
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
            if (!world.hasFlag("flag_route_resist_ready") ||
                player.getReputation() < 60 ||
                player.getSkillLevel(SkillType::Combat) < 2) {
                return makeResult(false,
                                  "反击路线需要先完成反击准备，并达到声望60、战斗技能2级。");
            }
            if (!world.hasFlag("flag_hertz_defeated")) {
                world.setFlag("flag_pending_battle_hertz");
                return makeResult(
                    true,
                    "你发出反击的号令，带领猴群冲向赫兹，"
                    "守卫青木谷的最终决战就此开始！"
                    "击败赫兹后，反击路线将进入最终结局。");
            }
            world.removeFlag("flag_pending_battle_hertz");
            world.setFlag("flag_choice_resist");
            world.setFlag("flag_final_choice");
            return completeEvent(
                event,
                "赫兹倒在停止运转的抽取塔前。此前组织的守卫与获得的族群信任"
                "让猴群守住了青木谷，反击结局已经达成。",
                ctx);
        }
        if (option == 2) {
            if (!world.hasFlag("flag_route_hack_ready") ||
                player.getWisdom() < 4 ||
                !world.hasFlag("flag_complete_log")) {
                return makeResult(false,
                                  "智取路线需要先完成星猿技术研究，并达到智慧4、取得完整日志。");
            }
            world.setFlag("flag_system_hacked");
            world.setFlag("flag_choice_hack");
            world.setFlag("flag_final_choice");
            return completeEvent(
                event,
                "你把一路读懂的管线规律、晶片结构和完整日志写入控制程序。"
                "抽取设备在没有交战的情况下全部停机，智取结局已经达成。",
                ctx);
        }
        if (!world.hasFlag("flag_route_migrate_ready") ||
            world.getResource(ResourceType::MigrationSupply) < 8 ||
            player.getSkillLevel(SkillType::Leadership) < 2 ||
            !world.hasFlag("flag_new_home_found")) {
            return makeResult(
                false,
                "迁徙路线需要先完成迁徙准备，备齐8份物资、达到领导技能2级，"
                "并找到新家园。");
        }
        world.setFlag("flag_choice_migrate");
        world.setFlag("flag_final_choice");
        return completeEvent(
            event,
            "你按照此前勘察的路线分配物资、护送老幼离开。猴群不是仓促逃亡，"
            "而是有准备地前往新河谷，迁徙结局已经达成。",
            ctx);
    }

    if (event.eventId == "event_wildfire") {
        if (option == 1) {
            if (!player.hasItem("item_flint")) {
                return makeResult(false, "开辟隔火带需要一块燧石。");
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
            if (!grantChip(player)) {
                return makeResult(false,
                                  "背包已满，无法安全取走星猿晶片。");
            }
            const bool firstResearch =
                !world.hasFlag("flag_route_hack_ready");
            if (firstResearch) {
                player.changeWisdom(1);
            }
            world.setFlag("flag_chip_found");
            world.setFlag("flag_drone_analyzed");
            return completeEvent(
                event,
                firstResearch
                    ? "你拆解侦察机核心，读懂了部分星猿数据。"
                      "智慧+1，晶片已放入背包。"
                    : "你已经通过晶片研究理解过这套技术，本次没有重复获得智慧，"
                      "但仍取得了侦察机核心中的晶片。",
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

    return makeResult(false, "这个选项暂时无法执行，请选择其他方案。");
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
    std::string playerMessage = message;
    if (event.kind == EventKind::Random) {
        playerMessage +=
            "\n随机事件已经结束。下一步可输入“指引（guide）”查看主线目标。";
    } else if (event.eventId != "event_final_choice") {
        playerMessage +=
            "\n下一步可输入“指引（guide）”查看新的主线目标。";
    }
    return makeResult(true,
                      playerMessage,
                      turnConsumed,
                      event.completesStage);
}

ActionResult EventSystem::makeResult(bool success,
                                     const std::string& message,
                                     bool turnConsumed,
                                     bool stageCompleted) {
    return {success, message, turnConsumed, stageCompleted};
}

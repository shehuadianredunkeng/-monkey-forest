#include "NPCSystem.h"

#include "Player.h"
#include "WorldState.h"

namespace {
constexpr const char* kScoutQuest = "flag_scout_quest_complete";
constexpr const char* kChildQuest = "flag_child_rescued";
constexpr const char* kHealerQuest = "flag_healer_supplied";
constexpr const char* kKingSupport = "flag_king_support";
}

void NPCSystem::initializeNPCs() {
    npcs_.clear();
    npcs_.emplace("npc_king", NPC{"npc_king", "岩背",
                                  "稳重而谨慎的老猴王。"});
    npcs_.emplace("npc_scout", NPC{"npc_scout", "闪尾",
                                   "行动敏捷、喜欢冒险的侦察猴。"});
    npcs_.emplace("npc_healer", NPC{"npc_healer", "叶婆婆",
                                    "熟悉森林草药的年长医护猴。"});
    npcs_.emplace("npc_child", NPC{"npc_child", "豆豆",
                                   "总想证明自己勇敢的小猴。"});
    npcs_.emplace("npc_hertz", NPC{"npc_hertz", "赫兹",
                                   "负责青木晶抽取工程的星猿。"});
}

const NPC* NPCSystem::findNPC(const std::string& npcId) const {
    const auto it = npcs_.find(npcId);
    return it == npcs_.end() ? nullptr : &it->second;
}

ActionResult NPCSystem::talkToNPC(const std::string& npcId, GameContext& ctx) {
    if (npcs_.empty()) initializeNPCs();
    if (!findNPC(npcId)) {
        return {false, "这里没有这个角色。", false, false};
    }

    std::string text;
    if (npcId == "npc_king") {
        if (ctx.world.getStage() <= 2) {
            text = ctx.world.hasFlag("flag_water_fixed")
                ? "岩背：你让清泉重新流动了。继续追查蓝光，我开始相信你能保护大家。"
                : "岩背：河谷的水正在消失。先查清蓝光管线，再谈守护整个猴群。";
        } else if (ctx.world.hasFlag("flag_complete_log")) {
            text = npcWillHelp(npcId, ctx)
                ? "岩背：日志证明星猿正在掏空青木谷。我会召集猴群响应你的最终行动。"
                : "岩背：证据足够，但族群还没有完全信任你。先帮助需要帮助的同伴。";
        } else {
            text = "岩背：河谷和山洞都出现了异常，去查清楚再回来。";
        }
    } else if (npcId == "npc_scout") {
        if (ctx.world.hasFlag(kScoutQuest)) {
            text = "闪尾：树冠捷径已经清理好，需要时我可以替你引开巡逻机。";
        } else {
            text = ctx.player.hasItem("item_rope")
                ? "闪尾盯着你背后的藤索：有它就能横过断崖。把计划交给我吧。"
                : "闪尾：山洞入口有发光脚印。找到森林里的藤索，我就带你走捷径。";
        }
    } else if (npcId == "npc_healer") {
        if (ctx.world.hasFlag(kHealerQuest)) {
            text = ctx.player.getHealth() < 60
                ? "叶婆婆：别逞强。战斗时先防御，再找机会使用草药。"
                : "叶婆婆：你带回的草药救了不少同伴。最终行动时我会照料伤员。";
        } else {
            text = ctx.player.hasItem("item_herb")
                ? "叶婆婆闻到草药香：正是这一株，交给我就能救治伤员。"
                : "叶婆婆：河谷还长着草药。带一株回来，我会替你疗伤。";
        }
    } else if (npcId == "npc_child") {
        if (ctx.world.hasFlag(kChildQuest)) {
            text = "豆豆：下次我不会再一个人跑到河边了！";
        } else if (ctx.world.hasFlag("flag_child_found")) {
            text = "豆豆抓住你的手：蓝光机器突然响了，快带我回猴王树！";
        } else {
            text = "豆豆：我沿着蓝光脚印跑来，却找不到回家的路了……";
        }
    } else {
        if (!ctx.world.hasFlag("flag_complete_log")) {
            text = "赫兹挡住控制台：离开。青木谷已被列入星猿能源区。";
        } else if (npcWillHelp(npcId, ctx)) {
            text = "赫兹沉默片刻：日志证明总部隐瞒了智慧生命信号。若你能破解护甲，我会听完你的条件。";
        } else {
            text = "赫兹：你读过日志又如何？没有足够智慧理解能源核心，就只能靠拳头阻止我。";
        }
    }
    return {true, text, false, false};
}

bool NPCSystem::npcWillHelp(const std::string& npcId,
                            const GameContext& ctx) const {
    if (npcId == "npc_scout") {
        return ctx.player.getReputation() >= 30 &&
               ctx.world.hasFlag(kScoutQuest);
    }
    if (npcId == "npc_king") {
        return ctx.player.getReputation() >= 60 ||
               ctx.world.hasFlag(kKingSupport);
    }
    if (npcId == "npc_healer") {
        return ctx.world.hasFlag(kHealerQuest);
    }
    if (npcId == "npc_child") {
        return ctx.world.hasFlag(kChildQuest);
    }
    if (npcId == "npc_hertz") {
        return ctx.player.getWisdom() >= 4 &&
               ctx.world.hasFlag("flag_complete_log");
    }
    return false;
}

std::string NPCSystem::getNPCQuest(const std::string& npcId,
                                   const GameContext& ctx) const {
    if (npcId == "npc_scout" && !ctx.world.hasFlag(kScoutQuest))
        return "取得藤索并交给闪尾。";
    if (npcId == "npc_healer" && !ctx.world.hasFlag(kHealerQuest))
        return "取得草药并交给叶婆婆。";
    if (npcId == "npc_child" && !ctx.world.hasFlag(kChildQuest))
        return "在河谷找到并救回豆豆。";
    if (npcId == "npc_king" && !ctx.world.hasFlag(kKingSupport))
        return "提高声望并取得猴王对最终行动的支持。";
    if (npcId == "npc_hertz")
        return "取得完整日志后决定说服赫兹或与其战斗。";
    return "当前没有可接取的任务。";
}

ActionResult NPCSystem::completeNPCQuest(const std::string& npcId,
                                         GameContext& ctx) {
    if (npcId == "npc_scout") {
        if (ctx.world.hasFlag(kScoutQuest))
            return {false, "闪尾的任务已经完成。", false, false};
        if (!ctx.player.hasItem("item_rope"))
            return {false, "你还没有找到藤索。", false, false};
        // 藤索是4号定义的关键物品，任务完成后仍保留在背包中。
        ctx.world.setFlag(kScoutQuest);
        ctx.world.setFlag("flag_scout_help");
        ctx.player.changeReputation(10);
        return {true, "闪尾接过藤索，答应在潜入基地时帮助你。声望+10。",
                true, false};
    }
    if (npcId == "npc_healer") {
        if (ctx.world.hasFlag(kHealerQuest))
            return {false, "叶婆婆的任务已经完成。", false, false};
        if (!ctx.player.hasItem("item_herb"))
            return {false, "你还没有带回草药。", false, false};
        ctx.player.removeItem("item_herb");
        ctx.world.setFlag(kHealerQuest);
        ctx.player.changeHealth(25);
        return {true, "叶婆婆收下草药并替你治疗。生命+25。", true, false};
    }
    if (npcId == "npc_child") {
        if (ctx.world.hasFlag(kChildQuest))
            return {false, "豆豆已经安全回到猴群。", false, false};
        if (!ctx.world.hasFlag("flag_child_found"))
            return {false, "你还没有在河谷找到豆豆。", false, false};
        ctx.world.setFlag(kChildQuest);
        ctx.player.changeReputation(15);
        return {true, "你把豆豆安全送回猴群。声望+15。", true, false};
    }
    if (npcId == "npc_king") {
        if (ctx.world.hasFlag(kKingSupport))
            return {false, "岩背已经答应支持最终行动。", false, false};
        if (ctx.player.getReputation() < 60)
            return {false, "你的声望还不足以获得猴王支持。", false, false};
        ctx.world.setFlag(kKingSupport);
        return {true, "岩背正式宣布支持你的最终行动。", false, false};
    }
    return {false, "该角色没有可完成的任务。", false, false};
}

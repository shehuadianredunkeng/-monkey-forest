#include "NPCSystem.h"
#include "CollectionSystem.h"

#include "Player.h"
#include "WorldState.h"

namespace {
constexpr const char* kScoutQuest = "flag_scout_quest_complete";
constexpr const char* kScoutMet = "flag_scout_met";
constexpr const char* kScoutChoiceMade = "flag_scout_choice_made";
constexpr const char* kScoutFightRequest = "flag_scout_fight_request";
constexpr const char* kScoutBananaPromise = "flag_scout_banana_promise";
constexpr const char* kEscapeSkill = "flag_skill_escape_unlocked";
constexpr const char* kChildQuest = "flag_child_rescued";
constexpr const char* kHealerQuest = "flag_healer_supplied";
constexpr const char* kKingSupport = "flag_king_support";

int storyYear(const WorldState& world) {
    const int stage = world.getStage();
    if (stage < 1) return 1;
    if (stage > 6) return 6;
    return stage;
}

std::string kingYearlyDialogue(const GameContext& ctx) {
    switch (storyYear(ctx.world)) {
    case 1:
        return "岩背：第一片新叶已经张开。年轻的脚不能只踩熟悉的树枝，去果实森林看看，也学会为自己的选择负责。";
    case 2:
        return "岩背：秋风提前到了，粮仓却没有满。真正的守护不是喊得响，而是让每只幼猴都能熬过寒夜。";
    case 3:
        return ctx.world.hasFlag("flag_water_fixed")
            ? "岩背：清泉重新流动了，可水里的蓝光不会说谎。顺着管线查下去，别让暂时的安稳蒙住眼睛。"
            : "岩背：河谷的水正在变浅。先查清那道蓝光，再回来告诉我，这场灾祸究竟来自天意还是别的什么。";
    case 4:
        return "岩背：外敌还没露出全貌，猴群自己先吵成了一团。能压住争执的是拳头，能带大家走远的却是判断。";
    case 5:
        return ctx.world.hasFlag("flag_complete_log")
            ? "岩背：日志已经说明一切。把你看见的真相记牢，最后的决定会让整个猴群承担后果。"
            : "岩背：那座基地像一根扎进山里的刺。进去以后别只顾着逞强，带回能让所有猴子相信的证据。";
    default:
        return ctx.world.hasFlag(kKingSupport)
            ? "岩背：我已经老了，但青木谷还年轻。今天由你站在最前面，我会让整个猴群站在你身后。"
            : "岩背：最后的路已经摆在面前。若想让族群跟随你，就先让他们看见你一路留下的担当。";
    }
}

std::string childYearlyDialogue(const WorldState& world) {
    switch (storyYear(world)) {
    case 1:
        return "豆豆：腿已经不疼啦！不过叶婆婆说今天还不许我爬高。等我长大，也要像你一样把迷路的小猴背回家。";
    case 2:
        return "豆豆把半颗果子塞进你手里：我偷偷留的，不许告诉别人。冬天很冷，可两只吗喽分着吃就没那么冷啦。";
    case 3:
        return "豆豆：我听见河谷下面嗡嗡响。大家说小猴别管，可我觉得害怕的东西更应该弄明白。你调查时要小心呀。";
    case 4:
        return "豆豆：他们为什么总在吵谁对谁错？如果每只猴子都肯先分一颗果子，是不是就能坐下来慢慢说了？";
    case 5:
        return "豆豆攥住你的手指：基地里会不会很黑？这颗亮石送给你。它其实不会发光，但你可以假装它会。";
    default:
        return "豆豆仰头看着你：不管你最后选哪条路，我都记得你没有把我丢在河边。所以这一次，也别把自己丢下。";
    }
}

std::string scoutYearlyDialogue(const WorldState& world) {
    switch (storyYear(world)) {
    case 1: return "闪尾：第一年就别皱着脸嘛。树梢风大，抓紧藤蔓，摔下来哥可只负责笑。";
    case 2: return "闪尾：粮食不够就别硬撑。哥去远处探过路，真有危险喊一声，咱们先活着回来。";
    case 3: return "闪尾：河谷那阵蓝光不对劲。哥在高处替你盯梢，你负责把藏在地下的秘密揪出来。";
    case 4: return "闪尾：一群猴子吵起来比蜂群还响。你决定往哪边走，哥就先替你看看那条路会不会塌。";
    case 5: return "闪尾：基地墙高得很，不过墙再高也拦不住会荡藤蔓的。逃跑不是认输，是给下次赢留条命。";
    default: return "闪尾：都走到最后一年了，还客气什么？你冲你的，真撑不住就喊哥——咱俩一起荡出去。";
    }
}
}

void NPCSystem::initializeNPCs() {
    npcs_.clear();
    activeDialogueNpcId_.clear();
    npcs_.emplace("npc_king", NPC{"npc_king", "岩背", "稳重而谨慎的老猴王。"});
    npcs_.emplace("npc_scout", NPC{"npc_scout", "闪尾", "行动敏捷、喜欢冒险的侦察猴。"});
    npcs_.emplace("npc_healer", NPC{"npc_healer", "叶婆婆", "熟悉森林草药的年长医护猴。"});
    npcs_.emplace("npc_child", NPC{"npc_child", "豆豆", "总想证明自己勇敢的小猴。"});
    npcs_.emplace("npc_hertz", NPC{"npc_hertz", "赫兹", "负责青木晶抽取工程的星猿。"});
}

const NPC* NPCSystem::findNPC(const std::string& npcId) const {
    const auto it = npcs_.find(npcId);
    return it == npcs_.end() ? nullptr : &it->second;
}

std::string NPCSystem::normalizeNPCId(const std::string& npcId) const {
    if (npcId == "岩背" || npcId == "猴王" || npcId == "king") return "npc_king";
    if (npcId == "闪尾" || npcId == "scout") return "npc_scout";
    if (npcId == "叶婆婆" || npcId == "healer") return "npc_healer";
    if (npcId == "豆豆" || npcId == "child") return "npc_child";
    if (npcId == "赫兹" || npcId == "hertz") return "npc_hertz";
    return npcId;
}

ActionResult NPCSystem::talkToNPC(const std::string& npcId, GameContext& ctx) {
    if (npcs_.empty()) initializeNPCs();
    const std::string id = normalizeNPCId(npcId);
    if (!findNPC(id)) return {false, "这里没有这个角色。", false, false};
    activeDialogueNpcId_.clear();

    // 玩家只需要持续使用“对话（talk）”：满足条件时自动提交并结算任务。
    if (id == "npc_scout" && ctx.world.hasFlag(kScoutChoiceMade) &&
        !ctx.world.hasFlag(kScoutQuest) && ctx.player.hasItem("item_rope"))
        return completeNPCQuest(id, ctx);
    if (id == "npc_healer" && !ctx.world.hasFlag(kHealerQuest) &&
        ctx.player.hasItem("item_herb"))
        return completeNPCQuest(id, ctx);
    if (id == "npc_king" && !ctx.world.hasFlag(kKingSupport) &&
        ctx.player.getReputation() >= 60)
        return completeNPCQuest(id, ctx);

    std::string text;
    if (id == "npc_king") {
        text = kingYearlyDialogue(ctx);
    } else if (id == "npc_scout") {
        if (ctx.world.hasFlag(kScoutQuest)) {
            text = scoutYearlyDialogue(ctx.world) +
                   "\n战斗中真撑不住就用“逃跑（escape）”，哥带你走！";
        } else if (!ctx.world.hasFlag(kScoutMet)) {
            ctx.world.setFlag(kScoutMet);
            text = "闪尾主动和你打招呼：嘿！小猴儿，有什么需要帮忙的找哥就是了，哥罩着你！\n"
                   "再次与闪尾对话即可回应他。";
        } else if (!ctx.world.hasFlag(kScoutChoiceMade)) {
            activeDialogueNpcId_ = id;
            text = "闪尾：说吧，小猴儿，想让哥怎么帮你？\n"
                   "1. 我打不过人家，你能帮我打回去不\n"
                   "2. 先谢了哥，回头给你带巴拿拿（香蕉）\n"
                   "请直接输入 1 或 2。";
        } else {
            text = "闪尾：答应哥的藤蔓还没影儿呢。去果实森林找一根能荡的藤蔓吧！\n"
                   "找到后回来再次与我对话（talk），哥马上教你保命绝活。";
        }
    } else if (id == "npc_healer") {
        if (ctx.world.hasFlag(kHealerQuest)) {
            text = ctx.player.getHealth() < 60
                ? "叶婆婆：别逞强。战斗时先防御，再找机会使用草药。"
                : "叶婆婆：你带回的草药救了不少同伴。最终行动时我会照料伤员。";
        } else {
            text = ctx.player.hasItem("item_herb")
                ? "叶婆婆闻到草药香：正是这一株，交给我就能救治伤员。"
                : "叶婆婆：河谷还长着草药。带一株回来，我会替你疗伤。";
        }
    } else if (id == "npc_child") {
        if (ctx.world.hasFlag(kChildQuest)) {
            text = childYearlyDialogue(ctx.world);
        } else if (!ctx.world.hasFlag("flag_child_found")) {
            ctx.world.setFlag("flag_child_found");
            text = "你在河谷边找到了豆豆。她的腿被划伤，正强忍着眼泪。\n"
                   "豆豆：我走不动了……你能帮帮我吗？再次与豆豆对话查看办法。";
        } else {
            activeDialogueNpcId_ = id;
            text = "豆豆的伤口还在流血，你准备怎么做？\n"
                   "1. 找叶婆婆前来救治【需要草药】\n"
                   "2. 给豆豆疗伤后将她带回猴王树【需要草药】\n"
                   "3. 暂时离开，寻找草药\n"
                   "请直接输入 1、2 或 3。";
        }
    } else {
        if (!ctx.world.hasFlag("flag_complete_log"))
            text = "赫兹挡住控制台：离开。青木谷已被列入星猿能源区。";
        else if (npcWillHelp(id, ctx))
            text = "赫兹沉默片刻：日志证明总部隐瞒了智慧生命信号。若你能破解护甲，我会听完你的条件。";
        else
            text = "赫兹：你读过日志又如何？没有足够智慧理解能源核心，就只能靠拳头阻止我。";
    }
    return {true, text, false, false};
}

ActionResult NPCSystem::chooseNPCDialogue(const std::string& npcId,
                                          int option,
                                          GameContext& ctx) {
    if (npcs_.empty()) initializeNPCs();
    const std::string id = normalizeNPCId(npcId);
    if (id == "npc_child") {
        if (ctx.world.hasFlag(kChildQuest))
            return {false, "豆豆已经安全回到猴群。", false, false};
        if (!ctx.world.hasFlag("flag_child_found"))
            return {false, "请先与豆豆交谈，确认她的情况。", false, false};
        if (option == 3)
            return {true, "你让豆豆留在安全处，决定先去寻找草药。", false, false};
        if (option != 1 && option != 2)
            return {false, "请选择 1、2 或 3。", false, false};
        if (!ctx.player.hasItem("item_herb")) {
            ctx.world.setFlag("flag_achievement_no_rice");
            CollectionSystem().unlockAchievement(
                "achievement_no_rice", ctx.world);
            return {false,
                    "没有草药，无法处理伤口。隐藏成就解锁：巧妇难为无米之炊！\n"
                    "请选择 3 暂时离开，并去河谷附近寻找草药。",
                    false, false};
        }
        ctx.player.removeItem("item_herb");
        ctx.world.setFlag(kChildQuest);
        ctx.world.setFlag("flag_child_saved");
        ctx.world.setFlag("flag_child_returned");
        ctx.player.changeReputation(15);
        if (option == 1) {
            ctx.world.setFlag("flag_healer_called_for_child");
            return {true,
                    "叶婆婆赶来敷好草药，并带豆豆返回猴王树。豆豆获得救治，声望+15。\n"
                    "你获得了豆豆的神秘祝福。",
                    true, false};
        }
        ctx.world.setFlag("flag_child_carried_home");
        ctx.player.setCurrentRoomId("room_tree");
        return {true,
                "你替豆豆包扎伤口，把她安全背回猴王树。声望+15。\n"
                "你获得了豆豆的神秘祝福。当前位置已更新为猴王树。",
                true, false};
    }
    if (id != "npc_scout")
        return {false, "这个角色当前没有对话选项。", false, false};
    if (!ctx.world.hasFlag(kScoutMet))
        return {false, "请先输入 talk 闪尾（或 talk scout）。", false, false};
    if (ctx.world.hasFlag(kScoutChoiceMade))
        return {false, "你已经回应过闪尾了。请按约定寻找藤蔓。", false, false};
    if (option != 1 && option != 2)
        return {false, "请选择 1 或 2。", false, false};

    ctx.world.setFlag(kScoutChoiceMade);
    if (option == 1) {
        ctx.world.setFlag(kScoutFightRequest);
        return {true,
                "闪尾：打不过不要紧，你找根能荡的绳儿给我，哥自有办法～\n"
                "任务更新：前往果实森林寻找藤蔓，取得后回来再次与闪尾对话（talk）。",
                true, false};
    }
    ctx.world.setFlag(kScoutBananaPromise);
    return {true,
            "闪尾：嘿～这么客气呢，那帮哥找根趁手能荡的藤蔓，有机会哥带你去溜溜！\n"
            "任务更新：前往果实森林寻找藤蔓，取得后回来再次与闪尾对话（talk）。",
            true, false};
}

ActionResult NPCSystem::chooseDialogueOption(int option, GameContext& ctx) {
    if (activeDialogueNpcId_.empty())
        return {false, "当前没有等待选择的对话，请先输入 talk NPC名。", false, false};
    const std::string npcId = activeDialogueNpcId_;
    ActionResult result = chooseNPCDialogue(npcId, option, ctx);
    if (result.success || option == 3) activeDialogueNpcId_.clear();
    return result;
}

bool NPCSystem::npcWillHelp(const std::string& npcId,
                            const GameContext& ctx) const {
    const std::string id = normalizeNPCId(npcId);
    if (id == "npc_scout")
        return ctx.world.hasFlag(kScoutQuest) && !ctx.world.hasFlag("flag_scout_left");
    if (id == "npc_king")
        return ctx.player.getReputation() >= 60 || ctx.world.hasFlag(kKingSupport);
    if (id == "npc_healer") return ctx.world.hasFlag(kHealerQuest);
    if (id == "npc_child") return ctx.world.hasFlag(kChildQuest);
    if (id == "npc_hertz")
        return ctx.player.getWisdom() >= 4 && ctx.world.hasFlag("flag_complete_log");
    return false;
}

std::string NPCSystem::getNPCQuest(const std::string& npcId,
                                   const GameContext& ctx) const {
    const std::string id = normalizeNPCId(npcId);
    if (id == "npc_scout" && !ctx.world.hasFlag(kScoutMet))
        return "先和闪尾打个招呼，听听他能提供什么帮助。";
    if (id == "npc_scout" && !ctx.world.hasFlag(kScoutChoiceMade))
        return "与闪尾对话并选择回应方式。";
    if (id == "npc_scout" && !ctx.world.hasFlag(kScoutQuest))
        return "在果实森林找到藤蔓并交给闪尾。";
    if (id == "npc_healer" && !ctx.world.hasFlag(kHealerQuest))
        return "取得草药并交给叶婆婆。";
    if (id == "npc_child" && !ctx.world.hasFlag("flag_child_found"))
        return "前往清泉河谷寻找豆豆，并直接与她交谈。";
    if (id == "npc_child" && !ctx.world.hasFlag(kChildQuest))
        return "准备一份草药，再与受伤的豆豆交谈并选择救治方式。";
    if (id == "npc_king" && !ctx.world.hasFlag(kKingSupport))
        return "提高声望并取得猴王对最终行动的支持。";
    if (id == "npc_hertz") return "取得完整日志后决定说服赫兹或与其战斗。";
    return "当前没有可接取的任务。";
}

ActionResult NPCSystem::completeNPCQuest(const std::string& npcId,
                                         GameContext& ctx) {
    const std::string id = normalizeNPCId(npcId);
    if (id == "npc_scout") {
        if (ctx.world.hasFlag(kScoutQuest))
            return {false, "闪尾的任务已经完成。", false, false};
        if (!ctx.world.hasFlag(kScoutChoiceMade))
            return {false, "请先和闪尾交谈并选择回应方式。", false, false};
        if (!ctx.player.hasItem("item_rope"))
            return {false, "你还没有找到藤蔓。去果实森林看看吧。", false, false};
        ctx.world.setFlag(kScoutQuest);
        ctx.world.setFlag("flag_scout_help");
        ctx.world.setFlag(kEscapeSkill);
        ctx.player.changeReputation(10);
        return {true,
                "闪尾抓住藤蔓试荡两圈，满意地拍了拍你的肩膀。\n"
                "任务完成！解锁闪尾战斗技能——“逃跑（escape）”。声望+10。",
                true, false};
    }
    if (id == "npc_healer") {
        if (ctx.world.hasFlag(kHealerQuest))
            return {false, "叶婆婆的任务已经完成。", false, false};
        if (!ctx.player.hasItem("item_herb"))
            return {false, "你还没有带回草药。", false, false};
        ctx.player.removeItem("item_herb");
        ctx.world.setFlag(kHealerQuest);
        ctx.player.changeHealth(25);
        return {true, "叶婆婆收下草药并替你治疗。生命+25。", true, false};
    }
    if (id == "npc_child") {
        if (ctx.world.hasFlag(kChildQuest))
            return {false, "豆豆已经安全回到猴群。", false, false};
        return {false,
                "豆豆任务现在通过对话完成：输入 talk 豆豆（或 talk child）查看救治选项。",
                false, false};
    }
    if (id == "npc_king") {
        if (ctx.world.hasFlag(kKingSupport))
            return {false, "岩背已经答应支持最终行动。", false, false};
        if (ctx.player.getReputation() < 60)
            return {false, "你的声望还不足以获得猴王支持。", false, false};
        ctx.world.setFlag(kKingSupport);
        return {true, "岩背正式宣布支持你的最终行动。", false, false};
    }
    return {false, "该角色没有可完成的任务。", false, false};
}

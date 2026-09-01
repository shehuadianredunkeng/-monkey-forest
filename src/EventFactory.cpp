#include "EventFactory.h"

#include <utility>

std::map<std::string, Event> createAllEvents() {
    std::map<std::string, Event> events;
    auto add = [&events](Event event) {
        const std::string id = event.eventId;
        events.emplace(id, std::move(event));
    };

    add({
        "event_tree_trial",
        "树冠试炼",
        "第一批果实挂在高处，野蜂在枝叶间盘旋。岩背让你证明自己能为猴群带回食物。",
        1, 1, "room_forest", {},
        {
            "闯入蜂群附近抢下果实（进入野蜂战斗，无属性要求）",
            "沿枯枝攀到树冠背面采集（需要体力≥10；攀爬≥2奖励更高）",
            "观察枝叶后选择安全路线（需要体力≥8；完成后智慧+1）"
        },
        "flag_event_tree_trial_done", EventKind::Main, true
    });

    add({
        "event_winter_shortage",
        "冬季短缺",
        "寒潮提前到来，猴王树下的食物只够维持几天。幼猴们望着仓库，族群等待你的决定。",
        2, 2, "room_tree", {"flag_event_tree_trial_done"},
        {
            "拿出公共食物优先照顾幼猴（需要公共食物≥2）",
            "带队冒雪再去寻找食物（需要体力≥15）",
            "保留食物并要求大家节省体力（无属性要求）"
        },
        "flag_event_winter_shortage_done", EventKind::Main, true
    });

    add({
        "event_glowing_river",
        "发光的河水",
        "清泉表面漂着不自然的蓝光，一根银色管线钻进岩壁，谷中的水位正在下降。",
        3, 3, "room_river", {"flag_event_winter_shortage_done"},
        {
            "搬开压住水道的岩石（需要力量≥2且体力≥10）",
            "追踪管线的水声与震动规律（需要体力≥10；完成后智慧+1）",
            "组织猴群共同挖出新水道（需要声望≥20）"
        },
        "flag_event_glowing_river_done", EventKind::Main, false
    });

    add({
        "event_echo_tracking",
        "回声追踪",
        "管线延伸进回声山洞。三组不同的回声与石壁刻痕互相呼应，猴形影子在深处迅速闪过。",
        3, 3, "room_cave", {"flag_water_fixed"},
        {
            "结合石壁刻痕理解回声规律（需要体力≥12；完成后智慧+1）",
            "从高处石梁绕过落石区（需要攀爬≥2）",
            "请闪尾带路并搜索隐蔽出口（需要闪尾同意协助）"
        },
        "flag_event_echo_tracking_done", EventKind::Main, true
    });

    add({
        "event_drought_choice",
        "干旱中的方向",
        "第二个旱季来临。修好的水道只能暂时缓解危机，族群必须在备战、调查和迁徙准备之间分配力量。",
        4, 4, "room_river", {"flag_chip_found"},
        {
            "训练守卫并储备反击工具（需要战斗≥1）",
            "研究晶片，寻找关闭抽取塔的方法（需要智慧≥2；首次研究智慧+1）",
            "沿河谷上游寻找适合迁徙的新家园（需要体力≥10）"
        },
        "flag_event_drought_choice_done", EventKind::Main, false
    });

    add({
        "event_group_dispute",
        "猴群争执",
        "岩背主张守住青木谷，年轻猴子却担心全族覆灭。争吵令士气不断下降，所有目光都转向你。",
        4, 4, "room_tree", {"flag_event_drought_choice_done"},
        {
            "调解双方，承诺根据证据作最终决定（需要领导≥1或声望≥20）",
            "说服大家准备正面反击（需要战斗≥1）",
            "说服大家同时准备迁徙后路（无属性要求）"
        },
        "flag_event_group_dispute_done", EventKind::Main, true
    });

    add({
        "event_base_infiltration",
        "基地潜入",
        "晶片打开了实验基地的大门。巡逻机守在能源室前，控制台里保存着青木晶抽取计划。",
        5, 5, "room_base", {"flag_base_open", "flag_chip_found"},
        {
            "正面突破巡逻机（进入巡逻机战斗，无属性要求）",
            "使用晶片伪装权限并重组日志（需要晶片且智慧≥3；完成后智慧+1）",
            "让闪尾引开巡逻机（需要闪尾协助或声望≥30）"
        },
        "flag_event_base_infiltration_done", EventKind::Main, true
    });

    add({
        "event_final_choice",
        "家园抉择",
        "抽取塔已经启动，赫兹给出最后通牒。你一路积累的力量、智慧、声望和物资将在此刻决定猴群的命运。",
        6, 6, "room_tree", {"flag_complete_log"},
        {
            "带领猴群反击并挑战赫兹（需要完成反击准备、声望≥60且战斗≥2）",
            "改写抽取程序迫使星猿撤离（需要完成技术研究、智慧≥4且取得完整日志）",
            "带领猴群向新河谷迁徙（需要完成迁徙准备、物资≥8、领导≥2并找到新家园）"
        },
        "flag_event_final_choice_done", EventKind::Main, true
    });

    add({
        "event_wildfire",
        "山火",
        "雷击点燃了果林边缘的枯木，火势正借着风向逼近猴王树。",
        1, 5, "room_forest", {},
        {
            "用燧石和泥土开出隔火带（需要燧石）",
            "组织猴群取水灭火（需要水源≥2且体力≥12）",
            "立即撤离并保全体力（无属性要求）"
        },
        "flag_event_wildfire_done", EventKind::Random, false
    });

    add({
        "event_hidden_orchard",
        "隐藏果园",
        "藤蔓后藏着一片没有被旱灾影响的小果园，枝头的果实足够让猴群撑过一段困难时期。",
        1, 5, "room_forest", {},
        {
            "把果园位置告诉整个猴群（无属性要求；公共食物和声望提升）",
            "只带走一部分并保护果园（无属性要求；兼顾食物和士气）",
            "独自吃下果实恢复体力（无属性要求；声望和士气下降）"
        },
        "flag_event_hidden_orchard_done", EventKind::Random, false
    });

    add({
        "event_drone_crash",
        "侦察机坠落",
        "一只银色侦察机撞上岩壁，外壳仍在发热。它的核心闪烁着和河水相同的蓝光。",
        2, 4, "room_river", {},
        {
            "拆解核心并研究其中的数据（无智慧门槛；首次研究智慧+1）",
            "取走晶片，再把残骸埋起来（无属性要求）",
            "设置假痕迹误导前来回收的星猿（需要体力≥8）"
        },
        "flag_event_drone_crash_done", EventKind::Random, false
    });

    return events;
}

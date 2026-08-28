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
            "闯入蜂群附近抢下果实（进入野蜂战斗）",
            "沿枯枝攀到树冠背面采集（攀爬路线）",
            "观察枝叶后选择最安全的路线（保底路线）"
        },
        "flag_event_tree_trial_done", EventKind::Main, true
    });

    add({
        "event_winter_shortage",
        "冬季短缺",
        "寒潮提前到来，猴王树下的食物只够维持几天。幼猴们望着仓库，族群等待你的决定。",
        2, 2, "room_tree", {"flag_event_tree_trial_done"},
        {
            "拿出公共食物优先照顾幼猴",
            "带队冒雪再去寻找食物",
            "保留食物并要求大家节省体力"
        },
        "flag_event_winter_shortage_done", EventKind::Main, true
    });

    add({
        "event_glowing_river",
        "发光的河水",
        "清泉表面漂着不自然的蓝光，一根银色管线钻进岩壁，谷中的水位正在下降。",
        3, 3, "room_river", {"flag_event_winter_shortage_done"},
        {
            "搬开压住水道的岩石（力量路线）",
            "沿管线寻找抽水装置（智慧路线）",
            "组织猴群共同挖出新水道（声望路线）"
        },
        "flag_event_glowing_river_done", EventKind::Main, false
    });

    add({
        "event_echo_tracking",
        "回声追踪",
        "管线延伸进回声山洞。三组不同的回声指向深处，猴形影子在石壁后迅速闪过。",
        3, 3, "room_cave", {"flag_water_fixed"},
        {
            "分析回声间隔，判断正确岔路（智慧路线）",
            "从高处石梁绕过落石区（攀爬路线）",
            "请闪尾带路并搜索隐蔽出口（同伴路线）"
        },
        "flag_event_echo_tracking_done", EventKind::Main, true
    });

    add({
        "event_drought_choice",
        "干旱中的方向",
        "第二个旱季来临。修好的水道只能暂时缓解危机，族群必须在备战、调查和迁徙准备之间分配力量。",
        4, 4, "room_river", {"flag_chip_found"},
        {
            "训练守卫并储备反击工具",
            "研究晶片，寻找关闭抽取塔的方法",
            "沿河谷上游寻找适合迁徙的新家园"
        },
        "flag_event_drought_choice_done", EventKind::Main, false
    });

    add({
        "event_group_dispute",
        "猴群争执",
        "岩背主张守住青木谷，年轻猴子却担心全族覆灭。争吵令士气不断下降，所有目光都转向你。",
        4, 4, "room_tree", {"flag_event_drought_choice_done"},
        {
            "调解双方，承诺根据证据作最终决定",
            "说服大家准备正面反击",
            "说服大家同时准备迁徙后路"
        },
        "flag_event_group_dispute_done", EventKind::Main, true
    });

    add({
        "event_base_infiltration",
        "基地潜入",
        "晶片打开了实验基地的大门。巡逻机守在能源室前，控制台里保存着青木晶抽取计划。",
        5, 5, "room_base", {"flag_base_open", "flag_chip_found"},
        {
            "正面突破巡逻机（进入巡逻机战斗）",
            "使用晶片伪装权限进入控制台（智慧路线）",
            "让闪尾引开巡逻机（同伴路线）"
        },
        "flag_event_base_infiltration_done", EventKind::Main, true
    });

    add({
        "event_final_choice",
        "家园抉择",
        "抽取塔已经启动，赫兹给出最后通牒。你一路积累的力量、智慧、声望和物资将在此刻决定猴群的命运。",
        6, 6, "room_tree", {"flag_complete_log"},
        {
            "带领猴群反击并挑战赫兹",
            "改写抽取程序，迫使星猿撤离",
            "带领猴群向新河谷迁徙"
        },
        "flag_event_final_choice_done", EventKind::Main, true
    });

    add({
        "event_wildfire",
        "山火",
        "雷击点燃了果林边缘的枯木，火势正借着风向逼近猴王树。",
        1, 5, "room_forest", {},
        {
            "用燧石和泥土开出隔火带",
            "组织猴群取水灭火",
            "立即撤离并保全体力"
        },
        "flag_event_wildfire_done", EventKind::Random, false
    });

    add({
        "event_injured_child",
        "幼猴受伤",
        "豆豆从湿滑的树枝上摔下，腿上划开了一道伤口。远处还传来危险的响动。",
        1, 5, "room_river", {},
        {
            "使用一份草药处理伤口",
            "背着豆豆返回猴王树",
            "呼喊叶婆婆并守在原地"
        },
        "flag_event_injured_child_done", EventKind::Random, false
    });

    add({
        "event_hidden_orchard",
        "隐藏果园",
        "藤蔓后藏着一片没有被旱灾影响的小果园，枝头的果实足够让猴群撑过一段困难时期。",
        1, 5, "room_forest", {},
        {
            "把果园位置告诉整个猴群",
            "只带走一部分并保护果园",
            "独占果实，恢复自己的体力"
        },
        "flag_event_hidden_orchard_done", EventKind::Random, false
    });

    add({
        "event_drone_crash",
        "侦察机坠落",
        "一只银色侦察机撞上岩壁，外壳仍在发热。它的核心闪烁着和河水相同的蓝光。",
        2, 4, "room_river", {},
        {
            "拆解核心并研究其中的数据",
            "取走晶片，再把残骸埋起来",
            "设置假痕迹误导前来回收的星猿"
        },
        "flag_event_drone_crash_done", EventKind::Random, false
    });

    return events;
}

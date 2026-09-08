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
        "第一批果实挂在树冠最高处，野蜂正绕着蜂巢盘旋。岩背没有替你决定该冒险、"
        "绕行还是观察——这是你第一次独自为猴群作出选择。",
        1, 1, "room_forest", {},
        {
            "闯入蜂群附近抢下果实（进入野蜂战斗，无属性要求）",
            "凭力量压住摇晃的粗枝，从树冠背面采集（需要力量≥2且体力≥10）",
            "观察枝叶后选择安全路线（需要体力≥8；完成后智慧+1）"
        },
        "flag_event_tree_trial_done", EventKind::Main, true
    });

    add({
        "event_winter_shortage",
        "冬季短缺",
        "寒潮提前封住森林，猴王树下的储粮只够维持几天。幼猴守在空篮旁，守卫也已"
        "疲惫不堪；有限的食物应该先救谁，必须由你决定。",
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
        "清泉表面浮着不自然的蓝光，银色管线像根冰冷的藤蔓钻进岩壁。水位仍在下降，"
        "若不能立刻恢复水流，猴王树撑不过下一次旱季。",
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
        "管线消失在回声山洞深处。三组回声与石壁刻痕彼此呼应，黑暗里还残留着陌生"
        "机器的热气；找到管线尽头，才能确认是谁在偷走清泉。",
        3, 3, "room_cave", {"flag_water_fixed"},
        {
            "结合石壁刻痕理解回声规律（需要体力≥12；完成后智慧+1）",
            "凭力量推稳松动石梁，绕过落石区（需要力量≥2且体力≥6）",
            "请闪尾带路并搜索隐蔽出口（需要闪尾同意协助）"
        },
        "flag_event_echo_tracking_done", EventKind::Main, true
    });

    add({
        "event_drought_choice",
        "干旱中的方向",
        "晶片证明偷水者来自星空，但第二个旱季已经逼近。猴群没有足够时间同时准备"
        "所有方案：磨利木矛、破解晶片或寻找新家园，你必须选定一条优先路线。",
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
        "岩背坚持守住祖辈留下的青木谷，年轻猴子却害怕全族困死在枯树之间。争吵让"
        "刚形成的计划濒临破裂，你必须让猴群重新站到一起。",
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
        "晶片打开实验基地的大门，刺眼的白光随即扫过走廊。巡逻机守在能源室前，"
        "控制台里的完整日志，是揭穿赫兹计划并决定族群命运的最后证据。",
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
        "抽取塔轰鸣着刺入地下，清泉正在肉眼可见地退去。赫兹只给猴群最后一次选择；"
        "过去三年的准备、帮助与取舍，将在这一刻真正决定青木谷的命运。",
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
        "雷击点燃果林边缘的枯木，火星顺风越过树冠。若让火势穿过这片林地，猴王树"
        "和冬季储粮都会化成灰烬。",
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
        "厚重藤蔓后藏着一片未被旱灾侵袭的小果园。果香近在眼前，但公开位置、保护"
        "幼树还是独自取食，会让族群用不同方式记住你的选择。",
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
        "一只银色侦察机撞上岩壁，外壳仍在发热，远处已经传来回收队的金属脚步声。"
        "核心闪着与河水相同的蓝光，你只能迅速决定怎样处理残骸。",
        2, 4, "room_river", {},
        {
            "拆解核心并把发现告诉猴群（无属性要求；获得晶片和声望）",
            "取走晶片，再把残骸埋起来（无属性要求）",
            "设置假痕迹误导前来回收的星猿（需要体力≥8）"
        },
        "flag_event_drone_crash_done", EventKind::Random, false
    });

    return events;
}

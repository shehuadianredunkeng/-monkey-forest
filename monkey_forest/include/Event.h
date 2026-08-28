#pragma once

#include <string>
#include <vector>

enum class EventKind {
    Main,
    Random
};

// 事件只保存剧情数据；玩家和世界状态由 EventSystem 通过 GameContext 修改。
struct Event {
    std::string eventId;
    std::string title;
    std::string description;
    int minStage = 1;
    int maxStage = 1;
    std::string requiredRoomId;
    std::vector<std::string> requiredFlags;
    std::vector<std::string> choices;
    std::string completionFlag;
    EventKind kind = EventKind::Main;
    bool completesStage = false;

    std::string pendingFlag() const;
    std::string formatPrompt() const;
};

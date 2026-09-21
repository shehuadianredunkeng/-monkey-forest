#pragma once

#include <string>
#include <vector>

using namespace std;

enum class EventKind {
    Main,
    Random
};

// 事件只存剧情数据，玩家和世界状态由 EventSystem 改。
struct Event {
    string eventId;
    string title;
    string description;
    int minStage = 1;
    int maxStage = 1;
    string requiredRoomId;
    vector<string> requiredFlags;
    vector<string> choices;
    string completionFlag;
    EventKind kind = EventKind::Main;
    bool completesStage = false;

    string pendingFlag() const;
    string formatPrompt() const;
};

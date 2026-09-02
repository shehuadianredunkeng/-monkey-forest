#pragma once

#include <string>
#include <vector>

class CombatSystem;
struct GameContext;

class ConsoleUI {
public:
    ConsoleUI();
    void appendLog(const std::string& utf8Text);
    void render(const GameContext& ctx,
                const CombatSystem& combat,
                const std::string& guideText);
    std::string readCommand();
    void restoreCursor();

private:
    std::vector<std::wstring> history_;
};


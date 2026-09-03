#pragma once

#include "UI/ConsoleRenderer.h"

#include <optional>

struct GameContext;

namespace UI {
struct GameView {
    std::wstring location;
    std::wstring taskTitle;
    std::wstring taskStatus;
    std::wstring taskHint;
    int health = 100;
    int stamina = 60;
    int wisdom = 1;
    int strength = 1;
    int reputation = 0;
    int inventorySlots = 0;
    bool inBattle = false;
};
GameView readGameView(const GameContext& ctx, bool inBattle, const std::wstring& guide);

class GameUI {
public:
    explicit GameUI(ConsoleRenderer& renderer);
    void appendLog(const std::wstring& text);
    bool render(const GameView& view);
    std::optional<std::wstring> readCommand();
private:
    struct Line { std::wstring text; Color color; };
    ConsoleRenderer& renderer_;
    std::vector<Line> history_;
    GameView lastView_;
    std::size_t scrollBack_ = 0;
    bool frameReady_ = false;
    void paragraph(Rect area, const std::wstring& text, Color color);
    void bar(SHORT y, int value);
    void drawInput(const std::vector<std::wstring>& glyphs, std::size_t caret);
};
}

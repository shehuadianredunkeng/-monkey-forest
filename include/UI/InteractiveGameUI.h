#pragma once

#include "InteractiveMap.h"
#include "UI/ConsoleRenderer.h"

#include <optional>
#include <map>
#include <string>
#include <vector>

class CombatSystem;
struct GameContext;

namespace UI {

enum class ExploreAction {
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Interact,
    Inventory,
    UseItem,
    Save,
    Help,
    Menu,
    Choice1,
    Choice2,
    Choice3,
    Choice4,
    HistoryUp,
    HistoryDown,
    EndOfInput
};

class InteractiveGameUI {
public:
    explicit InteractiveGameUI(ConsoleRenderer& renderer);

    void appendLog(const std::string& text);
    bool render(const GameContext& ctx,
                const InteractiveMap& map,
                const CombatSystem& combat,
                const std::string& objective);
    ExploreAction readExploreAction();
    std::optional<std::string> readTypedCommand(const std::wstring& prompt);

    int showMainMenu(bool hasAnySave);
    int showPauseMenu();
    int showSlotMenu(const std::wstring& title,
                     const std::vector<std::wstring>& slotDescriptions,
                     bool allowEmpty);
    void showTextPage(const std::wstring& title, const std::wstring& text);
    void showEndingCinematic(const std::wstring& title,
                             const std::wstring& text);
    void clearLog();

private:
    struct LogLine {
        std::wstring text;
        Color color = Color::Normal;
    };

    ConsoleRenderer& renderer_;
    std::vector<LogLine> history_;
    std::vector<MapTileVisual> lastMapTiles_;
    std::map<int, LogLine> lastStableRows_;
    std::size_t historyScrollBack_ = 0;
    bool needsFullClear_ = true;

    void centered(SHORT y, const std::wstring& text, Color color);
    void drawStableLine(Rect area, SHORT y, const std::wstring& text,
                        Color color);
    int menu(const std::wstring& title,
             const std::vector<std::wstring>& options,
             int initial = 0);
    void drawTypedInput(const std::wstring& prompt,
                        const std::vector<std::wstring>& glyphs,
                        std::size_t caret);
};

}  // namespace UI

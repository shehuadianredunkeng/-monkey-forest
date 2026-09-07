#include "UI/InteractiveGameUI.h"

#include "CombatSystem.h"
#include "Player.h"
#include "Room.h"
#include "UI/TextLayout.h"
#include "WorldState.h"

#include <algorithm>

namespace UI {
namespace {
constexpr Rect LEFT_LOG{1, 19, DIVIDER_X - 1, INPUT_TOP - 1};
constexpr Rect RIGHT_PANEL{DIVIDER_X + 1, 1, UI_WIDTH - 1, INPUT_TOP - 1};

Color messageColor(const std::wstring& text) {
    if (text.find(L"失败") != std::wstring::npos ||
        text.find(L"无法") != std::wstring::npos ||
        text.find(L"不足") != std::wstring::npos) return Color::Error;
    if (text.find(L"获得") != std::wstring::npos ||
        text.find(L"成功") != std::wstring::npos ||
        text.find(L"完成") != std::wstring::npos ||
        text.find(L"+1") != std::wstring::npos) return Color::Success;
    if (text.find(L"提示") != std::wstring::npos ||
        text.find(L"任务") != std::wstring::npos) return Color::Hint;
    return Color::Normal;
}

std::wstring joinGlyphs(const std::vector<std::wstring>& glyphs,
                        std::size_t from, std::size_t to) {
    std::wstring text;
    for (std::size_t i = from; i < to; ++i) text += glyphs[i];
    return text;
}

std::wstring roomName(const GameContext& ctx) {
    const auto found = ctx.rooms.find(ctx.player.getCurrentRoomId());
    return found == ctx.rooms.end() ? L"未知" : fromUtf8(found->second.getName());
}

std::wstring roomShort(const std::string& id) {
    if (id == "room_tree") return L"王";
    if (id == "room_forest") return L"林";
    if (id == "room_river") return L"河";
    if (id == "room_cave") return L"洞";
    if (id == "room_base") return L"基";
    return L"?";
}

std::wstring progressBar(int value) {
    value = std::clamp(value, 0, 100);
    const int filled = value / 10;
    return L"[" + std::wstring(static_cast<size_t>(filled), L'#') +
           std::wstring(static_cast<size_t>(10 - filled), L'-') + L"] " +
           std::to_wstring(value);
}
}

InteractiveGameUI::InteractiveGameUI(ConsoleRenderer& renderer)
    : renderer_(renderer) {}

void InteractiveGameUI::appendLog(const std::string& text) {
    const std::wstring wide = fromUtf8(text);
    std::size_t start = 0;
    do {
        const std::size_t end = wide.find(L'\n', start);
        const std::wstring line = wide.substr(
            start, end == std::wstring::npos ? end : end - start);
        for (const std::wstring& wrapped :
             renderer_.wrapText(line, DIVIDER_X - 3))
            history_.push_back({wrapped, messageColor(wrapped)});
        if (end == std::wstring::npos) break;
        start = end + 1;
    } while (start <= wide.size());
    if (history_.size() > 300)
        history_.erase(history_.begin(), history_.end() - 300);
}

bool InteractiveGameUI::render(const GameContext& ctx,
                               const InteractiveMap& map,
                               const CombatSystem& combat,
                               const std::string& objective) {
    if (!renderer_.beginFrame()) return false;

    renderer_.drawText(1, 1, L"【" + roomName(ctx) + L"】互动地图",
                       Color::Title);
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const MapTileVisual tile = map.visualAt(x, y, ctx);
            renderer_.drawText(static_cast<SHORT>(1 + x * 2),
                               static_cast<SHORT>(2 + y), tile.glyph,
                               tile.color);
        }
    }
    renderer_.drawHorizontalLine(18, 0, DIVIDER_X);
    renderer_.drawText(1, 18, L"+ 剧情记录 ", Color::Title);
    const std::size_t capacity = LEFT_LOG.bottom - LEFT_LOG.top + 1;
    const std::size_t first = history_.size() > capacity
                                  ? history_.size() - capacity
                                  : 0;
    for (std::size_t i = first; i < history_.size(); ++i)
        renderer_.drawTextIn(
            LEFT_LOG, LEFT_LOG.left,
            static_cast<SHORT>(LEFT_LOG.top + i - first),
            history_[i].text, history_[i].color);

    const SHORT right = DIVIDER_X + 2;
    renderer_.drawText(right, 1, L"【小地图】", Color::Title);
    renderer_.drawText(right, 2, L"王--林--河--基", Color::Normal);
    renderer_.drawText(right, 3, L"   |    /", Color::Wall);
    renderer_.drawText(right, 4, L"   洞", Color::Normal);
    renderer_.drawText(right, 5,
                       L"当前位置：" + roomShort(ctx.player.getCurrentRoomId()) +
                           L" / " + roomName(ctx),
                       Color::Hint);
    renderer_.drawText(right, 6, L"门=切图  红！=主线", Color::Door);

    renderer_.drawText(right, 9, L"【状态】", Color::Title);
    renderer_.drawText(right, 10,
                       L"生命 " + progressBar(ctx.player.getHealth()),
                       ctx.player.getHealth() <= 30 ? Color::Error : Color::Success);
    renderer_.drawText(right, 11,
                       L"体力 " + progressBar(ctx.player.getStamina()),
                       Color::Success);
    renderer_.drawText(right, 12,
                       L"力量 " + std::to_wstring(ctx.player.getStrength()) +
                           L"  智慧 " + std::to_wstring(ctx.player.getWisdom()));
    renderer_.drawText(right, 13,
                       L"声望 " + std::to_wstring(ctx.player.getReputation()) +
                           L"  阶段 " + std::to_wstring(ctx.world.getStage()));
    renderer_.drawText(right, 14,
                       L"食物 " + std::to_wstring(ctx.world.getResource(ResourceType::Food)) +
                           L"  水 " + std::to_wstring(ctx.world.getResource(ResourceType::Water)));
    if (combat.isInBattle()) {
        renderer_.drawText(right, 15,
                           L"战斗：" + fromUtf8(combat.getBattleState().enemyId) +
                               L" HP " + std::to_wstring(combat.getBattleState().enemyHealth),
                           Color::Error);
    } else {
        renderer_.drawText(right, 15, L"背包：" +
                                          std::to_wstring(ctx.player.getInventory().getItems().size()) +
                                          L"/8");
    }
    renderer_.drawText(right, 16, L"【当前目标】", Color::Title);
    const auto objectiveLines = renderer_.wrapText(fromUtf8(objective),
                                                   UI_WIDTH - right);
    for (std::size_t i = 0; i < objectiveLines.size() && i < 3; ++i)
        renderer_.drawText(right, static_cast<SHORT>(17 + i),
                           objectiveLines[i], Color::Hint);

    renderer_.drawText(right, 21, L"【图例/操作】", Color::Title);
    renderer_.drawText(right, 22, L"猴 玩家  友 NPC", Color::Player);
    renderer_.drawText(right, 23, L"物 物品  宝 宝箱  敌 战斗", Color::Item);
    renderer_.drawText(right, 24, L"WASD移动  Enter/空格互动", Color::Hint);

    renderer_.drawText(1, 26,
                       combat.isInBattle()
                           ? L"战斗模式：请键入攻击/防御/偷窃/使用/逃跑"
                           : L"I背包  U使用物品  P存档  H帮助  Esc主菜单",
                       Color::Hint);
    renderer_.drawText(1, 27, fromUtf8(map.nearbyHint(ctx)), Color::Hint);
    renderer_.drawFrame();
    return true;
}

ExploreAction InteractiveGameUI::readExploreAction() {
    while (true) {
        const InputEvent event = renderer_.readEvent();
        if (event.key == Key::EndOfInput) return ExploreAction::EndOfInput;
        if (event.key == Key::Resize) return ExploreAction::None;
        if (event.key == Key::Up) return ExploreAction::MoveUp;
        if (event.key == Key::Down) return ExploreAction::MoveDown;
        if (event.key == Key::Left) return ExploreAction::MoveLeft;
        if (event.key == Key::Right) return ExploreAction::MoveRight;
        if (event.key == Key::Enter) return ExploreAction::Interact;
        if (event.key == Key::Escape) return ExploreAction::Menu;
        if (event.key != Key::Text || event.text.empty()) continue;
        wchar_t key = event.text.front();
        if (key >= L'A' && key <= L'Z') key = key - L'A' + L'a';
        if (key == L'w') return ExploreAction::MoveUp;
        if (key == L's') return ExploreAction::MoveDown;
        if (key == L'a') return ExploreAction::MoveLeft;
        if (key == L'd') return ExploreAction::MoveRight;
        if (key == L' ' || key == L'e') return ExploreAction::Interact;
        if (key == L'i' || key == L'b') return ExploreAction::Inventory;
        if (key == L'u') return ExploreAction::UseItem;
        if (key == L'p' || key == L'k') return ExploreAction::Save;
        if (key == L'h' || key == L'?') return ExploreAction::Help;
        if (key == L'1') return ExploreAction::Choice1;
        if (key == L'2') return ExploreAction::Choice2;
        if (key == L'3') return ExploreAction::Choice3;
    }
}

void InteractiveGameUI::drawTypedInput(
    const std::wstring& prompt, const std::vector<std::wstring>& glyphs,
    std::size_t caret) {
    renderer_.clearInput();
    const int promptWidth = renderer_.columns(prompt);
    const int available = UI_WIDTH - promptWidth - 2;
    std::size_t start = caret;
    int before = 0;
    while (start > 0) {
        const int width = renderer_.columns(glyphs[start - 1]);
        if (before + width > available) break;
        before += width;
        --start;
    }
    renderer_.drawText(1, INPUT_ROW, prompt, Color::Hint);
    renderer_.drawText(static_cast<SHORT>(1 + promptWidth), INPUT_ROW,
                       renderer_.clip(joinGlyphs(glyphs, start, glyphs.size()),
                                      available));
    renderer_.showCursor(static_cast<SHORT>(1 + promptWidth + before),
                         INPUT_ROW, true);
}

std::optional<std::string> InteractiveGameUI::readTypedCommand(
    const std::wstring& prompt) {
    std::vector<std::wstring> input;
    std::size_t caret = 0;
    drawTypedInput(prompt, input, caret);
    while (true) {
        const InputEvent event = renderer_.readEvent();
        if (event.key == Key::EndOfInput || event.key == Key::Escape)
            return std::nullopt;
        if (event.key == Key::Enter)
            return toUtf8(joinGlyphs(input, 0, input.size()));
        if (event.key == Key::Text) {
            const auto added = splitGlyphs(event.text);
            input.insert(input.begin() + static_cast<std::ptrdiff_t>(caret),
                         added.begin(), added.end());
            caret += added.size();
        } else if (event.key == Key::Backspace && caret > 0) {
            input.erase(input.begin() + static_cast<std::ptrdiff_t>(--caret));
        } else if (event.key == Key::Delete && caret < input.size()) {
            input.erase(input.begin() + static_cast<std::ptrdiff_t>(caret));
        } else if (event.key == Key::Left && caret > 0) {
            --caret;
        } else if (event.key == Key::Right && caret < input.size()) {
            ++caret;
        } else if (event.key == Key::Home) {
            caret = 0;
        } else if (event.key == Key::End) {
            caret = input.size();
        }
        drawTypedInput(prompt, input, caret);
    }
}

void InteractiveGameUI::centered(SHORT y, const std::wstring& text,
                                 Color color) {
    const int width = renderer_.columns(text);
    const SHORT x = static_cast<SHORT>(std::max(1, (UI_WIDTH - width) / 2));
    renderer_.drawText(x, y, text, color);
}

int InteractiveGameUI::menu(const std::wstring& title,
                            const std::vector<std::wstring>& options,
                            int initial) {
    int selected = std::clamp(initial, 0, static_cast<int>(options.size()) - 1);
    while (true) {
        renderer_.beginFrame();
        renderer_.drawHorizontalLine(3, 16, UI_WIDTH - 16);
        renderer_.drawHorizontalLine(22, 16, UI_WIDTH - 16);
        centered(5, L"吗 喽 森 林", Color::Success);
        centered(7, L"M O N K E Y   F O R E S T", Color::Title);
        centered(9, title, Color::Hint);
        for (std::size_t i = 0; i < options.size(); ++i) {
            const std::wstring line =
                (static_cast<int>(i) == selected ? L">  " : L"   ") + options[i];
            centered(static_cast<SHORT>(12 + i * 2), line,
                     static_cast<int>(i) == selected ? Color::Hint : Color::Normal);
        }
        centered(24, L"W/S或方向键选择 · Enter/空格确认 · Esc返回", Color::Wall);
        const InputEvent event = renderer_.readEvent();
        if (event.key == Key::EndOfInput) return -1;
        if (event.key == Key::Escape) return -1;
        const int optionCount = static_cast<int>(options.size());
        if (event.key == Key::Up) selected = (selected + optionCount - 1) % optionCount;
        if (event.key == Key::Down) selected = (selected + 1) % optionCount;
        if (event.key == Key::Enter) return selected;
        if (event.key == Key::Text && !event.text.empty()) {
            wchar_t key = event.text.front();
            if (key == L'w' || key == L'W')
                selected = (selected + optionCount - 1) % optionCount;
            else if (key == L's' || key == L'S')
                selected = (selected + 1) % optionCount;
            else if (key == L' ') return selected;
        }
    }
}

int InteractiveGameUI::showPauseMenu() {
    return menu(L"暂 停",
                {L"继续游戏", L"保存游戏", L"返回开始界面"});
}

int InteractiveGameUI::showMainMenu(bool hasAnySave) {
    return menu(L"家 园 守 卫 战",
                {L"新的开始", hasAnySave ? L"继续游戏" : L"继续游戏（暂无存档）",
                 L"结局收集", L"成就系统", L"退出游戏"});
}

int InteractiveGameUI::showSlotMenu(
    const std::wstring& title,
    const std::vector<std::wstring>& slotDescriptions,
    bool allowEmpty) {
    std::vector<std::wstring> options;
    for (std::size_t i = 0; i < slotDescriptions.size(); ++i) {
        std::wstring text = L"存档位 " + std::to_wstring(i + 1) + L"：" + slotDescriptions[i];
        if (!allowEmpty && slotDescriptions[i] == L"空") text += L"（不可读取）";
        options.push_back(text);
    }
    options.push_back(L"返回");
    while (true) {
        const int selected = menu(title, options);
        if (selected < 0 || selected == static_cast<int>(options.size()) - 1)
            return 0;
        if (allowEmpty || slotDescriptions[static_cast<std::size_t>(selected)] != L"空")
            return selected + 1;
    }
}

void InteractiveGameUI::showTextPage(const std::wstring& title,
                                     const std::wstring& text) {
    renderer_.beginFrame();
    centered(2, title, Color::Title);
    const auto lines = renderer_.wrapText(text, UI_WIDTH - 10);
    for (std::size_t i = 0; i < lines.size() && i < 22; ++i)
        renderer_.drawText(5, static_cast<SHORT>(5 + i), lines[i],
                           messageColor(lines[i]));
    centered(27, L"按Enter、空格或Esc返回", Color::Hint);
    while (true) {
        const InputEvent event = renderer_.readEvent();
        if (event.key == Key::Enter || event.key == Key::Escape ||
            event.key == Key::EndOfInput ||
            (event.key == Key::Text && event.text == L" ")) return;
    }
}

void InteractiveGameUI::clearLog() { history_.clear(); }

}  // namespace UI

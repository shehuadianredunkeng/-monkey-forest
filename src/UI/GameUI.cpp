#include "UI/GameUI.h"

#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <algorithm>

namespace UI {
namespace {
constexpr Rect LEFT{1, 3, DIVIDER_X - 1, INPUT_TOP - 1};
constexpr SHORT RIGHT = DIVIDER_X + 1;
std::wstring join(const std::vector<std::wstring>& glyphs, std::size_t from, std::size_t to) {
    std::wstring text;
    for (auto i = from; i < to; ++i) text += glyphs[i];
    return text;
}
Color logColor(const std::wstring& text) {
    if (text.find(L"【智慧 +1】") != std::wstring::npos || text.find(L"智慧+1") != std::wstring::npos ||
        text.find(L"成功") != std::wstring::npos) return Color::Success;
    if (text.find(L"失败") != std::wstring::npos || text.find(L"无法") != std::wstring::npos ||
        text.find(L"不足") != std::wstring::npos || text.find(L"错误") != std::wstring::npos) return Color::Error;
    if (text.find(L"> ") == 0 || text.find(L"提示") != std::wstring::npos) return Color::Hint;
    return Color::Normal;
}
}

GameView readGameView(const GameContext& ctx, bool inBattle, const std::wstring& guide) {
    GameView view;
    // Only existing public const getters. No HP/SP aliases or new Task model needed.
    const Player& player = ctx.player;
    const WorldState& world = ctx.world;
    const auto& rooms = ctx.rooms;
    const auto room = rooms.find(player.getCurrentRoomId());
    view.location = room == rooms.end() ? L"未知地点" : fromUtf8(room->second.getName());
    view.health = player.getHealth(); view.stamina = player.getStamina();
    view.wisdom = player.getWisdom(); view.strength = player.getStrength();
    view.reputation = player.getReputation();
    view.inventorySlots = static_cast<int>(player.getInventory().getItems().size());
    view.inBattle = inBattle;
    std::wstring clean = guide;
    // The existing guide includes technical IDs. Hide them in the presentation only.
    for (auto start = clean.find(L"（room_"); start != std::wstring::npos; start = clean.find(L"（room_")) {
        const auto end = clean.find(L'）', start);
        if (end == std::wstring::npos) { clean.erase(start); break; }
        clean.erase(start, end - start + 1);
    }
    const auto newline = clean.find(L'\n');
    view.taskTitle = clean.substr(0, newline);
    const std::wstring prefix = L"当前主线：";
    if (view.taskTitle.find(prefix) == 0) view.taskTitle.erase(0, prefix.size());
    view.taskHint = newline == std::wstring::npos ? L"输入 guide 查看详情" : clean.substr(newline + 1);
    view.taskStatus = player.getHealth() <= 0 || world.hasFlag("flag_final_choice") ? L"已结束" :
        clean.find(L"已完成") != std::wstring::npos || clean.find(L"已经完成") != std::wstring::npos ? L"已完成" : L"进行中";
    return view;
}

GameUI::GameUI(ConsoleRenderer& renderer) : renderer_(renderer) {}
void GameUI::appendLog(const std::wstring& text) {
    if (text.empty()) return;
    // Store unwrapped logical lines so a different console font is measured afresh.
    std::size_t from = 0;
    while (from < text.size()) {
        const auto end = text.find(L'\n', from);
        const auto line = text.substr(from, end == std::wstring::npos ? end : end - from);
        history_.push_back({line, logColor(line)});
        if (end == std::wstring::npos) break;
        from = end + 1;
    }
    if (history_.size() > 500) history_.erase(history_.begin(), history_.end() - 500);
    scrollBack_ = 0;
}
void GameUI::paragraph(Rect area, const std::wstring& text, Color color) {
    const int width = area.right - area.left + 1;
    const auto lines = renderer_.wrapText(text, width);
    const auto count = std::min<std::size_t>(lines.size(), area.bottom - area.top + 1);
    for (std::size_t n = 0; n < count; ++n) {
        auto line = lines[n];
        if (n + 1 == count && count < lines.size()) line = renderer_.clip(line, width - 3) + L"...";
        renderer_.drawTextIn(area, area.left, static_cast<SHORT>(area.top + n), line, color);
    }
}
void GameUI::bar(SHORT y, int value) {
    value = std::clamp(value, 0, 100);
    for (SHORT i = 0; i < 10; ++i)
        renderer_.drawText(RIGHT + i, y, i < value / 10 ? L"#" : L"-",
                           value <= 20 ? Color::Error : Color::Success);
    renderer_.drawText(RIGHT + 12, y, std::to_wstring(value) + L"/100");
}
bool GameUI::render(const GameView& view) {
    lastView_ = view;
    frameReady_ = renderer_.beginFrame();
    if (!frameReady_) return false;
    paragraph({1, 1, 72, 1}, L"【" + view.location + L"】", Color::Title);
    renderer_.drawText(1, 2, view.inBattle ? L"战斗中 · 请输入战斗指令" : L"剧情记录 · PageUp / PageDown 翻阅", Color::Hint);
    std::vector<Line> wrapped;
    for (const auto& line : history_)
        for (const auto& part : renderer_.wrapText(line.text, LEFT.right - LEFT.left + 1))
            wrapped.push_back({part, line.color});
    constexpr std::size_t rows = LEFT.bottom - LEFT.top + 1;
    scrollBack_ = std::min(scrollBack_, wrapped.size() > rows ? wrapped.size() - rows : 0);
    const auto end = wrapped.size() - scrollBack_;
    const auto start = end > rows ? end - rows : 0;
    for (auto n = start; n < end; ++n)
        renderer_.drawTextIn(LEFT, LEFT.left, static_cast<SHORT>(LEFT.top + n - start), wrapped[n].text, wrapped[n].color);

    renderer_.drawText(RIGHT, 1, L"【当前任务】", Color::Title);
    paragraph({RIGHT, 2, 109, 3}, view.taskTitle, Color::Normal);
    renderer_.drawText(RIGHT, 4, L"状态：" + view.taskStatus, Color::Hint);
    paragraph({RIGHT, 5, 109, 7}, L"提示：" + view.taskHint, Color::Hint);
    renderer_.drawText(RIGHT, 9, L"【玩家状态】", Color::Title);
    renderer_.drawText(RIGHT, 10, L"生命 HP:"); bar(11, view.health);
    renderer_.drawText(RIGHT, 12, L"体力 SP:"); bar(13, view.stamina);
    renderer_.drawText(RIGHT, 15, L"智慧 WIS: " + std::to_wstring(view.wisdom));
    renderer_.drawText(RIGHT, 16, L"力量 STR: " + std::to_wstring(view.strength));
    renderer_.drawText(RIGHT, 17, L"声望: " + std::to_wstring(view.reputation));
    renderer_.drawText(RIGHT, 18, L"背包格数: " + std::to_wstring(view.inventorySlots) + L"/8");
    renderer_.drawText(RIGHT, 21, L"【快捷命令】", Color::Title);
    renderer_.drawText(RIGHT, 22, L"look"); renderer_.drawText(86, 22, L"go"); renderer_.drawText(98, 22, L"talk");
    renderer_.drawText(RIGHT, 23, L"take"); renderer_.drawText(86, 23, L"bag"); renderer_.drawText(98, 23, L"use");
    renderer_.drawText(RIGHT, 24, L"help", Color::Hint);
    renderer_.drawText(1, 26, L"请输入 command:（Enter 执行，Ctrl+C 退出）", Color::Hint);
    renderer_.drawText(1, INPUT_ROW, L"> ");
    renderer_.drawText(1, 28, L"方向键编辑；长命令在输入行内横向滚动。", Color::Hint);
    renderer_.drawFrame(); // ALWAYS last: text cannot determine border coordinates.
    return true;
}
void GameUI::drawInput(const std::vector<std::wstring>& glyphs, std::size_t caret) {
    constexpr int available = UI_WIDTH - 4; // reserve one cell for end-of-line caret
    std::size_t start = caret;
    int before = 0;
    while (start > 0) {
        const int width = renderer_.columns(glyphs[start - 1]);
        if (before + width > available) break;
        before += width; --start;
    }
    renderer_.clearInput();
    renderer_.drawText(1, INPUT_ROW, L"> ");
    renderer_.drawText(3, INPUT_ROW, renderer_.clip(join(glyphs, start, glyphs.size()), available));
    renderer_.showCursor(static_cast<SHORT>(3 + before), INPUT_ROW, true);
}
std::optional<std::wstring> GameUI::readCommand() {
    std::vector<std::wstring> input;
    std::size_t caret = 0;
    drawInput(input, caret);
    while (true) {
        const auto event = renderer_.readEvent();
        if (!frameReady_ && event.key != Key::Resize && event.key != Key::EndOfInput) continue;
        switch (event.key) {
            case Key::EndOfInput: return std::nullopt;
            case Key::Enter: {
                auto command = join(input, 0, input.size());
                appendLog(L"> " + command);
                return command;
            }
            case Key::Text: {
                auto prefix = splitGlyphs(join(input, 0, caret) + event.text.substr(0, 4096));
                prefix.erase(std::remove(prefix.begin(), prefix.end(), L"\n"), prefix.end());
                const auto tail = input.size() - caret;
                if (prefix.size() + tail > 1024) break;
                prefix.insert(prefix.end(), input.begin() + static_cast<std::ptrdiff_t>(caret), input.end());
                caret = prefix.size() - tail;
                input = std::move(prefix);
                break;
            }
            case Key::Backspace:
                if (caret > 0) { input.erase(input.begin() + static_cast<std::ptrdiff_t>(caret - 1)); --caret; }
                break;
            case Key::Delete:
                if (caret < input.size()) input.erase(input.begin() + static_cast<std::ptrdiff_t>(caret));
                break;
            case Key::Left: if (caret > 0) --caret; break;
            case Key::Right: if (caret < input.size()) ++caret; break;
            case Key::Home: caret = 0; break;
            case Key::End: caret = input.size(); break;
            case Key::PageUp: scrollBack_ += 22; render(lastView_); break;
            case Key::PageDown: scrollBack_ = scrollBack_ > 22 ? scrollBack_ - 22 : 0; render(lastView_); break;
            case Key::Resize: render(lastView_); break;
        }
        drawInput(input, caret);
    }
}
} // namespace UI

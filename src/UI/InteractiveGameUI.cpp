#include "UI/InteractiveGameUI.h"

#include "CombatSystem.h"
#include "Player.h"
#include "Room.h"
#include "UI/TextLayout.h"
#include "WorldState.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace UI {
namespace {
constexpr Rect LEFT_LOG{1, 19, DIVIDER_X - 1, INPUT_TOP - 1};
constexpr Rect RIGHT_PANEL{DIVIDER_X + 1, 1, UI_WIDTH - 1, INPUT_TOP - 1};

Color messageColor(const std::wstring& text) {
    if (text.find(L"【成就解锁】") != std::wstring::npos ||
        text.find(L"【成就收集】") != std::wstring::npos)
        return Color::Achievement;
    if (text.find(L"【结局达成】") != std::wstring::npos ||
        text.find(L"【隐藏结局") != std::wstring::npos ||
        text.find(L"【坏结局") != std::wstring::npos ||
        text.find(L"【普通结局") != std::wstring::npos ||
        text.find(L"【结局：") != std::wstring::npos)
        return Color::Ending;
    if (text.find(L"失败") != std::wstring::npos ||
        text.find(L"无法") != std::wstring::npos ||
        text.find(L"不足") != std::wstring::npos) return Color::Error;
    if (text.find(L"闪尾") != std::wstring::npos ||
        text.find(L"岩背") != std::wstring::npos ||
        text.find(L"叶婆婆") != std::wstring::npos ||
        text.find(L"豆豆") != std::wstring::npos ||
        text.find(L"赫兹") != std::wstring::npos ||
        text.find(L"拾取") != std::wstring::npos ||
        text.find(L"获得") != std::wstring::npos)
        return Color::Item;
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

std::wstring seasonName(int turn) {
    static const wchar_t* seasons[] = {L"春", L"夏", L"秋", L"冬"};
    return seasons[(turn / 6) % 4];
}

void replaceAll(std::wstring& text, const std::wstring& from,
                const std::wstring& to) {
    std::size_t position = 0;
    while ((position = text.find(from, position)) != std::wstring::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
}

std::wstring cinematicSpacing(const std::wstring& text) {
    std::wstring spaced;
    spaced.reserve(text.size() + text.size() / 8);
    const auto punctuation = [](wchar_t ch) {
        return ch == L'。' || ch == L'！' || ch == L'？';
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        const wchar_t ch = text[i];
        spaced.push_back(ch);
        const bool sentenceEnd = punctuation(ch) ||
            ((ch == L'”' || ch == L'」') && i > 0 && punctuation(text[i - 1]));
        if (sentenceEnd && i + 1 < text.size() && text[i + 1] != L'\n')
            spaced.push_back(L'\n');
    }
    return spaced;
}
}

InteractiveGameUI::InteractiveGameUI(ConsoleRenderer& renderer)
    : renderer_(renderer) {}

void InteractiveGameUI::drawStableLine(Rect area, SHORT y,
                                       const std::wstring& text,
                                       Color color) {
    const int width = area.right - area.left + 1;
    std::wstring padded = renderer_.clip(text, width);
    padded += std::wstring(static_cast<std::size_t>(std::max(
        0, width - renderer_.columns(padded))), L' ');
    const int key = static_cast<int>(area.left) * 100 + y;
    const auto previous = lastStableRows_.find(key);
    if (previous != lastStableRows_.end() &&
        previous->second.text == padded && previous->second.color == color)
        return;
    renderer_.drawTextIn(area, area.left, y, padded, color);
    lastStableRows_[key] = {padded, color};
}

void InteractiveGameUI::appendLog(const std::string& text) {
    historyScrollBack_ = 0;
    std::wstring wide = fromUtf8(text);
    replaceAll(wide, L"隐藏成就解锁：", L"\n【成就解锁】");
    replaceAll(wide, L"隐藏结局：", L"\n【结局达成】");
    replaceAll(wide, L"坏结局：", L"\n【结局达成】");
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
    const bool fullRedraw = needsFullClear_;
    if (!renderer_.beginFrame(fullRedraw)) return false;
    needsFullClear_ = false;
    if (fullRedraw) lastStableRows_.clear();

    std::wstring mapTitle = L"【" + roomName(ctx) + L"】互动地图";
    const int titlePadding = std::max(0, DIVIDER_X - 2 - renderer_.columns(mapTitle));
    mapTitle += std::wstring(static_cast<std::size_t>(titlePadding), L' ');
    renderer_.drawText(1, 1, mapTitle, Color::Title);
    std::vector<MapTileVisual> currentTiles;
    currentTiles.reserve(static_cast<std::size_t>(map.width() * map.height()));
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const MapTileVisual tile = map.visualAt(x, y, ctx);
            const std::size_t index = currentTiles.size();
            currentTiles.push_back(tile);
            if (fullRedraw || index >= lastMapTiles_.size() ||
                tile.glyph != lastMapTiles_[index].glyph ||
                tile.color != lastMapTiles_[index].color)
                renderer_.drawText(static_cast<SHORT>(1 + x * 2),
                                   static_cast<SHORT>(2 + y), tile.glyph,
                                   tile.color);
        }
    }
    lastMapTiles_ = std::move(currentTiles);
    renderer_.drawHorizontalLine(18, 0, DIVIDER_X);
    renderer_.drawText(1, 18,
                       historyScrollBack_ == 0 ? L"+ 剧情记录 "
                                               : L"+ 剧情记录（正在回看） ",
                       Color::Title);
    const std::size_t capacity = LEFT_LOG.bottom - LEFT_LOG.top + 1;
    const std::size_t maxBack = history_.size() > capacity
                                    ? history_.size() - capacity : 0;
    historyScrollBack_ = std::min(historyScrollBack_, maxBack);
    const std::size_t end = history_.size() - historyScrollBack_;
    const std::size_t first = end > capacity ? end - capacity : 0;
    for (std::size_t row = 0; row < capacity; ++row) {
        const std::size_t index = first + row;
        const bool hasLine = index < end;
        drawStableLine(LEFT_LOG, static_cast<SHORT>(LEFT_LOG.top + row),
                       hasLine ? history_[index].text : L"",
                       hasLine ? history_[index].color : Color::Normal);
    }

    const SHORT right = DIVIDER_X + 2;
    renderer_.drawText(right, 1, L"【小地图】", Color::Title);
    renderer_.drawText(right, 2, L"王--林--河--基", Color::Normal);
    renderer_.drawText(right, 3, L"   |", Color::Wall);
    renderer_.drawText(right, 4, L"   洞", Color::Normal);
    drawStableLine(RIGHT_PANEL, 5,
                   L"当前位置：" + roomShort(ctx.player.getCurrentRoomId()) +
                       L" / " + roomName(ctx), Color::Hint);
    renderer_.drawText(right, 6, L"门=切图  锁=未解锁", Color::Door);

    renderer_.drawText(right, 9, L"【状态】", Color::Title);
    drawStableLine(RIGHT_PANEL, 10,
                   L"生命 " + progressBar(ctx.player.getHealth()),
                   ctx.player.getHealth() <= 30 ? Color::Error : Color::Success);
    drawStableLine(RIGHT_PANEL, 11,
                   L"体力 " + progressBar(ctx.player.getStamina()), Color::Success);
    drawStableLine(RIGHT_PANEL, 12,
                   L"力量 " + std::to_wstring(ctx.player.getStrength()) +
                       L"  智慧 " + std::to_wstring(ctx.player.getWisdom()), Color::Normal);
    drawStableLine(RIGHT_PANEL, 13,
                   L"声望 " + std::to_wstring(ctx.player.getReputation()) +
                       L"  阶段 " + std::to_wstring(ctx.world.getStage()) +
                       L"  " + seasonName(ctx.world.getTurnCount()) + L"季", Color::Normal);
    drawStableLine(RIGHT_PANEL, 14,
                   L"食物 " + std::to_wstring(ctx.world.getResource(ResourceType::Food)) +
                       L"  水 " + std::to_wstring(ctx.world.getResource(ResourceType::Water)),
                   Color::Normal);
    if (combat.isInBattle()) {
        drawStableLine(RIGHT_PANEL, 15,
                       L"战斗：" + fromUtf8(combat.getBattleState().enemyId) +
                           L" HP " + std::to_wstring(combat.getBattleState().enemyHealth),
                       Color::Error);
    } else {
        drawStableLine(RIGHT_PANEL, 15, L"背包：" +
                       std::to_wstring(ctx.player.getInventory().getItems().size()) +
                       L"/" + std::to_wstring(Inventory::MAX_SLOTS), Color::Normal);
    }
    renderer_.drawText(right, 16, L"【当前目标】", Color::Title);
    const auto objectiveLines = renderer_.wrapText(fromUtf8(objective),
                                                   UI_WIDTH - right);
    for (std::size_t i = 0; i < 3; ++i)
        drawStableLine(RIGHT_PANEL, static_cast<SHORT>(17 + i),
                       i < objectiveLines.size() ? objectiveLines[i] : L"",
                       Color::Hint);

    renderer_.drawText(right, 21, L"【图例/操作】", Color::Title);
    renderer_.drawText(right, 22, L"猴玩家  友/伴NPC  红！主线", Color::Player);
    renderer_.drawText(right, 23, L"物/宝拾取  敌战斗  奇事件", Color::Item);
    renderer_.drawText(right, 24, L"WASD移动  Enter/空格互动", Color::Hint);

    const Rect bottom{1, 26, UI_WIDTH - 1, UI_HEIGHT - 2};
    drawStableLine(bottom, 26,
                   combat.isInBattle()
                       ? L"战斗模式：请键入攻击/防御/偷窃/使用/逃跑"
                       : L"I背包 U物品 P存档 H帮助 PgUp/PgDn翻剧情 Esc菜单",
                   Color::Hint);
    drawStableLine(bottom, 27, fromUtf8(map.nearbyHint(ctx)), Color::Hint);
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
        if (event.key == Key::PageUp) {
            historyScrollBack_ += 6;
            return ExploreAction::HistoryUp;
        }
        if (event.key == Key::PageDown) {
            historyScrollBack_ = historyScrollBack_ > 6
                                     ? historyScrollBack_ - 6 : 0;
            return ExploreAction::HistoryDown;
        }
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
        if (key == L'4') return ExploreAction::Choice4;
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
        if (event.key == Key::EndOfInput) { needsFullClear_ = true; return -1; }
        if (event.key == Key::Escape) { needsFullClear_ = true; return -1; }
        const int optionCount = static_cast<int>(options.size());
        if (event.key == Key::Up) selected = (selected + optionCount - 1) % optionCount;
        if (event.key == Key::Down) selected = (selected + 1) % optionCount;
        if (event.key == Key::Enter) { needsFullClear_ = true; return selected; }
        if (event.key == Key::Text && !event.text.empty()) {
            wchar_t key = event.text.front();
            if (key == L'w' || key == L'W')
                selected = (selected + optionCount - 1) % optionCount;
            else if (key == L's' || key == L'S')
                selected = (selected + 1) % optionCount;
            else if (key == L' ') { needsFullClear_ = true; return selected; }
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
    const auto lines = renderer_.wrapText(text, UI_WIDTH - 10);
    constexpr std::size_t pageSize = 19;
    std::size_t offset = 0;
    const auto draw = [&]() {
        renderer_.beginFrame();
        centered(2, title, Color::Title);
        for (std::size_t i = 0; i < pageSize && offset + i < lines.size(); ++i)
            renderer_.drawText(5, static_cast<SHORT>(5 + i), lines[offset + i],
                               messageColor(lines[offset + i]));
        centered(27, L"↑↓滚动  PgUp/PgDn翻页  Enter/空格/Esc返回", Color::Hint);
    };
    draw();
    while (true) {
        const InputEvent event = renderer_.readEvent();
        if (event.key == Key::Enter || event.key == Key::Escape ||
            event.key == Key::EndOfInput ||
            (event.key == Key::Text && event.text == L" ")) {
            needsFullClear_ = true;
            return;
        }
        const std::size_t maxOffset = lines.size() > pageSize
                                          ? lines.size() - pageSize : 0;
        if (event.key == Key::Up && offset > 0) --offset;
        else if (event.key == Key::Down && offset < maxOffset) ++offset;
        else if (event.key == Key::PageUp)
            offset = offset > pageSize ? offset - pageSize : 0;
        else if (event.key == Key::PageDown)
            offset = std::min(maxOffset, offset + pageSize);
        else if (event.key != Key::Resize) continue;
        draw();
    }
}

void InteractiveGameUI::showEndingCinematic(const std::wstring& title,
                                             const std::wstring& text) {
    const auto lines = renderer_.wrapText(cinematicSpacing(text), UI_WIDTH - 16);
    constexpr std::size_t pageSize = 18;
    std::size_t first = 0;
    renderer_.beginFrame();
    centered(2, title, Color::Title);
    for (std::size_t revealed = 0; revealed < lines.size(); ++revealed) {
        if (revealed >= first + pageSize) {
            first = revealed - pageSize + 1;
            renderer_.beginFrame();
            centered(2, title, Color::Title);
            for (std::size_t i = first; i <= revealed; ++i)
                renderer_.drawText(8, static_cast<SHORT>(5 + i - first), lines[i],
                                   messageColor(lines[i]));
        } else {
            renderer_.drawText(8, static_cast<SHORT>(5 + revealed - first),
                               lines[revealed], messageColor(lines[revealed]));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(180));
    }

    std::size_t offset = first;
    const auto draw = [&]() {
        renderer_.beginFrame();
        centered(2, title, Color::Title);
        for (std::size_t i = 0; i < pageSize && offset + i < lines.size(); ++i)
            renderer_.drawText(8, static_cast<SHORT>(5 + i), lines[offset + i],
                               messageColor(lines[offset + i]));
        centered(27, L"谢幕完毕 · ↑↓回看 · PgUp/PgDn翻页 · Enter返回", Color::Hint);
    };
    draw();
    while (true) {
        const InputEvent event = renderer_.readEvent();
        if (event.key == Key::Enter || event.key == Key::Escape ||
            event.key == Key::EndOfInput ||
            (event.key == Key::Text && event.text == L" ")) {
            needsFullClear_ = true;
            return;
        }
        const std::size_t maxOffset = lines.size() > pageSize
                                          ? lines.size() - pageSize : 0;
        if (event.key == Key::Up && offset > 0) --offset;
        else if (event.key == Key::Down && offset < maxOffset) ++offset;
        else if (event.key == Key::PageUp)
            offset = offset > pageSize ? offset - pageSize : 0;
        else if (event.key == Key::PageDown)
            offset = std::min(maxOffset, offset + pageSize);
        else if (event.key != Key::Resize) continue;
        draw();
    }
}

void InteractiveGameUI::clearLog() {
    history_.clear();
    historyScrollBack_ = 0;
    lastStableRows_.clear();
    needsFullClear_ = true;
}

}  // namespace UI

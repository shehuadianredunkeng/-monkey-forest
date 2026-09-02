#define NOMINMAX
#include "ConsoleUI.h"

#include "CombatSystem.h"
#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <windows.h>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {
constexpr SHORT kRightX = 80;
constexpr SHORT kLastX = 119;
constexpr SHORT kInputTop = 29;
constexpr SHORT kBottom = 31;
constexpr int kLeftTextWidth = 76;
constexpr int kRightTextWidth = 35;

constexpr WORD kNormal = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD kBorder = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD kTitle = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD kTask = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD kDanger = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD kMuted = FOREGROUND_BLUE | FOREGROUND_GREEN;

HANDLE output() {
    static HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    return handle;
}

std::wstring fromUtf8(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0);
    if (length <= 0) return std::wstring(text.begin(), text.end());
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

int cellWidth(wchar_t ch) {
    if ((ch >= 0x1100 && ch <= 0x115F) ||
        (ch >= 0x2E80 && ch <= 0xA4CF) ||
        (ch >= 0xAC00 && ch <= 0xD7A3) ||
        (ch >= 0xF900 && ch <= 0xFAFF) ||
        (ch >= 0xFE10 && ch <= 0xFE6F) ||
        (ch >= 0xFF01 && ch <= 0xFF60)) return 2;
    return 1;
}

std::wstring clip(const std::wstring& text, int maxCells) {
    std::wstring result;
    int cells = 0;
    for (wchar_t ch : text) {
        const int width = cellWidth(ch);
        if (cells + width > maxCells) break;
        result.push_back(ch);
        cells += width;
    }
    return result;
}

std::vector<std::wstring> wrap(const std::wstring& text, int maxCells) {
    std::vector<std::wstring> lines;
    std::wstring line;
    int cells = 0;
    for (wchar_t ch : text) {
        if (ch == L'\r') continue;
        if (ch == L'\n') {
            lines.push_back(line);
            line.clear();
            cells = 0;
            continue;
        }
        const int width = cellWidth(ch);
        if (cells + width > maxCells) {
            lines.push_back(line);
            line.clear();
            cells = 0;
        }
        line.push_back(ch);
        cells += width;
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

void moveTo(SHORT x, SHORT y) {
    SetConsoleCursorPosition(output(), {x, y});
}

void writeAt(SHORT x, SHORT y, const std::wstring& text,
             int maxCells, WORD attributes = kNormal) {
    SetConsoleTextAttribute(output(), attributes);
    moveTo(x, y);
    const std::wstring shown = clip(text, maxCells);
    DWORD written = 0;
    WriteConsoleW(output(), shown.c_str(), static_cast<DWORD>(shown.size()),
                  &written, nullptr);
}

void repeatAt(SHORT x, SHORT y, wchar_t ch, SHORT count) {
    writeAt(x, y, std::wstring(static_cast<size_t>(count), ch), count, kBorder);
}

void clearScreen() {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    GetConsoleScreenBufferInfo(output(), &info);
    DWORD written = 0;
    const DWORD cells = static_cast<DWORD>(info.dwSize.X) * info.dwSize.Y;
    FillConsoleOutputCharacterW(output(), L' ', cells, {0, 0}, &written);
    FillConsoleOutputAttribute(output(), kNormal, cells, {0, 0}, &written);
}

void drawFrame() {
    writeAt(0, 0, L"+", 1, kBorder);
    repeatAt(1, 0, L'-', kRightX - 1);
    writeAt(kRightX, 0, L"+", 1, kBorder);
    repeatAt(kRightX + 1, 0, L'-', kLastX - kRightX - 1);
    writeAt(kLastX, 0, L"+", 1, kBorder);

    for (SHORT y = 1; y < kInputTop; ++y) {
        writeAt(0, y, L"|", 1, kBorder);
        writeAt(kRightX, y, L"|", 1, kBorder);
        writeAt(kLastX, y, L"|", 1, kBorder);
    }
    for (SHORT dividerY : {8, 16, 22}) {
        writeAt(kRightX, dividerY, L"+", 1, kBorder);
        repeatAt(kRightX + 1, dividerY, L'-', kLastX - kRightX - 1);
        writeAt(kLastX, dividerY, L"+", 1, kBorder);
    }

    writeAt(0, kInputTop, L"+", 1, kBorder);
    repeatAt(1, kInputTop, L'-', kLastX - 1);
    writeAt(kLastX, kInputTop, L"+", 1, kBorder);
    writeAt(0, kInputTop + 1, L"|", 1, kBorder);
    writeAt(kLastX, kInputTop + 1, L"|", 1, kBorder);
    writeAt(0, kBottom, L"+", 1, kBorder);
    repeatAt(1, kBottom, L'-', kLastX - 1);
    writeAt(kLastX, kBottom, L"+", 1, kBorder);
}

std::wstring roomName(const GameContext& ctx) {
    const auto found = ctx.rooms.find(ctx.player.getCurrentRoomId());
    if (found == ctx.rooms.end()) return fromUtf8(ctx.player.getCurrentRoomId());
    return fromUtf8(found->second.getName());
}

std::wstring bar(int value, int maximum, int blocks = 10) {
    value = std::max(0, std::min(value, maximum));
    const int filled = maximum == 0 ? 0 : value * blocks / maximum;
    return L"[" + std::wstring(filled, L'#') +
           std::wstring(blocks - filled, L'-') + L"]";
}

std::wstring marker(const GameContext& ctx, const std::string& roomId,
                    const wchar_t* name) {
    return std::wstring(ctx.player.getCurrentRoomId() == roomId ? L"*" : L" ") +
           name;
}
}

ConsoleUI::ConsoleUI() {
    SetConsoleTitleW(L"吗喽森林：第一年试玩版");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    GetConsoleScreenBufferInfo(output(), &info);
    SetConsoleScreenBufferSize(
        output(), {std::max<SHORT>(kLastX + 1, info.dwSize.X),
                   std::max<SHORT>(kBottom + 2, info.dwSize.Y)});
    SMALL_RECT window{0, 0, kLastX, kBottom + 1};
    SetConsoleWindowInfo(output(), TRUE, &window);
}

void ConsoleUI::appendLog(const std::string& utf8Text) {
    if (utf8Text.empty()) return;
    const auto lines = wrap(fromUtf8(utf8Text), kLeftTextWidth);
    history_.insert(history_.end(), lines.begin(), lines.end());
    if (history_.size() > 300)
        history_.erase(history_.begin(), history_.end() - 300);
}

void ConsoleUI::render(const GameContext& ctx,
                       const CombatSystem& combat,
                       const std::string& guideText) {
    CONSOLE_CURSOR_INFO cursor{20, FALSE};
    SetConsoleCursorInfo(output(), &cursor);
    clearScreen();

    writeAt(2, 1, L"吗喽森林：第一年·春夏", kLeftTextWidth, kTitle);
    const int logRows = 26;
    const size_t start = history_.size() > static_cast<size_t>(logRows)
                             ? history_.size() - logRows
                             : 0;
    SHORT y = 2;
    for (size_t i = start; i < history_.size() && y < kInputTop; ++i, ++y) {
        WORD shade = history_[i].find(L"失败") != std::wstring::npos ||
                             history_[i].find(L"无法") != std::wstring::npos
                         ? kDanger
                         : kNormal;
        writeAt(2, y, history_[i], kLeftTextWidth, shade);
    }

    writeAt(kRightX + 2, 1, L"【地图 Map】", kRightTextWidth, kTitle);
    writeAt(kRightX + 2, 2, marker(ctx, "room_tree", L"猴王树") +
                                    L" -- " + marker(ctx, "room_forest", L"果实森林"),
            kRightTextWidth);
    writeAt(kRightX + 2, 3, L"             |          /", kRightTextWidth, kMuted);
    writeAt(kRightX + 2, 4, marker(ctx, "room_cave", L"回声山洞") +
                                    L" -- " + marker(ctx, "room_river", L"清泉河谷"),
            kRightTextWidth);
    writeAt(kRightX + 2, 5, L"                         |", kRightTextWidth, kMuted);
    writeAt(kRightX + 2, 6, marker(ctx, "room_base", L"实验基地") +
                                    L"    *为当前位置", kRightTextWidth);

    writeAt(kRightX + 2, 9, L"【状态 Status】", kRightTextWidth, kTitle);
    writeAt(kRightX + 2, 10, L"地点：" + roomName(ctx) + L"  阶段：" +
                                      std::to_wstring(ctx.world.getStage()),
            kRightTextWidth);
    writeAt(kRightX + 2, 11, L"生命 " + bar(ctx.player.getHealth(), 100) + L" " +
                                      std::to_wstring(ctx.player.getHealth()),
            kRightTextWidth, ctx.player.getHealth() <= 30 ? kDanger : kNormal);
    writeAt(kRightX + 2, 12, L"体力 " + bar(ctx.player.getStamina(), 100) + L" " +
                                      std::to_wstring(ctx.player.getStamina()),
            kRightTextWidth);
    writeAt(kRightX + 2, 13, L"力量 " + std::to_wstring(ctx.player.getStrength()) +
                                      L"  智慧 " + std::to_wstring(ctx.player.getWisdom()) +
                                      L"  声望 " + std::to_wstring(ctx.player.getReputation()),
            kRightTextWidth);
    writeAt(kRightX + 2, 14, L"食物 " + std::to_wstring(ctx.world.getResource(ResourceType::Food)) +
                                      L"  水 " + std::to_wstring(ctx.world.getResource(ResourceType::Water)) +
                                      L"  士气 " + std::to_wstring(ctx.world.getResource(ResourceType::Morale)),
            kRightTextWidth);
    if (combat.isInBattle()) {
        writeAt(kRightX + 2, 15,
                L"战斗中：" + fromUtf8(combat.getBattleState().enemyId) +
                    L"  HP " + std::to_wstring(combat.getBattleState().enemyHealth),
                kRightTextWidth, kDanger);
    }

    writeAt(kRightX + 2, 17, L"【当前任务 Quest】", kRightTextWidth, kTask);
    const auto guideLines = wrap(fromUtf8(guideText), kRightTextWidth);
    for (size_t i = 0; i < guideLines.size() && i < 4; ++i)
        writeAt(kRightX + 2, static_cast<SHORT>(18 + i), guideLines[i],
                kRightTextWidth);

    writeAt(kRightX + 2, 23, L"【快捷命令 Commands】", kRightTextWidth, kTitle);
    writeAt(kRightX + 2, 24, L"移动：go east/west/north/south", kRightTextWidth);
    writeAt(kRightX + 2, 25, L"查看：look / map / guide / status", kRightTextWidth);
    writeAt(kRightX + 2, 26, L"互动：talk 名字 / take / use", kRightTextWidth);
    writeAt(kRightX + 2, 27, L"推进：investigate；选项直接输数字", kRightTextWidth);
    writeAt(kRightX + 2, 28, L"其他：bag / save / load / help", kRightTextWidth);

    writeAt(2, kInputTop + 1, L"请输入 Command：>", 18, kTask);
    drawFrame();
}

std::string ConsoleUI::readCommand() {
    CONSOLE_CURSOR_INFO cursor{20, TRUE};
    SetConsoleCursorInfo(output(), &cursor);
    moveTo(20, kInputTop + 1);
    SetConsoleTextAttribute(output(), kNormal);
    std::string line;
    std::getline(std::cin, line);
    return line;
}

void ConsoleUI::restoreCursor() {
    CONSOLE_CURSOR_INFO cursor{20, TRUE};
    SetConsoleCursorInfo(output(), &cursor);
    SetConsoleTextAttribute(output(), kNormal);
    moveTo(0, kBottom + 1);
}

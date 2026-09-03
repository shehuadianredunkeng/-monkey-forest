#include "UI/ConsoleRenderer.h"

#include <algorithm>
#include <map>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace UI {
namespace {
#ifdef _WIN32
class WindowsSurface final : public ConsoleSurface {
public:
    WindowsSurface() {
        output_ = GetStdHandle(STD_OUTPUT_HANDLE);
        input_ = GetStdHandle(STD_INPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleMode(output_, &oldOutputMode_) || !GetConsoleMode(input_, &oldInputMode_) ||
            !GetConsoleScreenBufferInfo(output_, &info) || !GetConsoleCursorInfo(output_, &oldCursor_)) return;
        saved_ = true;
        oldAttributes_ = info.wAttributes;
        oldOutputCP_ = GetConsoleOutputCP(); oldInputCP_ = GetConsoleCP();
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        // No native line echo or wrapping: input and bottom-right writes cannot scroll the frame.
        const DWORD inMode = (oldInputMode_ | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) &
            ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
        const DWORD outMode = (oldOutputMode_ | ENABLE_PROCESSED_OUTPUT) &
            ~(ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        valid_ = SetConsoleMode(input_, inMode) && SetConsoleMode(output_, outMode);
    }
    ~WindowsSurface() override { restore(); }

    bool prepare() override {
        if (!valid_) return false;
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(output_, &info)) return false;
        const COORD size{std::max<SHORT>(info.dwSize.X, REQUIRED_COLUMNS),
                         std::max<SHORT>(info.dwSize.Y, UI_HEIGHT)};
        if (size.X != info.dwSize.X || size.Y != info.dwSize.Y) SetConsoleScreenBufferSize(output_, size);
        // Keep larger windows; only grow/anchor the viewport when needed.
        const SMALL_RECT window{0, 0,
            std::max<SHORT>(UI_WIDTH, info.srWindow.Right - info.srWindow.Left),
            std::max<SHORT>(UI_HEIGHT - 1, info.srWindow.Bottom - info.srWindow.Top)};
        if (info.srWindow.Left != 0 || info.srWindow.Top != 0 ||
            info.srWindow.Right != window.Right || info.srWindow.Bottom != window.Bottom)
            SetConsoleWindowInfo(output_, TRUE, &window);
        if (!GetConsoleScreenBufferInfo(output_, &info)) return false;
        fits_ = info.srWindow.Left == 0 && info.srWindow.Top == 0 &&
                info.srWindow.Right >= UI_WIDTH && info.srWindow.Bottom >= UI_HEIGHT - 1;
        widths_.clear();
        font_ = CONSOLE_FONT_INFOEX{};
        font_.cbSize = sizeof(font_);
        GetCurrentConsoleFontEx(output_, FALSE, &font_);
        if (measure_ != INVALID_HANDLE_VALUE) CloseHandle(measure_);
        // Inactive buffer inherits the real console's font. Never displayed.
        measure_ = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
        if (measure_ != INVALID_HANDLE_VALUE) {
            SetConsoleMode(measure_, ENABLE_PROCESSED_OUTPUT); // disable wrap / VT
        }
        if (!fits_ || measure_ == INVALID_HANDLE_VALUE) {
            fits_ = false;
            clear({0, 0, static_cast<SHORT>(info.dwSize.X - 1), static_cast<SHORT>(info.dwSize.Y - 1)});
            const std::wstring message = measure_ == INVALID_HANDLE_VALUE ?
                L"无法建立文字测量缓冲区。请使用 Windows 控制台，按 Ctrl+C 退出。" :
                L"请扩大窗口至至少111列、30行。按 Ctrl+C 退出。";
            // Known CJK/ASCII diagnostic, conservatively clipped even if measurement is unavailable.
            const int cells = info.srWindow.Right - info.srWindow.Left;
            write(info.srWindow.Left, info.srWindow.Top, clipText(message, cells, portableColumns), Color::Error);
        }
        return fits_;
    }

    void clear(Rect area) override {
        if (!valid_) return;
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(output_, &info)) return;
        area.left = std::max<SHORT>(0, area.left); area.top = std::max<SHORT>(0, area.top);
        area.right = std::min<SHORT>(area.right, info.dwSize.X - 1);
        area.bottom = std::min<SHORT>(area.bottom, info.dwSize.Y - 1);
        if (area.left > area.right || area.top > area.bottom) return;
        for (SHORT y = area.top; y <= area.bottom; ++y) {
            DWORD written = 0;
            const DWORD cells = area.right - area.left + 1;
            FillConsoleOutputCharacterW(output_, L' ', cells, {area.left, y}, &written);
            FillConsoleOutputAttribute(output_, static_cast<WORD>(Color::Normal), cells, {area.left, y}, &written);
        }
    }
    void write(SHORT x, SHORT y, const std::wstring& text, Color color) override {
        if (!valid_ || text.empty()) return;
        if (!SetConsoleTextAttribute(output_, static_cast<WORD>(color)) ||
            !SetConsoleCursorPosition(output_, {x, y})) return;
        DWORD written = 0;
        WriteConsoleW(output_, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    }
    void cursor(SHORT x, SHORT y, bool visible) override {
        if (!valid_) return;
        CONSOLE_CURSOR_INFO cursorInfo{oldCursor_.dwSize, visible ? TRUE : FALSE};
        SetConsoleCursorInfo(output_, &cursorInfo);
        SetConsoleCursorPosition(output_, {x, y});
    }
    int measure(const std::wstring& glyph) override {
        const auto found = widths_.find(glyph);
        if (found != widths_.end()) return found->second;
        if (measure_ == INVALID_HANDLE_VALUE || glyph.size() > 256) return REQUIRED_COLUMNS;
        DWORD written = 0;
        FillConsoleOutputCharacterW(measure_, L' ', REQUIRED_COLUMNS, {0, 0}, &written);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!SetConsoleCursorPosition(measure_, {0, 0}) ||
            !WriteConsoleW(measure_, glyph.data(), static_cast<DWORD>(glyph.size()), &written, nullptr) ||
            written != glyph.size() || !GetConsoleScreenBufferInfo(measure_, &info)) return REQUIRED_COLUMNS;
        const int cells = info.dwCursorPosition.Y * info.dwSize.X + info.dwCursorPosition.X;
        widths_[glyph] = std::max(1, cells);
        return widths_[glyph];
    }
    InputEvent readEvent() override {
        if (!valid_) return {Key::EndOfInput, {}};
        if (repeat_ > 0) { --repeat_; return repeated_; }
        while (true) {
            INPUT_RECORD input{}; DWORD count = 0;
            if (!ReadConsoleInputW(input_, &input, 1, &count) || count != 1) return {Key::EndOfInput, {}};
            if (input.EventType == WINDOW_BUFFER_SIZE_EVENT) return {Key::Resize, {}};
            if (input.EventType != KEY_EVENT || !input.Event.KeyEvent.bKeyDown) continue;
            const auto& event = input.Event.KeyEvent;
            if ((event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) &&
                (event.wVirtualKeyCode == 'C' || event.wVirtualKeyCode == 'Z')) return {Key::EndOfInput, {}};
            // A viewport-only resize/font change need not produce a buffer-size event.
            // Recheck before accepting a command, so hidden UI can never advance gameplay.
            CONSOLE_SCREEN_BUFFER_INFO current{};
            CONSOLE_FONT_INFOEX font{}; font.cbSize = sizeof(font);
            if (!GetConsoleScreenBufferInfo(output_, &current)) return {Key::EndOfInput, {}};
            if (current.srWindow.Left != 0 || current.srWindow.Top != 0 ||
                current.srWindow.Right < UI_WIDTH || current.srWindow.Bottom < UI_HEIGHT - 1)
                return {Key::Resize, {}};
            if (GetCurrentConsoleFontEx(output_, FALSE, &font) &&
                (font.dwFontSize.X != font_.dwFontSize.X || font.dwFontSize.Y != font_.dwFontSize.Y ||
                 font.FontFamily != font_.FontFamily || font.FontWeight != font_.FontWeight ||
                 std::wstring(font.FaceName) != std::wstring(font_.FaceName))) return {Key::Resize, {}};
            InputEvent result{Key::Text, {}};
            switch (event.wVirtualKeyCode) {
                case VK_RETURN: result.key = Key::Enter; break;
                case VK_BACK: result.key = Key::Backspace; break;
                case VK_DELETE: result.key = Key::Delete; break;
                case VK_LEFT: result.key = Key::Left; break;
                case VK_RIGHT: result.key = Key::Right; break;
                case VK_HOME: result.key = Key::Home; break;
                case VK_END: result.key = Key::End; break;
                case VK_PRIOR: result.key = Key::PageUp; break;
                case VK_NEXT: result.key = Key::PageDown; break;
                default: {
                    const wchar_t ch = event.uChar.UnicodeChar;
                    if (ch == 0 || ch < L' ') continue;
                    if (ch >= 0xd800 && ch <= 0xdbff) { highSurrogate_ = ch; continue; }
                    if (ch >= 0xdc00 && ch <= 0xdfff && highSurrogate_) result.text.push_back(highSurrogate_);
                    highSurrogate_ = 0;
                    result.text.push_back(ch);
                    break;
                }
            }
            repeat_ = event.wRepeatCount > 0 ? event.wRepeatCount - 1 : 0;
            repeated_ = result;
            return result;
        }
    }
    void restore() override {
        if (measure_ != INVALID_HANDLE_VALUE) { CloseHandle(measure_); measure_ = INVALID_HANDLE_VALUE; }
        if (!saved_) return;
        SetConsoleMode(input_, oldInputMode_); SetConsoleMode(output_, oldOutputMode_);
        SetConsoleTextAttribute(output_, oldAttributes_);
        SetConsoleCursorInfo(output_, &oldCursor_);
        SetConsoleCursorPosition(output_, {0, UI_HEIGHT - 1});
        SetConsoleOutputCP(oldOutputCP_); SetConsoleCP(oldInputCP_);
        saved_ = false; valid_ = false;
    }
private:
    HANDLE output_ = INVALID_HANDLE_VALUE, input_ = INVALID_HANDLE_VALUE, measure_ = INVALID_HANDLE_VALUE;
    DWORD oldOutputMode_ = 0, oldInputMode_ = 0;
    UINT oldOutputCP_ = 0, oldInputCP_ = 0;
    WORD oldAttributes_ = 7;
    CONSOLE_CURSOR_INFO oldCursor_{};
    CONSOLE_FONT_INFOEX font_{};
    bool valid_ = false, saved_ = false, fits_ = false;
    unsigned int repeat_ = 0;
    wchar_t highSurrogate_ = 0;
    InputEvent repeated_{Key::Text, {}};
    std::map<std::wstring, int> widths_;
};
#else
class UnsupportedSurface final : public ConsoleSurface {
public:
    bool prepare() override { return false; }
    void clear(Rect) override {}
    void write(SHORT, SHORT, const std::wstring&, Color) override {}
    void cursor(SHORT, SHORT, bool) override {}
    int measure(const std::wstring& glyph) override { return portableColumns(glyph); }
    InputEvent readEvent() override { return {Key::EndOfInput, {}}; }
    void restore() override {}
};
#endif
}

ConsoleRenderer::ConsoleRenderer()
#ifdef _WIN32
    : ConsoleRenderer(std::make_unique<WindowsSurface>()) {}
#else
    : ConsoleRenderer(std::make_unique<UnsupportedSurface>()) {}
#endif
ConsoleRenderer::ConsoleRenderer(std::unique_ptr<ConsoleSurface> surface) : surface_(std::move(surface)) {
    if (!surface_) throw std::invalid_argument("Console surface is required");
}
ConsoleRenderer::~ConsoleRenderer() { restore(); }
bool ConsoleRenderer::beginFrame() {
    frameReady_ = surface_->prepare();
    if (frameReady_) {
        surface_->cursor(1, 1, false);
        surface_->clear({0, 0, UI_WIDTH, UI_HEIGHT - 1});
    }
    return frameReady_;
}
void ConsoleRenderer::moveCursor(SHORT x, SHORT y) { showCursor(x, y, true); }
void ConsoleRenderer::showCursor(SHORT x, SHORT y, bool visible) {
    if (frameReady_ && x >= 0 && x <= UI_WIDTH && y >= 0 && y < UI_HEIGHT) surface_->cursor(x, y, visible);
}
int ConsoleRenderer::columns(const std::wstring& text) {
    return displayColumns(text, [this](const std::wstring& g) { return surface_->measure(g); });
}
std::wstring ConsoleRenderer::clip(const std::wstring& text, int count) {
    return clipText(text, count, [this](const std::wstring& g) { return surface_->measure(g); });
}
std::vector<std::wstring> ConsoleRenderer::wrapText(const std::wstring& text, int count) {
    return UI::wrapText(text, count, [this](const std::wstring& g) { return surface_->measure(g); });
}
void ConsoleRenderer::drawTextIn(Rect area, SHORT x, SHORT y, const std::wstring& text, Color color) {
    // Enforce global borders even if a caller supplies an oversized clipping rectangle.
    if (y <= 0 || y >= UI_HEIGHT - 1 || y == INPUT_TOP || x <= 0 || x >= UI_WIDTH) return;
    area.left = std::max<SHORT>(area.left, 1);
    area.right = std::min<SHORT>(area.right, UI_WIDTH - 1);
    if (y < INPUT_TOP) {
        if (x == DIVIDER_X || (x > DIVIDER_X && (y == 8 || y == 20))) return;
        if (x < DIVIDER_X) area.right = std::min<SHORT>(area.right, DIVIDER_X - 1);
        else area.left = std::max<SHORT>(area.left, DIVIDER_X + 1);
    }
    if (!frameReady_ || x < area.left || y < area.top || y > area.bottom || x > area.right) return;
    for (const auto& glyph : splitGlyphs(text)) {
        if (glyph == L"\n") break;
        const int width = surface_->measure(glyph);
        if (width <= 0 || x + width - 1 > area.right) break;
        surface_->write(x, y, glyph, color);
        x = static_cast<SHORT>(x + width);
    }
}
void ConsoleRenderer::drawText(SHORT x, SHORT y, std::wstring text, Color color) {
    if (y <= 0 || y >= UI_HEIGHT - 1 || y == INPUT_TOP || x <= 0 || x >= UI_WIDTH) return;
    Rect area{1, 1, UI_WIDTH - 1, UI_HEIGHT - 2};
    if (y < INPUT_TOP) {
        if (x == DIVIDER_X || (x > DIVIDER_X && (y == 8 || y == 20))) return;
        if (x < DIVIDER_X) area.right = DIVIDER_X - 1;
        else area.left = DIVIDER_X + 1;
    }
    drawTextIn(area, x, y, text, color);
}
void ConsoleRenderer::drawHorizontalLine(SHORT y, SHORT from, SHORT to) {
    if (!frameReady_ || y < 0 || y >= UI_HEIGHT) return;
    for (SHORT x = std::max<SHORT>(0, from); x <= std::min<SHORT>(UI_WIDTH, to); ++x)
        surface_->write(x, y, L"-", Color::Title);
}
void ConsoleRenderer::drawVerticalLine(SHORT x, SHORT from, SHORT to) {
    if (!frameReady_ || x < 0 || x > UI_WIDTH) return;
    for (SHORT y = std::max<SHORT>(0, from); y <= std::min<SHORT>(UI_HEIGHT - 1, to); ++y)
        surface_->write(x, y, L"|", Color::Title);
}
void ConsoleRenderer::drawFrame() {
    if (!frameReady_) return;
    drawHorizontalLine(0); drawHorizontalLine(INPUT_TOP); drawHorizontalLine(UI_HEIGHT - 1);
    drawVerticalLine(0, 0, UI_HEIGHT - 1); drawVerticalLine(UI_WIDTH, 0, UI_HEIGHT - 1);
    drawVerticalLine();
    drawHorizontalLine(8, DIVIDER_X, UI_WIDTH); drawHorizontalLine(20, DIVIDER_X, UI_WIDTH);
    for (SHORT y : {SHORT(0), INPUT_TOP, SHORT(UI_HEIGHT - 1)}) {
        surface_->write(0, y, L"+", Color::Title); surface_->write(UI_WIDTH, y, L"+", Color::Title);
    }
    for (SHORT y : {SHORT(0), SHORT(8), SHORT(20), INPUT_TOP}) {
        surface_->write(DIVIDER_X, y, L"+", Color::Title);
        if (y == 8 || y == 20) surface_->write(UI_WIDTH, y, L"+", Color::Title);
    }
}
void ConsoleRenderer::clearInput() {
    if (frameReady_) surface_->clear({1, INPUT_ROW, UI_WIDTH - 1, INPUT_ROW});
}
InputEvent ConsoleRenderer::readEvent() { return surface_->readEvent(); }
void ConsoleRenderer::restore() { surface_->restore(); frameReady_ = false; }
}

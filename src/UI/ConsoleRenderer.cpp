#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "UI/ConsoleRenderer.h"

#include <algorithm>
#include <map>
#include <stdexcept>

using namespace std;

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
        // 只按可见窗口布局，回滚缓冲区不算数。
        bounds_ = {0, 0, static_cast<SHORT>(info.srWindow.Right - info.srWindow.Left),
                   static_cast<SHORT>(info.srWindow.Bottom - info.srWindow.Top)};
        origin_ = {info.srWindow.Left, info.srWindow.Top};
        fits_ = bounds_.right >= UI_WIDTH && bounds_.bottom >= 29;
        CONSOLE_FONT_INFOEX currentFont{};
        currentFont.cbSize = sizeof(currentFont);
        if (GetCurrentConsoleFontEx(output_, FALSE, &currentFont) &&
            (!fontReady_ || currentFont.dwFontSize.X != font_.dwFontSize.X ||
             currentFont.dwFontSize.Y != font_.dwFontSize.Y ||
             currentFont.FontFamily != font_.FontFamily ||
             currentFont.FontWeight != font_.FontWeight ||
             wstring(currentFont.FaceName) != wstring(font_.FaceName))) {
            font_ = currentFont;
            fontReady_ = true;
            widths_.clear();
        }
        if (measure_ == INVALID_HANDLE_VALUE) {
            // 不可见缓冲区继承当前字体，只用于测量，不上屏。
            measure_ = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
        }
        if (measure_ != INVALID_HANDLE_VALUE) {
            SetConsoleMode(measure_, ENABLE_PROCESSED_OUTPUT);
        }
        if (!fits_ || measure_ == INVALID_HANDLE_VALUE) {
            fits_ = false;
            clear(bounds_);
            const wstring message = measure_ == INVALID_HANDLE_VALUE ?
                L"无法建立文字测量缓冲区。请使用 Windows 控制台，按 Ctrl+C 退出。" :
                L"请扩大窗口至至少111列、30行。按 Ctrl+C 退出。";
            const int cells = info.srWindow.Right - info.srWindow.Left;
            write(0, 0, clipText(message, cells, portableColumns), Color::Error);
        }
        return fits_;
    }

    Rect viewport() const override { return bounds_; }

    void clear(Rect area) override {
        if (!valid_) return;
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(output_, &info)) return;
        area.left += origin_.X; area.right += origin_.X;
        area.top += origin_.Y; area.bottom += origin_.Y;
        area.left = max<SHORT>(0, area.left); area.top = max<SHORT>(0, area.top);
        area.right = min<SHORT>(area.right, info.dwSize.X - 1);
        area.bottom = min<SHORT>(area.bottom, info.dwSize.Y - 1);
        if (area.left > area.right || area.top > area.bottom) return;
        for (SHORT y = area.top; y <= area.bottom; ++y) {
            DWORD written = 0;
            const DWORD cells = area.right - area.left + 1;
            FillConsoleOutputCharacterW(output_, L' ', cells, {area.left, y}, &written);
            FillConsoleOutputAttribute(output_, static_cast<WORD>(Color::Normal), cells, {area.left, y}, &written);
        }
    }
    void write(SHORT x, SHORT y, const wstring& text, Color color) override {
        if (!valid_ || text.empty()) return;
        if (!SetConsoleTextAttribute(output_, static_cast<WORD>(color)) ||
            !SetConsoleCursorPosition(output_, {static_cast<SHORT>(x + origin_.X), static_cast<SHORT>(y + origin_.Y)})) return;
        DWORD written = 0;
        WriteConsoleW(output_, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    }
    void cursor(SHORT x, SHORT y, bool visible) override {
        if (!valid_) return;
        CONSOLE_CURSOR_INFO cursorInfo{oldCursor_.dwSize, visible ? TRUE : FALSE};
        SetConsoleCursorInfo(output_, &cursorInfo);
        SetConsoleCursorPosition(output_, {static_cast<SHORT>(x + origin_.X), static_cast<SHORT>(y + origin_.Y)});
    }
    int measure(const wstring& glyph) override {
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
        widths_[glyph] = max(1, cells);
        return widths_[glyph];
    }
    InputEvent readEvent() override {
        if (!valid_) return {Key::EndOfInput, {}};
        if (repeat_ > 0) { --repeat_; return repeated_; }
        while (true) {
            // Windows Terminal 拖窗口不一定发缓冲区事件，这里顺带轮询窗口。
            const DWORD wait = WaitForSingleObject(input_, 100);
            if (wait == WAIT_TIMEOUT) {
                CONSOLE_SCREEN_BUFFER_INFO current{};
                if (!GetConsoleScreenBufferInfo(output_, &current)) return {Key::EndOfInput, {}};
                if (current.srWindow.Right - current.srWindow.Left != bounds_.right ||
                    current.srWindow.Bottom - current.srWindow.Top != bounds_.bottom ||
                    current.srWindow.Left != origin_.X || current.srWindow.Top != origin_.Y)
                    return {Key::Resize, {}};
                continue;
            }
            if (wait != WAIT_OBJECT_0) return {Key::EndOfInput, {}};
            INPUT_RECORD input{}; DWORD count = 0;
            if (!ReadConsoleInputW(input_, &input, 1, &count) || count != 1) return {Key::EndOfInput, {}};
            if (input.EventType == WINDOW_BUFFER_SIZE_EVENT) return {Key::Resize, {}};
            if (input.EventType != KEY_EVENT || !input.Event.KeyEvent.bKeyDown) continue;
            const auto& event = input.Event.KeyEvent;
            if ((event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) &&
                (event.wVirtualKeyCode == 'C' || event.wVirtualKeyCode == 'Z')) return {Key::EndOfInput, {}};
            // 只改窗口大小或字体时同样不一定有缓冲区事件；接命令前再确认一次，
            // 界面显示不全时不允许继续推进游戏。
            CONSOLE_SCREEN_BUFFER_INFO current{};
            CONSOLE_FONT_INFOEX font{}; font.cbSize = sizeof(font);
            if (!GetConsoleScreenBufferInfo(output_, &current)) return {Key::EndOfInput, {}};
            if (!fits_ || current.srWindow.Left != origin_.X || current.srWindow.Top != origin_.Y ||
                current.srWindow.Right - current.srWindow.Left != bounds_.right ||
                current.srWindow.Bottom - current.srWindow.Top != bounds_.bottom)
                return {Key::Resize, {}};
            if (GetCurrentConsoleFontEx(output_, FALSE, &font) &&
                (font.dwFontSize.X != font_.dwFontSize.X || font.dwFontSize.Y != font_.dwFontSize.Y ||
                 font.FontFamily != font_.FontFamily || font.FontWeight != font_.FontWeight ||
                 wstring(font.FaceName) != wstring(font_.FaceName))) return {Key::Resize, {}};
            InputEvent result{Key::Text, {}};
            switch (event.wVirtualKeyCode) {
                case VK_RETURN: result.key = Key::Enter; break;
                case VK_BACK: result.key = Key::Backspace; break;
                case VK_DELETE: result.key = Key::Delete; break;
                case VK_LEFT: result.key = Key::Left; break;
                case VK_RIGHT: result.key = Key::Right; break;
                case VK_UP: result.key = Key::Up; break;
                case VK_DOWN: result.key = Key::Down; break;
                case VK_ESCAPE: result.key = Key::Escape; break;
                case VK_HOME: result.key = Key::Home; break;
                case VK_END: result.key = Key::End; break;
                case VK_PRIOR: result.key = Key::PageUp; break;
                case VK_NEXT: result.key = Key::PageDown; break;
                default: {
                    wchar_t ch = event.uChar.UnicodeChar;
                    // 中文输入法会把字母键变成拼音串，字符为空；用虚拟键码回退，
                    // 让 WASD/数字在中英文输入法下都能操作。打字输入不接受该回退。
                    if (ch == 0) {
                        const WORD vk = event.wVirtualKeyCode;
                        if (vk >= 'A' && vk <= 'Z')
                            ch = static_cast<wchar_t>(vk - 'A' + L'a');
                        else if (vk >= '0' && vk <= '9')
                            ch = static_cast<wchar_t>(vk);
                        if (ch == 0) continue;
                        result.key = Key::VirtualText;
                        result.text.push_back(ch);
                        break;
                    }
                    if (ch < L' ') continue;
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
    Rect bounds_{0, 0, UI_WIDTH, UI_HEIGHT - 1};
    COORD origin_{0, 0};
    HANDLE output_ = INVALID_HANDLE_VALUE, input_ = INVALID_HANDLE_VALUE, measure_ = INVALID_HANDLE_VALUE;
    DWORD oldOutputMode_ = 0, oldInputMode_ = 0;
    UINT oldOutputCP_ = 0, oldInputCP_ = 0;
    WORD oldAttributes_ = 7;
    CONSOLE_CURSOR_INFO oldCursor_{};
    CONSOLE_FONT_INFOEX font_{};
    bool valid_ = false, saved_ = false, fits_ = false, fontReady_ = false;
    unsigned int repeat_ = 0;
    wchar_t highSurrogate_ = 0;
    InputEvent repeated_{Key::Text, {}};
    map<wstring, int> widths_;
};
#else
class UnsupportedSurface final : public ConsoleSurface {
public:
    bool prepare() override { return false; }
    void clear(Rect) override {}
    void write(SHORT, SHORT, const wstring&, Color) override {}
    void cursor(SHORT, SHORT, bool) override {}
    int measure(const wstring& glyph) override { return portableColumns(glyph); }
    InputEvent readEvent() override { return {Key::EndOfInput, {}}; }
    void restore() override {}
};
#endif
}

ConsoleRenderer::ConsoleRenderer()
#ifdef _WIN32
    : ConsoleRenderer(make_unique<WindowsSurface>()) {}
#else
    : ConsoleRenderer(make_unique<UnsupportedSurface>()) {}
#endif
ConsoleRenderer::ConsoleRenderer(unique_ptr<ConsoleSurface> surface) : surface_(move(surface)) {
    if (!surface_) throw invalid_argument("Console surface is required");
}
ConsoleRenderer::~ConsoleRenderer() { restore(); }
bool ConsoleRenderer::beginFrame(bool clearScreen) {
    const bool wasReady = frameReady_;
    frameReady_ = surface_->prepare();
    frameCleared_ = false;
    if (frameReady_) {
        const Rect bounds = surface_->viewport();
        frameCleared_ = clearScreen || !wasReady || bounds.right != width_ || bounds.bottom + 1 != height_;
        width_ = bounds.right;
        height_ = bounds.bottom + 1;
        surface_->cursor(1, 1, false);
        if (frameCleared_) surface_->clear({0, 0, width_, static_cast<SHORT>(height_ - 1)});
    }
    return frameReady_;
}
void ConsoleRenderer::showCursor(SHORT x, SHORT y, bool visible) {
    if (frameReady_ && x >= 0 && x <= width_ && y >= 0 && y < height_) surface_->cursor(x, y, visible);
}
int ConsoleRenderer::columns(const wstring& text) {
    return displayColumns(text, [this](const wstring& g) { return surface_->measure(g); });
}
wstring ConsoleRenderer::clip(const wstring& text, int count) {
    return clipText(text, count, [this](const wstring& g) { return surface_->measure(g); });
}
vector<wstring> ConsoleRenderer::wrapText(const wstring& text, int count) {
    return UI::wrapText(text, count, [this](const wstring& g) { return surface_->measure(g); });
}
void ConsoleRenderer::drawTextIn(Rect area, SHORT x, SHORT y, const wstring& text, Color color) {
    // 调用方传进来的裁剪区再大，也要按全局边框压回去。
    if (y <= 0 || y >= static_cast<SHORT>(height_ - 1) || y == inputTop() || x <= 0 || x >= width_) return;
    area.left = max<SHORT>(area.left, 1);
    area.right = min<SHORT>(area.right, static_cast<SHORT>(width_ - 1));
    if (y < inputTop()) {
        if (x == DIVIDER_X || (x > DIVIDER_X && (y == 8 || y == 20))) return;
        if (x < DIVIDER_X) area.right = min<SHORT>(area.right, DIVIDER_X - 1);
        else area.left = max<SHORT>(area.left, DIVIDER_X + 1);
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
void ConsoleRenderer::drawOverlayTextIn(Rect area, SHORT x, SHORT y, const wstring& text, Color color) {
    if (y <= 0 || y >= static_cast<SHORT>(height_ - 1) || x <= 0 || x >= width_) return;
    area.left = max<SHORT>(area.left, 1);
    area.right = min<SHORT>(area.right, static_cast<SHORT>(width_ - 1));
    area.top = max<SHORT>(area.top, 1);
    area.bottom = min<SHORT>(area.bottom, static_cast<SHORT>(height_ - 2));
    if (!frameReady_ || x < area.left || y < area.top || y > area.bottom || x > area.right) return;
    for (const auto& glyph : splitGlyphs(text)) {
        if (glyph == L"\n") break;
        const int width = surface_->measure(glyph);
        if (width <= 0 || x + width - 1 > area.right) break;
        surface_->write(x, y, glyph, color);
        x = static_cast<SHORT>(x + width);
    }
}
void ConsoleRenderer::drawOverlayText(SHORT x, SHORT y, wstring text, Color color) {
    drawOverlayTextIn({1, 1, static_cast<SHORT>(width_ - 1), static_cast<SHORT>(height_ - 2)}, x, y, text, color);
}
void ConsoleRenderer::drawText(SHORT x, SHORT y, wstring text, Color color) {
    if (y <= 0 || y >= static_cast<SHORT>(height_ - 1) || y == inputTop() || x <= 0 || x >= width_) return;
    Rect area{1, 1, static_cast<SHORT>(width_ - 1), static_cast<SHORT>(height_ - 2)};
    if (y < inputTop()) {
        if (x == DIVIDER_X || (x > DIVIDER_X && (y == 8 || y == 20))) return;
        if (x < DIVIDER_X) area.right = DIVIDER_X - 1;
        else area.left = DIVIDER_X + 1;
    }
    drawTextIn(area, x, y, text, color);
}
void ConsoleRenderer::drawHorizontalLine(SHORT y, SHORT from, SHORT to) {
    if (to < 0) to = width_;
    if (!frameReady_ || y < 0 || y >= height_) return;
    for (SHORT x = max<SHORT>(0, from); x <= min<SHORT>(width_, to); ++x)
        surface_->write(x, y, L"-", Color::Title);
}
void ConsoleRenderer::drawVerticalLine(SHORT x, SHORT from, SHORT to) {
    if (to < 0) to = inputTop();
    if (!frameReady_ || x < 0 || x > width_) return;
    for (SHORT y = max<SHORT>(0, from); y <= min<SHORT>(static_cast<SHORT>(height_ - 1), to); ++y)
        surface_->write(x, y, L"|", Color::Title);
}
void ConsoleRenderer::drawFrame() {
    if (!frameReady_) return;
    drawHorizontalLine(0); drawHorizontalLine(inputTop()); drawHorizontalLine(static_cast<SHORT>(height_ - 1));
    drawVerticalLine(0, 0, static_cast<SHORT>(height_ - 1)); drawVerticalLine(width_, 0, static_cast<SHORT>(height_ - 1));
    drawVerticalLine();
    drawHorizontalLine(8, DIVIDER_X, width_); drawHorizontalLine(20, DIVIDER_X, width_);
    for (SHORT y : {SHORT(0), inputTop(), static_cast<SHORT>(height_ - 1)}) {
        surface_->write(0, y, L"+", Color::Title); surface_->write(width_, y, L"+", Color::Title);
    }
    for (SHORT y : {SHORT(0), SHORT(8), SHORT(20), inputTop()}) {
        surface_->write(DIVIDER_X, y, L"+", Color::Title);
        if (y == 8 || y == 20) surface_->write(width_, y, L"+", Color::Title);
    }
}
void ConsoleRenderer::clearInput() {
    if (frameReady_) surface_->clear({1, inputRow(), static_cast<SHORT>(width_ - 1), inputRow()});
}
InputEvent ConsoleRenderer::readEvent() { return surface_->readEvent(); }
void ConsoleRenderer::restore() { surface_->restore(); frameReady_ = false; }
}

#pragma once

#include "UI/TextLayout.h"

#include <cstdint>
#include <memory>

namespace UI {
using SHORT = std::int16_t;
constexpr SHORT UI_WIDTH = 110; // last x coordinate, NOT buffer column count
constexpr SHORT UI_HEIGHT = 30;
constexpr SHORT DIVIDER_X = 73;
constexpr SHORT INPUT_TOP = 25;
constexpr SHORT INPUT_ROW = 27;
constexpr SHORT REQUIRED_COLUMNS = UI_WIDTH + 1;

enum class Color : std::uint16_t {
    Normal = 7, Title = 11, Hint = 14, Success = 10, Error = 12
};
struct Rect { SHORT left; SHORT top; SHORT right; SHORT bottom; };
enum class Key { Text, Enter, Backspace, Delete, Left, Right, Home, End,
                 PageUp, PageDown, Resize, EndOfInput };
struct InputEvent { Key key; std::wstring text; };

// Small device boundary: native Win32 console in production, recording device in tests.
class ConsoleSurface {
public:
    virtual ~ConsoleSurface() = default;
    virtual bool prepare() = 0;
    virtual void clear(Rect area) = 0;
    virtual void write(SHORT x, SHORT y, const std::wstring& text, Color color) = 0;
    virtual void cursor(SHORT x, SHORT y, bool visible) = 0;
    virtual int measure(const std::wstring& glyph) = 0;
    virtual InputEvent readEvent() = 0;
    virtual void restore() = 0;
};

class ConsoleRenderer {
public:
    ConsoleRenderer();
    explicit ConsoleRenderer(std::unique_ptr<ConsoleSurface> surface);
    ~ConsoleRenderer();
    bool beginFrame();
    void moveCursor(SHORT x, SHORT y);
    void showCursor(SHORT x, SHORT y, bool visible);
    void drawText(SHORT x, SHORT y, std::wstring text, Color color = Color::Normal);
    void drawTextIn(Rect area, SHORT x, SHORT y, const std::wstring& text, Color color);
    void drawHorizontalLine(SHORT y = 0, SHORT from = 0, SHORT to = UI_WIDTH);
    void drawVerticalLine(SHORT x = DIVIDER_X, SHORT from = 0, SHORT to = INPUT_TOP);
    void drawFrame();
    void clearInput();
    std::vector<std::wstring> wrapText(const std::wstring& text, int columns);
    int columns(const std::wstring& text);
    std::wstring clip(const std::wstring& text, int columns);
    InputEvent readEvent();
    void restore();
private:
    std::unique_ptr<ConsoleSurface> surface_;
    bool frameReady_ = false;
};
}

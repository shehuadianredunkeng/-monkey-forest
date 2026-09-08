#include "UI/TextLayout.h"

#include <algorithm>
#include <climits>
#include <cstdint>

namespace UI {
namespace {
using Code = std::uint32_t;
std::wstring wide(Code code) {
    if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) code = 0xfffd;
#if WCHAR_MAX <= 0xffff
    if (code > 0xffff) {
        code -= 0x10000;
        return {static_cast<wchar_t>(0xd800 + (code >> 10)),
                static_cast<wchar_t>(0xdc00 + (code & 0x3ff))};
    }
#endif
    return {static_cast<wchar_t>(code)};
}
Code nextCode(const std::wstring& text, std::size_t& index) {
    Code code = static_cast<Code>(text[index++]);
    if (code >= 0xd800 && code <= 0xdbff && index < text.size()) {
        const Code low = static_cast<Code>(text[index]);
        if (low >= 0xdc00 && low <= 0xdfff) {
            ++index;
            return 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
        }
    }
    return (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) ? 0xfffd : code;
}
bool mark(Code code) {
    return (code >= 0x0300 && code <= 0x036f) || (code >= 0x1ab0 && code <= 0x1aff) ||
           (code >= 0x1dc0 && code <= 0x1dff) || (code >= 0x20d0 && code <= 0x20ff) ||
           (code >= 0xfe00 && code <= 0xfe0f) || (code >= 0xfe20 && code <= 0xfe2f) ||
           (code >= 0xe0100 && code <= 0xe01ef) || (code >= 0x1f3fb && code <= 0x1f3ff);
}
}

std::vector<std::wstring> splitGlyphs(const std::wstring& text) {
    std::vector<std::wstring> result;
    bool joinNext = false;
    for (std::size_t i = 0; i < text.size();) {
        Code code = nextCode(text, i);
        if (code == L'\r') continue;
        if (code == L'\n') { result.push_back(L"\n"); joinNext = false; continue; }
        if (code == L'\t') code = L' '; // no cursor-control escape sequences
        if (code < 0x20 || (code >= 0x7f && code <= 0x9f) ||
            (code >= 0x202a && code <= 0x202e) || (code >= 0x2066 && code <= 0x2069)) code = 0xfffd;
        const bool attach = mark(code) || code == 0x200d || joinNext;
        if (attach && !result.empty() && result.back() != L"\n") result.back() += wide(code);
        else result.push_back((mark(code) || code == 0x200d ? L"\u25cc" : L"") + wide(code));
        joinNext = code == 0x200d;
    }
    return result;
}

int portableColumns(const std::wstring& glyph) {
    // Deterministic fallback for headless tests. Windows uses measured cursor advance.
    int cells = 0;
    for (std::size_t i = 0; i < glyph.size();) {
        const Code c = nextCode(glyph, i);
        if (mark(c) || c == 0x200d || c == L'\n') continue;
        const bool wideChar = (c >= 0x1100 && c <= 0x115f) || c == 0x2329 || c == 0x232a ||
            (c >= 0x2e80 && c <= 0xa4cf && c != 0x303f) || (c >= 0xac00 && c <= 0xd7a3) ||
            (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe10 && c <= 0xfe6f) ||
            (c >= 0xff01 && c <= 0xff60) || (c >= 0xffe0 && c <= 0xffe6) ||
            (c >= 0x1f300 && c <= 0x1faff) || (c >= 0x20000 && c <= 0x3fffd);
        cells = std::max(cells, wideChar ? 2 : 1);
    }
    return cells;
}

int displayColumns(const std::wstring& text, const Measure& measure) {
    int cells = 0;
    for (const auto& glyph : splitGlyphs(text)) {
        if (glyph == L"\n") break;
        cells += std::max(0, measure(glyph));
    }
    return cells;
}

std::wstring clipText(const std::wstring& text, int columns, const Measure& measure) {
    std::wstring shown;
    if (columns <= 0) return shown;
    int cells = 0;
    for (const auto& glyph : splitGlyphs(text)) {
        if (glyph == L"\n") break;
        const int width = std::max(0, measure(glyph));
        if (cells + width > columns) break;
        shown += glyph;
        cells += width;
    }
    return shown;
}

std::vector<std::wstring> wrapText(const std::wstring& text, int columns, const Measure& measure) {
    std::vector<std::wstring> lines;
    if (columns <= 0) return lines;
    std::wstring line;
    int cells = 0;
    for (const auto& glyph : splitGlyphs(text)) {
        if (glyph == L"\n") { lines.push_back(line); line.clear(); cells = 0; continue; }
        int width = std::max(0, measure(glyph));
        const std::wstring shown = width > columns ? L"?" : glyph;
        if (width > columns) width = std::max(1, measure(shown));
        if (cells + width > columns && !line.empty()) {
            lines.push_back(line); line.clear(); cells = 0;
        }
        if (width <= columns) { line += shown; cells += width; }
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

std::wstring fromUtf8(const std::string& text) {
    std::wstring result;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) { result += wide(lead); continue; }
        int count = lead >= 0xc2 && lead <= 0xdf ? 1 : lead >= 0xe0 && lead <= 0xef ? 2 :
                    lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
        if (!count) { result += wide(0xfffd); continue; }
        Code code = lead & ((1u << (6 - count)) - 1);
        const auto start = i;
        for (int n = 0; n < count && i < text.size(); ++n) {
            const auto byte = static_cast<unsigned char>(text[i]);
            if ((byte & 0xc0) != 0x80) break;
            code = (code << 6) | (byte & 0x3f); ++i;
        }
        const Code minimum[] = {0, 0x80, 0x800, 0x10000};
        if (i - start != static_cast<std::size_t>(count) || code < minimum[count]) code = 0xfffd;
        result += wide(code);
    }
    return result;
}

std::string toUtf8(const std::wstring& text) {
    std::string result;
    for (std::size_t i = 0; i < text.size();) {
        const Code c = nextCode(text, i);
        if (c < 0x80) result.push_back(static_cast<char>(c));
        else {
            const int count = c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
            const unsigned int prefix[] = {0, 0, 0xc0, 0xe0, 0xf0};
            result.push_back(static_cast<char>(prefix[count] | (c >> (6 * (count - 1)))));
            for (int n = count - 2; n >= 0; --n) result.push_back(static_cast<char>(0x80 | ((c >> (6 * n)) & 0x3f)));
        }
    }
    return result;
}
}

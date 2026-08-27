#include "Event.h"

#include <sstream>

std::string Event::pendingFlag() const {
    return "flag_pending_" + eventId;
}

std::string Event::formatPrompt() const {
    std::ostringstream output;
    output << "【事件】" << title << '\n';
    output << description << '\n';
    for (std::size_t i = 0; i < choices.size(); ++i) {
        output << (i + 1) << ". " << choices[i] << '\n';
    }
    output << "请输入 choose 选项编号。";
    return output.str();
}

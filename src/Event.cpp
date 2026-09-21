#include "Event.h"

#include <sstream>

using namespace std;

string Event::pendingFlag() const {
    return "flag_pending_" + eventId;
}

string Event::formatPrompt() const {
    ostringstream output;
    output << "【事件】" << title << '\n';
    output << description << '\n';
    for (size_t i = 0; i < choices.size(); ++i) {
        output << (i + 1) << ". " << choices[i] << '\n';
    }
    output << "直接输入选项编号。";
    return output.str();
}

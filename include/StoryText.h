#pragma once

#include <string>
#include <vector>

using namespace std;

// 结局的名称、条件、评价和正文只在这里写一份，收集页和结局页都从这里取。
struct EndingInfo {
    string id;
    string name;
    string heading;
    string conditionTag;
    string condition;
    string evaluation;
    bool hidden = false;
    string body;
};

const vector<EndingInfo>& allEndingInfos();
const EndingInfo* findEndingInfo(const string& endingId);

string composeEndingText(const EndingInfo& info, const string& body);

string getStageIntroductionText(int stage);
string getEndingTextById(const string& endingId);

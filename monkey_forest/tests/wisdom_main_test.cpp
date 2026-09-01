// Exercise the real command loop, not a parallel parser or mocked story.
#define main monkeyForestMainForWisdomTest
#include "../src/main.cpp"
#undef main

#include <array>
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string play(const std::string& commands) {
    std::istringstream input(commands);
    std::ostringstream output;
    auto* oldInput = std::cin.rdbuf(input.rdbuf());
    auto* oldOutput = std::cout.rdbuf(output.rdbuf());
    std::cin.clear();
    int result = 0;
    try {
        result = monkeyForestMainForWisdomTest();
    } catch (...) {
        std::cin.rdbuf(oldInput);
        std::cout.rdbuf(oldOutput);
        std::cin.clear();
        throw;
    }
    std::cin.rdbuf(oldInput);
    std::cout.rdbuf(oldOutput);
    std::cin.clear();
    require(result == 0, "actual main must exit normally");
    return output.str();
}

std::string route(const std::array<bool, 5>& selected) {
    std::ostringstream commands;
    // Scout's existing rope quest supplies help/reputation for non-wisdom routes.
    commands << "go east\ntake item_rope\nfinish npc_scout\ninvestigate\nchoose "
             << (selected[0] ? 3 : 2)
             << "\ngo west\ninvestigate\nchoose 1\ngo east\ngo east\nrest\n"
             << "investigate\nchoose " << (selected[1] ? 2 : 3)
             << "\ngo west\ngo south\nrest\ninvestigate\nchoose "
             << (selected[2] ? 1 : 3)
             << "\ngo north\ngo east\nrest\ninvestigate\nchoose "
             << (selected[3] ? 2 : 1)
             << "\ngo west\ngo west\ninvestigate\nchoose 1\n"
             << "go east\ngo east\ngo east\nrest\ninvestigate\nchoose 3\n";
    if (selected[4]) commands << "investigate 日志\n";
    commands << "status\ngo west\ngo west\ngo west\ninvestigate\nchoose 2\nquit\n";
    return commands.str();
}

void testEveryThreeNodePlaythrough() {
    int total = 0;
    for (int first = 0; first < 3; ++first) {
        for (int second = first + 1; second < 4; ++second) {
            for (int third = second + 1; third < 5; ++third) {
                std::array<bool, 5> selected{};
                selected[first] = selected[second] = selected[third] = true;
                const std::string output = play(route(selected));
                require(output.find("【结局：无声胜利】") != std::string::npos,
                        "real three-node route failed: " + std::to_string(first) +
                        "," + std::to_string(second) + "," + std::to_string(third));
                require(output.find("智慧：4") != std::string::npos,
                        "three selected nodes must yield wisdom 4");
                ++total;
            }
        }
    }
    require(total == 10, "all ten real three-node routes must be exercised");
}

void testSkipTreeAndRiver() {
    const auto output = play(route({false, false, true, true, true}));
    require(output.find("【结局：无声胜利】") != std::string::npos,
            "skipping tree AND river must still reach hack ending");
    require(output.find("智慧：4") != std::string::npos,
            "cave + chip + base must supply three wisdom points");
}

void testAllNodesCapAndChip() {
    std::string commands = route({true, true, true, true, true});
    const auto status = commands.find("status\n");
    commands.insert(status, "inventory\ninvestigate 日志\n");
    const auto output = play(commands);
    require(output.find("智慧：5") != std::string::npos,
            "all rewards cap wisdom at 5");
    require(output.find("【结局：无声胜利】") != std::string::npos,
            "all-exploration route must finish");
    require(output.find("芯片 x1") != std::string::npos,
            "real inventory must still contain the chip after research");
    require(output.find("已经") != std::string::npos,
            "repeated log investigation must explain prior understanding");
    require(output.find("flag_wisdom_") == std::string::npos,
            "normal status output must not expose new wisdom reward IDs");
}

class SaveDirectory {
public:
    SaveDirectory() : previous(std::filesystem::current_path()),
        path(std::filesystem::temp_directory_path() /
            ("wisdom main " + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()))) {
        require(std::filesystem::create_directory(path), "unique save directory");
        std::filesystem::current_path(path);
    }
    ~SaveDirectory() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        std::filesystem::remove(path / "progress.txt", error);
        std::filesystem::remove(path, error);
    }
    std::filesystem::path previous;
    std::filesystem::path path;
};

void testSaveExitLoadThenRepeatRiver() {
    SaveDirectory directory;
    const std::string setup =
        "go east\ninvestigate\nchoose 2\ngo west\ninvestigate\nchoose 1\n"
        "go east\ngo east\nrest\ninvestigate\nchoose 2\nsave progress.txt\nquit\n";
    const std::string first = play(setup);
    require(std::filesystem::is_regular_file(directory.path / "progress.txt"),
            "save command must write the exact isolated file, even with spaces in cwd");
    const std::string second = play("load progress.txt\ninvestigate 管线\n"
                                    "investigate 银色管线\nstatus\nquit\n");
    require(first.find("存档成功") != std::string::npos &&
            second.find("读档成功") != std::string::npos,
            "real save/quit/new-main/load commands must work");
    require(second.find("智慧：2") != std::string::npos,
            "loaded river reward must not be granted again");
    require(second.find("已经") != std::string::npos &&
            second.find("【智慧 +1】") == std::string::npos,
            "loaded repeats must only explain prior research");
}

void testNoWisdomTraining() {
    const auto output = play("train wisdom\ntrain 智慧\nstatus\nquit\n");
    require(output.find("无法识别这个技能") != std::string::npos,
            "wisdom is not a trainable skill");
    require(output.find("智慧：1") != std::string::npos,
            "training attempts must not change wisdom");
}

void testLateResearchAfterFinalRejection() {
    std::string commands = route({false, false, false, false, false});
    commands.erase(commands.rfind("quit\n"));
    commands += "go east\ngo east\ninvestigate 管线\n"
                "go west\ngo south\ninvestigate 碑文\ninvestigate 晶片\n"
                "status\ngo north\ngo west\nchoose 2\nquit\n";
    const auto output = play(commands);
    require(output.find("智取路线需要智慧至少4点且拥有完整日志") != std::string::npos,
            "fixture must actually reach a rejected final choice");
    require(output.find("智慧：4") != std::string::npos &&
            output.find("【结局：无声胜利】") != std::string::npos,
            "late studies must rescue the pending final choice in actual main");
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;
    auto run = [&](const std::string& name, auto test) {
        try { test(); ++passed; }
        catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    run("all ten real routes", testEveryThreeNodePlaythrough);
    run("skip tree and river", testSkipTreeAndRiver);
    run("all nodes cap and chip", testAllNodesCapAndChip);
    run("real save/exit/load/repeat", testSaveExitLoadThenRepeatRiver);
    run("no wisdom training", testNoWisdomTraining);
    run("late research after final rejection", testLateResearchAfterFinalRejection);
    std::cout << "wisdom_main_test: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

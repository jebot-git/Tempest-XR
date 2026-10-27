#include "records.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::to_string(__LINE__) + ": " #c);                          \
    } while (false)
static void endRun(Game &g, int level, int score, const std::string &name) {
    g.accessKey = -1;
    g.start(level);
    g.score = score;
    g.mode = Mode::GameOver;
    g.frontPage = FrontPage::GameOver;
    g.tick(.2f);
    g.activate();
    CHECK(g.enteringScore);
    for (char ch : name)
        g.setScoreLetter(ch);
    if (g.enteringBrag)
        for (char ch : std::string("BEAST"))
            g.setScoreLetter(ch);
    CHECK(!g.enteringScore && g.frontPage == FrontPage::Scores);
}
int main() {
    auto dir = std::filesystem::temp_directory_path() /
               ("tempest-keys-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    auto path = (dir / "records.json").string();
    try {
        Game g;
        CHECK(g.highScores[0].score == 500017);
        CHECK(std::all_of(g.accessKeys.begin(), g.accessKeys.end(),
                          [](const auto &k) { return !k.unlocked; }));
        endRun(g, 18, 100, "YAK");
        CHECK(g.accessKeys[0].initials == "YAK" && g.accessKeys[0].unlocked == 17);
        endRun(g, 20, 200, "YAK");
        CHECK(g.accessKeys[0].unlocked == 19 && !g.accessKeys[1].unlocked);
        endRun(g, 22, 300, "EWE");
        endRun(g, 24, 400, "COW");
        endRun(g, 26, 500, "GNU");
        CHECK(g.accessKeys[3].unlocked == 25);
        endRun(g, 28, 600, "OX ");
        CHECK(g.accessKeys[0].initials == "OX " && g.accessKeys[0].unlocked == 27);
        g.returnToTitle();
        g.selectAccessKey();
        g.steer(2);
        g.activate();
        CHECK(g.accessKey == 2 && g.level == 23 && g.score > 0);
        g.mode = Mode::GameOver;
        g.frontPage = FrontPage::GameOver;
        g.tick(.2f);
        g.activate();
        CHECK(!g.enteringScore);
        CHECK(g.accessKeys[2].initials == "COW");
        endRun(g, 99, 600000, "XR!");
        CHECK(g.highScores[0].initials == "XR!");
        CHECK(std::any_of(g.accessKeys.begin(), g.accessKeys.end(),
                          [](const auto &k) { return k.initials == "XR!" && k.unlocked == 97; }));
        g.beastlyUnlocked = true;
        CHECK(saveRecords(path, g));
        auto data = nlohmann::json::parse(std::ifstream(path));
        CHECK(data["version"] == 2 && data["key_bytes"].size() == 16);
        Game restored;
        CHECK(loadRecords(path, restored));
        CHECK(restored.accessKey == -1 && restored.selectedLevel == 1);
        CHECK(restored.highScores[0].initials == "XR!" && restored.beastlyUnlocked);
        for (size_t i = 0; i < 4; ++i)
            CHECK(restored.accessKeys[i].unlocked == g.accessKeys[i].unlocked);
        std::ofstream(path) << "{broken";
        CHECK(!loadRecords(path, restored));
        CHECK(restored.highScores[0].score == 600000);
        data["version"] = 1;
        data.erase("key_bytes");
        data["keys"] = nlohmann::json::array();
        for (int level : {35, 1, 17, 99})
            data["keys"].push_back({{"unlocked", level}, {"beastly", false}});
        std::ofstream(path) << data;
        Game migrated;
        CHECK(loadRecords(path, migrated));
        CHECK(migrated.accessKeys[0].initials == "YAK" && migrated.accessKeys[0].unlocked == 33);
        CHECK(!migrated.accessKeys[1].unlocked && !migrated.accessKeys[2].unlocked &&
              migrated.accessKeys[3].unlocked == 97);
        CHECK(!saveRecords((dir / "absent" / "records.json").string(), g));
        std::filesystem::remove_all(dir);
        std::cout << "Earned keys, odd checkpoints, signature reuse, four-slot replacement, raw "
                     "key bytes and legacy migration passed\n";
    } catch (const std::exception &e) {
        std::filesystem::remove_all(dir);
        std::cerr << e.what() << '\n';
        return 1;
    }
}

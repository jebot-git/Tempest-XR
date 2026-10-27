#include "records.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::to_string(__LINE__) + ": " #c);                          \
    } while (false)
int main() {
    auto directory = std::filesystem::temp_directory_path() /
                     ("tempest-records-test-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    auto path = (directory / "records.json").string();
    try {
        Game g;
        CHECK(g.highScores[0].score == 500017 && g.highScores[9].initials == "FUR");
        g.start(99);
        g.score = 600000;
        g.enemiesRemaining = 0;
        for (int i = 0; i < 300; ++i)
            g.tick(1.f / 120);
        CHECK(g.mode == Mode::Victory && g.enteringScore && g.beastlyUnlocked);
        g.steer(.4f);
        CHECK(g.scoreInitials == "AAA");
        g.steer(.6f);
        CHECK(g.scoreInitials == "BAA");
        g.steer(1);
        CHECK(g.scoreInitials == "CAA");
        g.setScoreLetter('X');
        g.setScoreLetter('R');
        g.setScoreLetter('2');
        g.finishScoreEntry();
        CHECK(g.highScores[0].initials == "XR2" && g.highScores[0].score == 600000);
        CHECK(g.highScores[1].score == 500017 && !g.enteringScore);
        g.mode = Mode::Menu;
        g.selectAccessKey();
        CHECK(g.accessKey == 1 && g.unlockedLevel == 1 && !g.beastlyUnlocked);
        g.unlockedLevel = 17;
        g.selectAccessKey();
        g.selectAccessKey();
        g.selectAccessKey();
        CHECK(g.accessKey == 0 && g.unlockedLevel == 99 && g.beastlyUnlocked);
        CHECK(saveRecords(path, g));
        Game restored;
        CHECK(loadRecords(path, restored));
        CHECK(restored.highScores[0].initials == "XR2" && restored.best == 600000);
        CHECK(restored.unlockedLevel == 99 && restored.beastlyUnlocked);
        CHECK(restored.accessKeys[1].unlocked == 17);
        std::ofstream(path) << "{broken";
        CHECK(!loadRecords(path, restored));
        CHECK(restored.highScores[0].score == 600000 && restored.unlockedLevel == 99);
        CHECK(!saveRecords((directory / "absent" / "records.json").string(), g));
        CHECK(!std::filesystem::exists(path + ".tmp"));
        std::filesystem::remove_all(directory);
        std::cout << "Source default scores, initials entry, Beastly unlock, independent access "
                     "keys and atomic persistence passed\n";
    } catch (const std::exception &e) {
        std::filesystem::remove_all(directory);
        std::cerr << e.what() << '\n';
        return 1;
    }
}

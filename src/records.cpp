#include "records.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
namespace tempest {
using nlohmann::json;
bool loadRecords(const std::string &path, Game &g) {
    std::ifstream file(path);
    if (!file)
        return false;
    try {
        auto data = json::parse(file);
        if ((data.at("version") != 1 && data.at("version") != 2) ||
            data.at("scores").size() != g.highScores.size())
            return false;
        auto scores = g.highScores;
        std::array<AccessKey, 4> keys{};
        for (size_t i = 0; i < scores.size(); ++i) {
            const auto &entry = data.at("scores").at(i);
            scores[i] = {entry.at("initials").get<std::string>(), entry.at("score").get<int>(),
                         entry.value("level", 1), entry.value("beastly", false)};
            if (scores[i].initials.size() != 3 || scores[i].score < 0 || scores[i].level < 1 ||
                scores[i].level > 100 ||
                scores[i].initials.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .!-") !=
                    std::string::npos ||
                (i && scores[i].score > scores[i - 1].score))
                return false;
        }
        bool unlockedBeastly = data.value("beastly_unlocked", false);
        if (data.at("version") == 1) {
            if (data.at("keys").size() != keys.size())
                return false;
            for (size_t i = 0; i < keys.size(); ++i) {
                int old = data.at("keys").at(i).at("unlocked").get<int>();
                if (old < 1 || old > 100)
                    return false;
                bool hard = data.at("keys").at(i).value("beastly", false);
                unlockedBeastly = unlockedBeastly || hard;
                if (old > 17)
                    keys[i] = {Game::checkpoint(old), false, Game::keyNames[i]};
            }
        } else {
            const auto &bytes = data.at("key_bytes");
            if (bytes.size() != 16)
                return false;
            for (size_t i = 0; i < keys.size(); ++i) {
                std::string name;
                for (int ch = 0; ch < 3; ++ch) {
                    int value = bytes.at(i * 4 + size_t(ch)).get<int>();
                    if (value < 32 || value > 126)
                        return false;
                    name.push_back(char(value >= 'a' && value <= 'z' ? value - 32 : value));
                }
                int checkpoint = bytes.at(i * 4 + 3).get<int>();
                if (checkpoint < 0 || checkpoint > 98 || (checkpoint & 1))
                    return false;
                if (checkpoint)
                    keys[i] = {checkpoint + 1, false, name};
            }
        }
        auto brag = data.value("brag", std::string("BEAST"));
        if (brag.size() != 5 ||
            brag.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .!-") != std::string::npos)
            return false;
        g.brag = brag;
        g.highScores = scores;
        g.accessKeys = keys;
        g.accessKey = -1;
        g.selectedLevel = 1;
        g.beastlyUnlocked = unlockedBeastly;
        g.best = std::max(g.best, g.highScores.front().score);
        return true;
    } catch (const json::exception &) {
        return false;
    }
}
bool saveRecords(const std::string &path, Game &g) {
    json data{{"version", 2},
              {"brag", g.brag},
              {"beastly_unlocked", g.beastlyUnlocked},
              {"scores", json::array()},
              {"key_bytes", json::array()}};
    for (const auto &entry : g.highScores)
        data["scores"].push_back({{"initials", entry.initials},
                                  {"score", entry.score},
                                  {"level", entry.level},
                                  {"beastly", entry.beastly}});
    for (size_t i = 0; i < g.accessKeys.size(); ++i) {
        const auto &key = g.accessKeys[i];
        std::string initials = key.unlocked ? key.initials : Game::keyNames[i];
        for (char ch : initials)
            data["key_bytes"].push_back(int(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch));
        data["key_bytes"].push_back(key.unlocked ? key.unlocked - 1 : 0);
    }
    auto temporary = path + ".tmp";
    std::ofstream file(temporary);
    if (!file)
        return false;
    file << data.dump(2) << '\n';
    file.close();
    if (!file)
        return false;
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    return !error;
}
} // namespace tempest

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
        if (data.at("version") != 1 || data.at("scores").size() != g.highScores.size() ||
            data.at("keys").size() != g.accessKeys.size())
            return false;
        auto scores = g.highScores;
        auto keys = g.accessKeys;
        for (size_t i = 0; i < scores.size(); ++i) {
            const auto &entry = data.at("scores").at(i);
            scores[i] = {entry.at("initials").get<std::string>(), entry.at("score").get<int>(),
                         entry.value("level", 1), entry.value("beastly", false)};
            if (scores[i].initials.size() != 3 || scores[i].score < 0 || scores[i].level < 1 ||
                scores[i].level > 100 ||
                scores[i].initials.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ") !=
                    std::string::npos ||
                (i && scores[i].score > scores[i - 1].score))
                return false;
        }
        for (size_t i = 0; i < keys.size(); ++i) {
            keys[i].unlocked = data.at("keys").at(i).at("unlocked").get<int>();
            keys[i].beastly = data.at("keys").at(i).value("beastly", false);
            if (keys[i].unlocked < 1 || keys[i].unlocked > 100)
                return false;
        }
        int selected = data.value("key", 0);
        if (selected < 0 || selected >= int(keys.size()))
            return false;
        g.highScores = scores;
        g.accessKeys = keys;
        g.accessKey = selected;
        g.unlockedLevel = keys[size_t(selected)].unlocked;
        g.selectedLevel = g.unlockedLevel;
        g.beastlyUnlocked = keys[size_t(selected)].beastly;
        g.best = std::max(g.best, g.highScores.front().score);
        return true;
    } catch (const json::exception &) {
        return false;
    }
}
bool saveRecords(const std::string &path, Game &g) {
    g.storeAccessKey();
    json data{
        {"version", 1}, {"key", g.accessKey}, {"scores", json::array()}, {"keys", json::array()}};
    for (const auto &entry : g.highScores)
        data["scores"].push_back({{"initials", entry.initials},
                                  {"score", entry.score},
                                  {"level", entry.level},
                                  {"beastly", entry.beastly}});
    for (const auto &key : g.accessKeys)
        data["keys"].push_back({{"unlocked", key.unlocked}, {"beastly", key.beastly}});
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

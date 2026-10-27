#pragma once
#include "game.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace tempest {
struct Color {
    float r, g, b, a = 1;
};
struct Line {
    Vec3 a, b;
    Color color;
};
class Renderer {
    std::vector<Line> lines;
    std::unique_ptr<Game> nextWeb;
    int nextLevel = 0;
    float lastTrailTime = -1;
    std::vector<std::pair<Vec3, float>> shipTrail;
    struct MeltState {
        unsigned texture = 0;
        int width = 0, height = 0;
        Mode previous = Mode::Menu;
        bool seeded = false;
        float started = -100, lastTime = 0;
    };
    std::array<MeltState, 5> meltStates{}; // XR eyes, SBS preview eyes, desktop.
    void line(Vec3 a, Vec3 b, Color c);
    void flush();
    void enemy(const Game &g, const Enemy &e);
    void grid(const Game &g, Vec3 offset = {}, float alpha = 1);
    void particleMessage(const Game &g, bool headset);
    void bonusWorld(const Game &g);
    void headsetHud(const Game &g);
    void deathWorld(const Game &g);
    bool mesh(std::string_view name, Vec3 center, Vec3 tangent, Vec3 normal, float scale,
              Color tint, float angle = 0, float fold = 1);

  public:
    void world(const Game &g, bool headset = false);
    void desktop(const Game &g, int width, int height);
    void hud(const Game &g, int width, int height, const std::string &status, float sensitivity,
             bool inverted, bool muted);
    void finishFrame(const Game &g, int view, int x, int y, int width, int height);
    void releaseGL();
    static void text(float x, float y, const std::string &value, float scale, Color color);
    static void rect(float x, float y, float w, float h, Color color);
    static std::vector<std::string> xrHudLines(const Game &g);
    static bool originalArtAvailable();
};
} // namespace tempest

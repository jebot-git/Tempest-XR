#include "game.hpp"
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #c);                                                      \
    } while (false)
constexpr float dt = 1.f / 120;
static Game setup(int level = 1) {
    Game g(42);
    g.start(level);
    g.tick(1.f / 120);
    g.fireHeld = false;
    g.invulnerable = 100;
    return g;
}
static int count(const Game &g, Kind kind) {
    return int(std::count_if(g.enemies.begin(), g.enemies.end(),
                             [&](auto &e) { return !e.dead && e.kind == kind; }));
}
int main() {
    try {
        {
            auto g = setup();
            g.enemiesRemaining = 0;
            Enemy e{Kind::Flipper, g.boundedLane(g.lane + 3), 0};
            e.rim = true;
            e.motion = FlipperMotion::Stopped;
            e.stopTime = 1;
            g.enemies = {e};
            g.spikes[size_t(g.lane)] = .5f;
            g.tick(dt);
            CHECK(g.mode == Mode::Transition && count(g, Kind::Flipper) == 1);
            CHECK(g.time < .02f && g.spikes[size_t(g.lane)] == .5f);
        }
        for (auto kind : {Kind::Flipper, Kind::EnemyShot, Kind::Powerup}) {
            auto g = setup();
            g.enemiesRemaining = 0;
            g.spawn(kind, g.boundedLane(g.lane + 3), .2f);
            g.tick(dt);
            CHECK(g.mode == (kind == Kind::Powerup ? Mode::Transition : Mode::Playing));
        }
        {
            auto g = setup();
            Enemy e{Kind::Flipper, g.boundedLane(g.lane + 3), 0};
            e.rim = true;
            e.motion = FlipperMotion::Stopped;
            e.stopTime = 100;
            g.enemies = {e};
            // Generation has not finished: a rim enemy cannot expire to clear the wave.
            for (int i = 0; i < 120 * 25; ++i) {
                g.enemiesRemaining = 1000;
                g.tick(dt);
            }
            CHECK(std::any_of(g.enemies.begin(), g.enemies.end(),
                              [](auto &e) { return e.kind == Kind::Flipper && e.age > 24; }));
        }
        for (auto kind : {Kind::SuperFlipper, Kind::SuperFlipper2})
            for (int weapon = 0; weapon < 3; ++weapon) {
                auto g = setup();
                g.boost = weapon == 1;
                g.spawn(kind, g.lane, .5f);
                Shot shot{g.lane, .49f, .49f};
                shot.ally = weapon == 2;
                g.shots = {shot};
                g.tick(dt);
                CHECK(count(g, kind) == 0 && count(g, Kind::EnemyShot) == 0 && g.score == 150);
                CHECK(g.shots.size() == (weapon == 1 ? 1u : 0u));
            }
        for (float phase : {0.f, 1.8f}) {
            auto g = setup();
            g.invulnerable = 0;
            g.waveTime = phase;
            Enemy e{Kind::PulsarSpark, g.lane, 0};
            e.rim = true;
            g.enemies = {e};
            g.fireHeld = true;
            g.tick(dt);
            CHECK(g.lives == 2 && g.score == 0 && count(g, Kind::PulsarSpark) == 1);
        }
        for (int level : {1, 3})
            for (int side : {0, 1}) {
                auto g = setup(level);
                int at = side ? g.web.lanes() - 1 : 0;
                Enemy e{Kind::PulsarSpark, at, 0};
                e.rim = true;
                e.direction = side ? 1 : -1;
                e.sparkTime = dt / 2;
                g.enemies = {e};
                g.tick(dt);
                CHECK(count(g, Kind::PulsarSpark) == 1);
                int expected =
                    g.web.closed ? g.boundedLane(at + (side ? 1 : -1)) : at + (side ? -1 : 1);
                CHECK(g.enemies[0].lane == expected);
                CHECK(g.enemies[0].direction == (g.web.closed ? (side ? 1 : -1) : (side ? -1 : 1)));
            }
        {
            auto g = setup();
            g.invulnerable = 0;
            g.jumpEnabled = true;
            g.jump();
            Enemy e{Kind::PulsarSpark, g.lane, 0};
            e.rim = true;
            g.enemies = {e};
            g.fireHeld = true;
            g.tick(dt);
            for (int i = 0; i < 9; ++i)
                g.tick(dt);
            CHECK(g.lives == 3 && g.score == 150 && count(g, Kind::PulsarSpark) == 0);
        }
        {
            auto g = setup();
            int at = g.boundedLane(g.lane + 3);
            Enemy e{Kind::PulsarSpark, at, 0};
            e.rim = true;
            g.enemies = {e};
            for (int i = 0; i < 8; ++i)
                g.tick(dt);
            CHECK(g.enemies[0].lane == at);
            g.tick(dt);
            CHECK(g.enemies[0].lane == g.boundedLane(at + 1));
        }
        for (auto family : {Kind::Tanker, Kind::FuseTanker, Kind::PulsarTanker}) {
            auto g = setup();
            int at = g.boundedLane(g.lane + 3);
            g.spawn(family, at, .22f);
            g.tick(dt);
            CHECK(count(g, family) == 1 && g.score == 0);
            g.enemies[0].depth = .00001f;
            g.tick(1.f / 60);
            CHECK(count(g, family) == 0 && g.enemies.size() == 3 && g.score == 100);
            CHECK(std::find(g.sounds.begin(), g.sounds.end(), Sound::TankerOpen) != g.sounds.end());
            int contents = 0;
            for (const auto &e : g.enemies)
                if (e.kind != Kind::Powerup) {
                    ++contents;
                    CHECK(e.depth == 0);
                    if (family != Kind::FuseTanker)
                        CHECK(e.emerging && e.motion == FlipperMotion::Turning && e.turnFrom == at);
                }
            CHECK(contents == 2);
        }
        {
            auto g = setup();
            int at = g.boundedLane(g.lane + 3);
            g.spawn(Kind::Tanker, at, .6f);
            g.shots = {{at, .59f, .59f}};
            g.tick(dt);
            CHECK(count(g, Kind::Flipper) == 2);
            for (auto &e : g.enemies)
                if (e.kind == Kind::Flipper)
                    CHECK(e.depth > .59f && e.emerging);
            for (int i = 0; i < 80; ++i)
                g.tick(dt);
            CHECK(std::any_of(g.enemies.begin(), g.enemies.end(), [](auto &e) {
                return e.kind == Kind::Flipper && !e.emerging && e.depth < .59f;
            }));
        }
        std::cout
            << "Source wave clearance, retained enemies and single-hit super flippers passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

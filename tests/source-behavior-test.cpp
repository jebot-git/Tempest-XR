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
    g.autoFire = false;
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
        std::cout << "Source wave-clear condition and retained rim enemies passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

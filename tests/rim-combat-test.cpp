#include "game.hpp"
#include "webs.hpp"
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #c);                                                      \
    } while (false)
constexpr float dt = 1.f / 60;
static Game setup(int level = 1) {
    Game g(42);
    g.start(level);
    g.fireHeld = false;
    g.invulnerable = 0;
    g.sounds.clear();
    return g;
}
static Enemy rim(const Game &g, int offset = 0, Kind kind = Kind::Flipper) {
    Enemy e{kind, g.boundedLane(g.lane + offset), 0};
    e.rim = true;
    return e;
}
static int count(const Game &g, Kind kind) {
    return int(std::count_if(g.enemies.begin(), g.enemies.end(),
                             [&](auto &e) { return e.kind == kind && !e.dead; }));
}
static Game turning(int level = 1, Kind kind = Kind::Flipper) {
    auto g = setup(level);
    g.enemies = {rim(g, -1, kind)};
    g.tick(dt);
    CHECK(g.enemies[0].motion == FlipperMotion::Turning);
    CHECK(g.enemies[0].lane == g.lane && g.enemies[0].turnAngle == 0);
    return g;
}
int main() {
    try {
        // Shots are generated through the actual fire button path, not injected at contact.
        for (bool fire : {false, true}) {
            auto g = setup();
            g.enemies = {rim(g)};
            if (fire)
                g.shots = {{g.lane}};
            g.tick(dt);
            CHECK(g.lives == (fire ? 3 : 2));
            CHECK(g.score == (fire ? 150 : 0));
            CHECK(g.snatched == !fire);
        }
        {
            auto g = setup();
            g.enemies = {rim(g)};
            g.shots = {{g.lane, .1f, .1f}};
            g.tick(dt);
            CHECK(g.lives == 2 && g.score == 0);
        }
        {
            auto g = setup();
            g.enemies = {rim(g)};
            g.shots = {{g.lane, 6.f / 160, 6.f / 160}};
            g.tick(dt);
            CHECK(g.lives == 2 && g.score == 0); // colok requires <6, not <=6.
        }
        // A lane switch starts a real turn; it is not an immediate kill in the new lane.
        {
            auto g = turning();
            CHECK(g.enemies[0].turnArc == 112); // circle orientation 16 units between lanes.
            CHECK(!Game::shotVulnerable(g.enemies[0]));
            Vec3 before = g.enemyPosition(g.enemies[0]);
            auto old = g.position(float(g.enemies[0].turnFrom), 0);
            CHECK(std::hypot(before.x - old.x, before.y - old.y) < .0001f);
            g.fireHeld = true;
            g.tick(dt);
            g.fireHeld = false;
            CHECK(g.score == 0 && g.lives == 3 && count(g, Kind::Flipper) == 1);
            auto after = g.enemyPosition(g.enemies[0]);
            CHECK(std::hypot(after.x - before.x, after.y - before.y) < .13f);
            // That shot leaves the rim long before vulnerability opens.
            for (int i = 0; i < 100 && g.deathTime == 0; ++i)
                g.tick(dt);
            CHECK(g.lives == 2 && g.score == 0 && g.snatched);
        }
        for (int weapon = 0; weapon < 3; ++weapon) {
            auto g = turning();
            g.boost = weapon == 1;
            while (g.enemies[0].turnArc - g.enemies[0].turnAngle > 56)
                g.tick(dt);
            CHECK(!Game::shotVulnerable(g.enemies[0]));
            if (weapon == 2) {
                Shot shot{g.lane};
                shot.ally = true;
                g.shots = {shot};
            } else
                g.shots = {{g.lane}};
            // Remaining angle reaches exactly 52 during this tick. A live shot wins.
            g.tick(dt);
            CHECK(g.lives == 3 && g.deathTime == 0 && g.score == 150);
            CHECK(count(g, Kind::Flipper) == 0);
        }
        // stdstop/rail check an existing shot before initiating a new turn.
        {
            auto g = setup();
            auto e = rim(g, -1);
            e.motion = FlipperMotion::Stopped;
            g.enemies = {e};
            Shot shot{e.lane};
            shot.ally = true;
            g.shots = {shot};
            g.tick(dt);
            CHECK(g.score == 150 && count(g, Kind::Flipper) == 0 && g.lives == 3);
            CHECK(std::any_of(g.enemies.begin(), g.enemies.end(), [&](auto &p) {
                return p.kind == Kind::Powerup && p.lane == e.lane;
            }));
        }
        // The shot has already left before the window opens in the same long tick.
        {
            auto g = turning();
            while (g.enemies[0].turnArc - g.enemies[0].turnAngle > 64)
                g.tick(dt);
            g.shots = {{g.lane, .037f, .037f}};
            g.tick(.06f);
            CHECK(Game::shotVulnerable(g.enemies[0]));
            CHECK(count(g, Kind::Flipper) == 1 && g.score == 0);
        }
        // Wrong lane misses; turning into the vacated lane is dangerous only in
        // the source's invulnerable rotation phase.
        {
            auto g = turning();
            int old = g.enemies[0].turnFrom;
            g.steer(float(old - g.lane));
            g.fireHeld = true;
            g.tick(dt);
            CHECK(g.lives == 2 && g.snatched);
        }
        {
            auto g = turning();
            while (!Game::shotVulnerable(g.enemies[0]))
                g.tick(dt);
            int old = g.enemies[0].turnFrom;
            g.steer(float(old - g.lane));
            g.fireHeld = true;
            g.tick(dt);
            CHECK(g.lives == 3 && g.score == 0 && count(g, Kind::Flipper) == 1);
        }
        {
            auto g = turning();
            g.jumpEnabled = true;
            g.jump();
            int old = g.enemies[0].turnFrom;
            g.steer(float(old - g.lane));
            g.tick(dt);
            CHECK(g.lives == 3);
        }
        // Pause freezes the same geometry and vulnerability state that is drawn.
        {
            auto g = turning();
            for (int i = 0; i < 8; ++i)
                g.tick(dt);
            auto before = g.enemyPosition(g.enemies[0]);
            float angle = g.enemies[0].turnAngle;
            g.pause();
            g.tick(.5f);
            auto after = g.enemyPosition(g.enemies[0]);
            CHECK(angle == g.enemies[0].turnAngle && before.x == after.x && before.y == after.y);
            g.pause();
            g.tick(dt);
            CHECK(g.enemies[0].turnAngle > angle);
        }
        {
            auto g = turning(1, Kind::SuperFlipper);
            g.tick(dt);
            CHECK(g.enemies[0].turnAngle == 8); // mode 2 rotates at twice normal speed.
        }
        // A concave web can start inside the wrapped vulnerability window,
        // become invulnerable midway through a long turn, then open again.
        {
            auto g = setup(10);
            g.lane = 4;
            g.enemies = {rim(g, -1)};
            g.tick(dt);
            CHECK(g.enemies[0].turnArc == 224 && Game::shotVulnerable(g.enemies[0]));
            g.shots = {{g.lane}};
            g.tick(dt);
            CHECK(g.score == 150 && g.lives == 3);
        }
        {
            auto g = setup(10);
            g.lane = 4;
            g.enemies = {rim(g, -1)};
            g.tick(dt);
            while (g.enemies[0].turnAngle <= 20)
                g.tick(dt);
            CHECK(!Game::shotVulnerable(g.enemies[0]));
            g.fireHeld = true;
            g.tick(dt);
            g.fireHeld = false;
            CHECK(count(g, Kind::Flipper) == 1 && g.score == 0);
            while (!Game::shotVulnerable(g.enemies[0]))
                g.tick(dt);
            CHECK(g.score == 0);
            g.shots = {{g.lane}};
            g.tick(dt);
            CHECK(g.score == 150 && g.lives == 3);
        }
        // Basic flippers pause on landing, then pursue the new ship position.
        {
            auto g = setup();
            g.invulnerable = 100;
            g.lane = 2;
            Enemy e{Kind::Flipper, 0, 0};
            e.rim = true;
            g.enemies = {e};
            g.tick(dt);
            CHECK(g.enemies[0].lane == 1);
            while (g.enemies[0].motion == FlipperMotion::Turning)
                g.tick(dt);
            CHECK(std::find(g.sounds.begin(), g.sounds.end(), Sound::FlipperLand) !=
                  g.sounds.end());
            float pause = g.enemies[0].stopTime;
            CHECK(std::abs(pause - .15f) < .0001f);
            g.tick(dt);
            CHECK(g.enemies[0].motion == FlipperMotion::Stopped && g.enemies[0].lane == 1);
            while (g.enemies[0].motion == FlipperMotion::Stopped)
                g.tick(dt);
            CHECK(g.enemies[0].turnFrom == 1 && g.enemies[0].lane == 2);
        }
        {
            auto g = setup();
            g.lane = 8;
            Enemy e{Kind::Flipper, 0, 0};
            e.rim = true;
            g.enemies = {e};
            g.tick(dt);
            CHECK(g.enemies[0].lane == 15); // source half-web direction tie.
        }
        // The reproduction must have identical results for both vector orders,
        // for normal fire, friendly fire, and piercing laser fire.
        for (int weapon = 0; weapon < 3; ++weapon)
            for (bool reverse : {false, true}) {
                auto g = setup();
                g.boost = weapon == 1;
                Enemy behind{Kind::Flipper, g.lane, .04f};
                behind.serial = 10;
                auto rimEnemy = rim(g);
                rimEnemy.serial = 20;
                g.enemies = {behind, rimEnemy};
                if (reverse)
                    std::reverse(g.enemies.begin(), g.enemies.end());
                if (weapon == 2) {
                    Shot shot{g.lane};
                    shot.ally = true;
                    g.shots = {shot};
                } else
                    g.shots = {{g.lane}};
                g.shots[0].serial = 30;
                g.tick(dt);
                CHECK(g.lives == 3 && g.deathTime == 0);
                CHECK(g.score == (weapon == 1 ? 300 : 150));
                CHECK(count(g, Kind::Flipper) == (weapon == 1 ? 0 : 1));
            }
        // Resolve occlusion and opposing bullets in the same impact order.
        for (bool reverse : {false, true}) {
            auto g = setup();
            g.invulnerable = 100;
            g.enemies = {{Kind::Flipper, g.lane, .5f}, {Kind::EnemyShot, g.lane, .35f}};
            if (reverse)
                std::reverse(g.enemies.begin(), g.enemies.end());
            g.shots = {{g.lane, .3f, .3f}};
            g.tick(.1f);
            CHECK(count(g, Kind::EnemyShot) == 0 && count(g, Kind::Flipper) == 1 && g.score == 0);
        }
        {
            auto g = setup();
            g.enemies = {{Kind::Flipper, g.lane, .5f}};
            g.spikes[size_t(g.lane)] = .25f;
            g.shots = {{g.lane, .24f, .24f}};
            g.tick(.2f);
            CHECK(count(g, Kind::Flipper) == 1 && g.score == 3 && g.shots.empty());
        }
        // Reaching the rim cannot teleport a climbing flipper to another lane.
        {
            auto g = setup();
            g.enemies = {{Kind::Flipper, g.boundedLane(g.lane - 1), .1f}};
            int old = g.enemies[0].lane;
            while (!g.enemies[0].rim) {
                g.tick(dt);
                if (!g.enemies[0].rim)
                    CHECK(g.enemies[0].lane == old);
            }
            auto p = g.enemyPosition(g.enemies[0]), start = g.position(float(old), 0);
            CHECK(std::hypot(p.x - start.x, p.y - start.y) < .0001f);
        }
        // All original orientation tables and all campaign webs, including wrap,
        // corners and open ends: the visual path has continuous endpoints.
        for (const auto &raw : rawWebs)
            CHECK(raw.orientations.size() == raw.xy.size() / 2 - (raw.closed ? 0 : 1));
        for (int level = 1; level <= 100; ++level)
            for (int direction : {-1, 1}) {
                auto g = setup(level);
                g.invulnerable = 100;
                int from = direction > 0 ? g.web.lanes() - 1 : 0;
                int target = g.web.closed ? g.boundedLane(from + direction) : from - direction;
                g.lane = target;
                Enemy e{Kind::Flipper, from, 0};
                e.rim = true;
                g.enemies = {e};
                g.tick(dt);
                CHECK(g.enemies[0].lane == target);
                CHECK(g.web.orientations.size() == size_t(g.web.lanes()));
                int ticks = 0;
                auto previous = g.enemyPosition(g.enemies[0]);
                while (g.enemies[0].motion == FlipperMotion::Turning && ticks++ < 200) {
                    g.tick(dt);
                    auto p = g.enemyPosition(g.enemies[0]);
                    CHECK(std::hypot(p.x - previous.x, p.y - previous.y) < .2f);
                    auto t = g.enemyTangent(g.enemies[0]);
                    CHECK(std::abs(std::hypot(t.x, t.y) - 1) < .0001f);
                    previous = p;
                }
                CHECK(ticks < 200);
                auto end = g.position(float(target), 0);
                CHECK(std::hypot(previous.x - end.x, previous.y - end.y) < .0001f);
                CHECK(g.enemies[0].stopTime > 0);
            }
        std::cout << "Timed rim kills/misses, angular windows, source turn speed/geometry, capture "
                     "lanes, pause, jump and order-independent impacts passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

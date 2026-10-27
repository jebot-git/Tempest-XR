#include "game.hpp"
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define REQUIRE(condition)                                                                         \
    do {                                                                                           \
        if (!(condition))                                                                          \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #condition);                                              \
    } while (false)
static void advance(Game &g, float seconds) {
    for (int i = 0; i < int(seconds * 120); i++)
        g.tick(1.f / 120);
}
int main() {
    try {
        Game g(42);
        g.start();
        REQUIRE(g.web.closed);
        REQUIRE(g.web.lanes() == 16);
        REQUIRE(g.lane == 7);
        g.steer(.4f);
        REQUIRE(g.lane == 7);
        g.steer(.6f);
        REQUIRE(g.lane == 8);
        g.steer(10);
        REQUIRE(g.lane == 2);
        g.steer(-4);
        REQUIRE(g.lane == 14);
        // Player fire follows the held button: no unattended fire, repeat while
        // held, and no new shots after release (existing shots keep travelling).
        g.start();
        advance(g, .5f);
        REQUIRE(g.shots.empty());
        g.fireHeld = true;
        advance(g, .2f);
        auto fired = std::count(g.sounds.begin(), g.sounds.end(), Sound::Shot);
        REQUIRE(fired >= 2);
        g.fireHeld = false;
        advance(g, .2f);
        REQUIRE(std::count(g.sounds.begin(), g.sounds.end(), Sound::Shot) == fired);
        g.start(3);
        REQUIRE(!g.web.closed);
        REQUIRE(g.web.lanes() == 11);
        REQUIRE(g.web.points[0].y < -1);
        g.steer(32);
        REQUIRE(g.lane == 10);
        g.steer(-32);
        REQUIRE(g.lane == 0);
        g.start();
        g.fireHeld = false;
        g.enemies = {{Kind::Flipper, g.lane, .3f}};
        g.shots = {{g.lane, .29f, .28f}};
        g.tick(.01f);
        REQUIRE(std::none_of(g.enemies.begin(), g.enemies.end(),
                             [](auto &e) { return Game::hostile(e.kind); }));
        REQUIRE(g.score == 150);
        REQUIRE(g.shots.empty());
        g.start(3);
        g.enemies = {{Kind::Tanker, 5, .6f}};
        g.shots = {{5, .59f, .58f}};
        g.tick(.01f);
        g.enemies.erase(std::remove_if(g.enemies.begin(), g.enemies.end(),
                                       [](auto &e) { return e.kind == Kind::Powerup; }),
                        g.enemies.end());
        REQUIRE(g.enemies.size() == 2);
        REQUIRE(g.enemies[0].kind == Kind::Flipper);
        REQUIRE(g.enemies[0].lane == 4);
        REQUIRE(g.enemies[1].lane == 6);
        REQUIRE(g.score == 100);
        g.start();
        g.enemies = {{Kind::Flipper, g.lane, .4f}, {Kind::Tanker, 4, .7f}};
        g.zap();
        REQUIRE(g.zaps == 1);
        REQUIRE(g.score == 250);
        g.zap();
        REQUIRE(g.score == 250);
        g.tick(.01f);
        REQUIRE(std::none_of(g.enemies.begin(), g.enemies.end(),
                             [](auto &e) { return Game::hostile(e.kind); }));
        g.start();
        g.invulnerable = 0;
        g.enemies = {{Kind::Flipper, g.lane, 0}};
        g.jumpEnabled = true;
        g.jump();
        g.tick(.01f);
        REQUIRE(g.lives == 3);
        float cooldown = g.jumpCooldown;
        g.jumpEnabled = true;
        g.jump();
        REQUIRE(g.jumpCooldown == cooldown);
        g.start();
        g.invulnerable = 0;
        g.enemies = {{Kind::Flipper, g.lane, 0}};
        g.tick(.01f);
        REQUIRE(g.lives == 2);
        g.tick(.01f);
        REQUIRE(g.lives == 2);
        g.pause();
        float time = g.time, death = g.deathTime;
        advance(g, 1);
        REQUIRE(g.time == time);
        REQUIRE(g.deathTime == death);
        g.pause();
        REQUIRE(g.mode == Mode::Playing);
        g.start();
        g.enemiesRemaining = 0;
        g.enemies = {{Kind::Flipper, 4, .5f}};
        g.tick(.01f);
        REQUIRE(g.mode == Mode::Playing);
        g.zap();
        g.tick(.01f);
        REQUIRE(g.mode == Mode::Transition);
        advance(g, 2.5f);
        REQUIRE(g.level == 2);
        REQUIRE(g.web.name == "SQUARE");
        REQUIRE(g.zaps == 2);
        g.start(4);
        g.spikes[3] = .4f;
        Enemy spiker{Kind::Spiker, 3, .4f};
        spiker.spikerMotion = SpikerMotion::Climbing;
        g.enemies = {spiker};
        g.tick(1.f / 60);
        REQUIRE(g.spikes[3] < .4f);
        g.zap();
        g.tick(.01f);
        REQUIRE(g.spikes[3] < .4f); // Superzapper leaves spikes behind.
        g.shots = {{3, .39f, .38f}};
        g.tick(.01f);
        REQUIRE(g.spikes[3] > .41f && g.spikes[3] < .42f);
        g.start();
        g.invulnerable = 0;
        g.enemiesRemaining = 0;
        g.spikes[size_t(g.lane)] = .5f;
        g.tick(.01f);
        REQUIRE(g.mode == Mode::Transition);
        advance(g, .7f);
        g.jumpEnabled = true;
        g.jump();
        advance(g, 1.7f);
        REQUIRE(g.lives == 3); // Jumping across the spike tip protects the claw.
        g.start();
        g.invulnerable = 0;
        g.enemiesRemaining = 0;
        g.spikes[size_t(g.lane)] = .5f;
        g.tick(.01f);
        advance(g, 1.4f);
        REQUIRE(g.lives == 2);
        // Ship-attached rig: rim movement pans, jump pushes back, pause freezes it.
        g.start();
        auto initial = g.xrCameraPosition();
        auto ship = g.shipPosition();
        REQUIRE(std::abs(initial.x - ship.x) < .0001f);
        REQUIRE(std::abs(initial.y - ship.y) < .0001f);
        REQUIRE(std::abs(initial.z - ship.z - 3.8f) < .0001f);
        g.steer(3);
        advance(g, .25f);
        auto panned = g.xrCameraPosition();
        REQUIRE(std::hypot(panned.x - initial.x, panned.y - initial.y) > .5f);
        g.jumpEnabled = true;
        g.jump();
        advance(g, .4f);
        REQUIRE(g.xrCameraPosition().z > .75f);
        g.pause();
        auto held = g.xrCameraPosition();
        advance(g, .2f);
        REQUIRE(g.xrCameraPosition().z == held.z);
        g.pause();
        // The rig moves beyond the old tube and joins the next lane continuously.
        g.start();
        g.enemiesRemaining = 0;
        g.tick(1.f / 120);
        float previousDistance = 0;
        Vec3 beforeReset{};
        bool completed = false;
        for (int i = 0; i < 400; i++) {
            if (g.mode == Mode::Transition) {
                REQUIRE(g.warpDistance() + .0001f >= previousDistance);
                previousDistance = g.warpDistance();
                beforeReset = g.xrCameraPosition();
            }
            g.tick(1.f / 120);
            if (g.level == 2) {
                completed = true;
                break;
            }
        }
        REQUIRE(completed);
        REQUIRE(previousDistance > 13.99f);
        auto afterReset = g.xrCameraPosition();
        REQUIRE(std::hypot(beforeReset.x - afterReset.x, beforeReset.y - afterReset.y) < .01f);
        REQUIRE(std::abs(beforeReset.z + Game::warpTravel - afterReset.z) < .01f);
        for (int level = 1; level <= 100; level++) {
            g.start(level);
            REQUIRE(g.web.lanes() >= 7);
            for (auto p : g.web.points)
                REQUIRE(std::isfinite(p.x) && std::isfinite(p.y));
        }
        // Sustained deterministic stress: no invalid lanes or unbounded objects.
        g.start(4);
        g.lives = 10000;
        for (int i = 0; i < 120 * 180; i++) {
            if (i % 7 == 0)
                g.steer(i % 2 ? 1 : -1);
            if (i % 240 == 0)
                g.jumpEnabled = true;
            g.jump();
            g.tick(1.f / 120);
            for (auto &e : g.enemies)
                REQUIRE(e.lane >= 0 && e.lane < g.web.lanes());
            REQUIRE(g.enemies.size() < 150);
            g.sounds.clear();
        }
        std::cout << "Gameplay checks passed: geometry, wheel, collisions, tankers, zap, jump, "
                     "damage, pause, progression, 180s stress\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

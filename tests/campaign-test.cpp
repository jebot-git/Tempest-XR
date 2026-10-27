#include "campaign.hpp"
#include "game.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #c);                                                      \
    } while (false)
static void step(Game &g, float seconds) {
    for (int i = 0; i < int(seconds * 120); i++)
        g.tick(1.f / 120);
}
static int count(const Game &g, Kind kind) {
    return int(
        std::count_if(g.enemies.begin(), g.enemies.end(), [&](auto &e) { return e.kind == kind; }));
}
static void pickup(Game &g) {
    g.spawn(Kind::Powerup, g.lane, 0);
    g.tick(.01f);
}
int main() {
    try {
        // All three audit reproductions, including a jump that overlaps the coordinate reset.
        Game g;
        g.start();
        g.enemiesRemaining = 0;
        g.tick(1.f / 120);
        step(g, 1.85f);
        g.jumpEnabled = true;
        g.jump();
        Vec3 before{};
        while (g.level == 1) {
            before = g.xrCameraPosition();
            g.tick(1.f / 120);
        }
        auto after = g.xrCameraPosition();
        CHECK(std::abs(before.z + Game::warpTravel - after.z) < .03f);
        CHECK(g.jumpTime > 0);
        g.start();
        g.score = 19500;
        g.enemiesRemaining = 0;
        g.tick(.01f);
        CHECK(g.score == 20500);
        CHECK(g.lives == 4);
        g.addScore(40000);
        CHECK(g.lives == 6); // Every crossed threshold awards a life.
        g.start();
        g.invulnerable = 0;
        g.enemiesRemaining = 0;
        g.spikes[size_t(g.lane + 1)] = .1f;
        g.tick(.01f);
        step(g, 1);
        g.steer(1);
        g.tick(.01f);
        CHECK(g.lives == 2);
        // Both boundaries of each bonus family preserve the ship-follow rig.
        for (int type = 0; type < 3; ++type) {
            g.start();
            g.steer(3);
            step(g, .3f);
            auto entry = g.xrCameraPosition();
            g.startBonus(type);
            auto same = g.xrCameraPosition();
            CHECK(std::hypot(same.x - entry.x, same.y - entry.y) < .0001f);
            CHECK(std::abs(same.z - entry.z) < .0001f);
            g.steer(18.75f);
            g.bonusDown = true;
            while (!g.bonusFinished)
                g.tick(1.f / 120);
            Vec3 last;
            while (g.mode == Mode::Bonus) {
                last = g.xrCameraPosition();
                g.tick(1.f / 120);
            }
            same = g.xrCameraPosition();
            CHECK(std::hypot(same.x - last.x, same.y - last.y) < .03f);
            CHECK(std::abs(same.z - last.z) < .03f);
            while (g.mode == Mode::Transition) {
                last = g.xrCameraPosition();
                g.tick(1.f / 120);
            }
            same = g.xrCameraPosition();
            CHECK(std::hypot(same.x - last.x, same.y - last.y) < .03f);
            CHECK(std::abs(last.z + Game::warpTravel - same.z) < .03f);
        }
        // Enemy bullets are shootable, including crossing between two ticks.
        for (int weapon = 0; weapon < 3; ++weapon) {
            g.start(9);
            g.autoFire = false;
            g.boost = weapon == 2;
            g.spawn(Kind::EnemyShot, 0, .5f);
            Shot shot{0, .48f, .48f};
            shot.ally = weapon == 1;
            g.shots = {shot};
            g.tick(.02f);
            CHECK(count(g, Kind::EnemyShot) == 0);
            CHECK(g.shots.size() == (weapon == 2 ? 1u : 0u));
            CHECK(g.score == 0);
        }
        g.start(9);
        g.autoFire = false;
        g.spawn(Kind::EnemyShot, 1, .5f);
        g.shots = {{0, .48f, .48f}};
        g.tick(.02f);
        CHECK(count(g, Kind::EnemyShot) == 1);
        CHECK(g.shots.size() == 1);
        // Tanker variants must release the corresponding two contents.
        for (auto pair :
             {std::pair{Kind::Tanker, Kind::Flipper}, std::pair{Kind::FuseTanker, Kind::Fuseball},
              std::pair{Kind::PulsarTanker, Kind::Pulsar}}) {
            g.start();
            g.autoFire = false;
            g.spawn(pair.first, 3, .6f);
            g.shots = {{3, .59f, .58f}};
            g.tick(.01f);
            CHECK(count(g, pair.second) == 2);
            CHECK(count(g, pair.first) == 0);
        }
        g.start();
        g.autoFire = false;
        g.lane = 6;
        g.spawn(Kind::Fuseball, 0, .8f);
        step(g, .6f);
        CHECK(g.enemies[0].lane == 1);
        CHECK(g.enemies[0].depth < .8f);
        g.start();
        g.autoFire = false;
        g.invulnerable = 0;
        g.waveTime = 1.8f;
        g.spawn(Kind::Pulsar, g.lane, .8f);
        CHECK(g.pulseActive());
        g.tick(.01f);
        CHECK(g.lives == 2);
        g.start();
        g.autoFire = false;
        g.spawn(Kind::Pulsar, 0, .0001f);
        g.tick(.01f);
        CHECK(count(g, Kind::PulsarSpark) == 2);
        for (Kind kind : {Kind::SuperFlipper, Kind::SuperFlipper2, Kind::Mirror}) {
            g.start();
            g.autoFire = false;
            g.spawn(kind, 0, .6f);
            g.shots = {{0, .59f, .58f}};
            g.tick(.01f);
            CHECK(count(g, kind) == 1);
            CHECK(count(g, Kind::EnemyShot) == 1);
            CHECK(g.enemies[0].hp >= 1);
        }
        g.start();
        g.autoFire = false;
        g.spawn(Kind::EnemyShot, g.lane, .001f);
        g.invulnerable = 0;
        g.tick(.01f);
        CHECK(g.lives == 2);
        CHECK(count(g, Kind::EnemyShot) == 0);
        g.start();
        g.autoFire = false;
        g.spawn(Kind::Flipper, 0, .7f);
        g.spawn(Kind::Fuseball, 1, .7f);
        g.spawn(Kind::Mirror, 2, .7f);
        g.zap();
        CHECK(g.zaps == 1);
        g.tick(.01f);
        CHECK(g.enemies.empty());
        g.spawn(Kind::Flipper, 0, .7f);
        g.spawn(Kind::Flipper, 1, .6f);
        g.zap();
        g.tick(.01f);
        CHECK(count(g, Kind::Flipper) == 1);
        CHECK(g.zaps == 0);
        // Pickup order: laser, points, jump, points, friendly droid, token.
        g.start();
        g.autoFire = false;
        g.jump();
        CHECK(g.jumpTime == 0);
        pickup(g);
        CHECK(g.boost);
        pickup(g);
        CHECK(g.score == 2000);
        pickup(g);
        CHECK(g.jumpEnabled);
        pickup(g);
        pickup(g);
        CHECK(g.droidActive);
        pickup(g);
        CHECK(g.bonusTokens == 1);
        g.spawn(Kind::Flipper, 5, .5f);
        step(g, .2f);
        CHECK(std::any_of(g.shots.begin(), g.shots.end(), [](auto &s) { return s.ally; }));
        // Play every course with public wheel/button controls, including impulses.
        std::set<int> gateEffects;
        for (int type = 0; type < 3; type++) {
            g.start();
            for (int course = 0; course < 8; ++course) {
                int startingLevel = g.level;
                g.startBonus(type);
                if (type == 0 && course == 0) {
                    CHECK(g.bonusGateDelay(0) == 8);
                    CHECK(g.bonusGateEffect(0) == 0);
                    CHECK(g.bonusGateDistance(1) - g.bonusGateDistance(0) == 108);
                }
                int steps = 0;
                while (!g.bonusFinished && steps++ < 120 * 600) {
                    auto target = g.bonusTarget(g.bonusGate);
                    if (type == 1) {
                        float delta = std::atan2(target.y, target.x) - g.bonusAngle;
                        delta = std::atan2(std::sin(delta), std::cos(delta));
                        g.steer(delta / .19634954f);
                    } else {
                        g.steer((target.x - g.bonusX) / .16f);
                        float correction = (target.y - g.bonusY) * 30 - g.bonusImpulse;
                        g.bonusUp = correction > .8f;
                        g.bonusDown = correction < -.8f;
                    }
                    int gate = g.bonusGate, effect = g.bonusGateEffect(gate), score = g.score;
                    float speed = g.bonusSpeed;
                    g.tick(1.f / 120);
                    if (g.bonusGate != gate) {
                        gateEffects.insert(effect);
                        CHECK(g.score - score == g.bonusLastScore);
                        if (effect == 1)
                            CHECK(g.bonusImpulse > 2.3f);
                        if (effect == 2)
                            CHECK(g.bonusImpulse < -2.3f);
                        if (effect == 3)
                            CHECK(std::abs(g.bonusSpeed - speed * 1.125f) < .001f);
                        if (effect == 4)
                            CHECK(g.bonusWon);
                    }
                    g.sounds.clear();
                }
                CHECK(g.bonusFinished);
                CHECK(g.bonusWon);
                CHECK(g.bonusHits == g.bonusGateCount());
                step(g, 2.1f);
                CHECK(g.mode == Mode::Transition);
                CHECK(g.targetLevel == startingLevel + 5);
                step(g, 2.5f);
                CHECK(g.level == startingLevel + 5);
            }
        }
        for (int effect = 0; effect <= 4; ++effect)
            CHECK(gateEffects.count(effect));
        g.start();
        g.bonusTokens = 3;
        g.enemiesRemaining = 0;
        g.tick(.01f);
        CHECK(g.mode == Mode::Bonus);
        g.bonusX = -3;
        g.bonusY = -3;
        step(g, 2.1f);
        CHECK(g.bonusFinished && !g.bonusWon);
        CHECK(g.lives == 3);
        step(g, 1.7f);
        CHECK(g.targetLevel == 2);
        CHECK(campaignWaves.size() == 100);
        CHECK(campaignWaves[0].count == 25);
        CHECK(campaignWaves[2].spawns[1].periodTicks == 444);
        CHECK(campaignWaves[98].count == campaignWaves[99].count);
        std::set<Kind> seen;
        // Exercise every imported schedule until its exact generation budget is exhausted.
        for (int level = 1; level <= 100; level++) {
            g.start(level);
            g.autoFire = false;
            g.invulnerable = 100000;
            int frames = 0;
            while (g.mode == Mode::Playing && frames++ < 600 * 60) {
                g.enemies.clear();
                g.tick(1.f / 60);
                for (auto &e : g.enemies)
                    seen.insert(e.kind);
                g.sounds.clear();
            }
            CHECK(g.enemiesRemaining == 0);
            CHECK(g.mode == (level == 100 ? Mode::Victory : Mode::Transition));
            CHECK(g.unlockedLevel == 100 || g.unlockedLevel >= level);
        }
        for (int type = 0; type < 12; type++)
            CHECK(seen.count(Game::kindForSource(type)) > 0);
        CHECK(g.mode == Mode::Victory);
        CHECK(g.level == 100);
        std::cout << "Audit regressions, all enemy families, two zaps, powerups/droid, three bonus "
                     "families, all 100 original spawn schedules and victory passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

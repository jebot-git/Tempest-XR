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
    g.invulnerable = 1000;
    g.enemiesRemaining =
        1000; // Keep generation unfinished; short tests precede first scheduled spawn.
    return g;
}
static int count(const Game &g, Kind k) {
    return int(std::count_if(g.enemies.begin(), g.enemies.end(),
                             [&](const auto &e) { return e.kind == k && !e.dead; }));
}
static Enemy &enemy(Game &g, Kind k) {
    auto e = std::find_if(g.enemies.begin(), g.enemies.end(), [&](auto &a) { return a.kind == k; });
    CHECK(e != g.enemies.end());
    return *e;
}
static void step(Game &g, int ticks) {
    for (int i = 0; i < ticks; ++i)
        g.tick(dt);
}
static void shoot(Game &g, const Enemy &e, bool powered = false, bool ally = false) {
    g.boost = powered;
    Shot s{e.lane, std::max(0.f, e.depth - .03f)};
    s.ally = ally;
    g.shots = {s};
    g.tick(1.f / 60);
}
int main() {
    try {
        // Exact source phase counter: only phase seven is deadly, with +1 frame on expiry.
        for (auto tuning :
             {std::pair{1, 9}, std::pair{21, 8}, std::pair{45, 4}, std::pair{100, 7}}) {
            auto g = setup(tuning.first);
            for (int frame = 0; frame < 240; ++frame) {
                g.waveTime = double(frame) / 60;
                int phase = (frame / tuning.second) & 15;
                CHECK(g.pulsePhase() == phase && g.pulseActive() == (phase == 7));
            }
        }
        {
            auto g = setup();
            g.waveTime = 62.f / 60;
            CHECK(g.pulsePhase() == 6 && !g.pulseActive());
            g.waveTime = 63.f / 60;
            CHECK(g.pulsePhase() == 7 && g.pulseActive());
            g.waveTime = 72.f / 60;
            CHECK(g.pulsePhase() == 8 && !g.pulseActive());
            g.spawn(Kind::Pulsar, 4, .8f);
            float before = g.enemies[0].depth;
            step(g, 120);
            CHECK(std::abs(g.enemies[0].depth - (before - .1875f)) < .0001f);
        }
        // Super type three runs away from the claw while below the rim, then pursues it.
        {
            auto g = setup();
            g.lane = 5;
            g.spawn(Kind::SuperFlipper2, 3, .5f);
            g.tick(dt);
            CHECK(g.enemies[0].motion == FlipperMotion::Turning && g.enemies[0].lane == 2);
            float before = g.enemies[0].depth;
            step(g, 8);
            CHECK(g.enemies[0].depth < before && g.enemies[0].runningBelowRim);
            g.enemies[0].depth = .000001f;
            step(g, 2);
            CHECK(g.enemies[0].rim && !g.enemies[0].runningBelowRim);
            for (int i = 0; i < 250 && g.enemies[0].lane != 3; ++i)
                g.tick(dt);
            CHECK(g.enemies[0].lane == 3);
        }
        // Spikers choose an empty rail, only extend after reaching its tip, then retreat.
        {
            auto g = setup();
            g.spawn(Kind::Spiker);
            g.spawn(Kind::Spiker);
            CHECK(count(g, Kind::Spiker) == 1);
            step(g, 2);
            auto &e = enemy(g, Kind::Spiker);
            int rail = e.lane;
            CHECK(e.spikerMotion == SpikerMotion::Climbing && g.spikes[size_t(rail)] == .8125f);
            step(g, 50);
            CHECK(g.spikes[size_t(rail)] == .8125f && enemy(g, Kind::Spiker).buildFrames == 50);
            int frames = 0;
            while (enemy(g, Kind::Spiker).spikerMotion == SpikerMotion::Climbing &&
                   frames++ < 3000) {
                // The imported generator is independent of this single-spiker fixture.
                g.enemies.erase(std::remove_if(g.enemies.begin(), g.enemies.end(),
                                               [](auto &a) { return a.kind != Kind::Spiker; }),
                                g.enemies.end());
                g.tick(dt);
            }
            CHECK(frames < 3000 && enemy(g, Kind::Spiker).buildFrames == -1);
            CHECK(g.spikes[size_t(rail)] < .8125f && g.spikes[size_t(rail)] > .0625f);
            while (enemy(g, Kind::Spiker).spikerMotion != SpikerMotion::Seeking && frames++ < 6000)
                g.tick(dt);
            CHECK(frames < 6000);
            step(g, 2);
            CHECK(enemy(g, Kind::Spiker).lane != rail);
            g.zap();
            for (int i = 0; i < 120 && count(g, Kind::Spiker); ++i)
                g.tick(dt);
            CHECK(count(g, Kind::Spiker) == 0 && g.hasSpike(rail));
        }
        {
            auto g = setup();
            std::fill(g.spikes.begin(), g.spikes.end(), .3f);
            g.spikes[3] = .9f;
            g.spawn(Kind::Spiker);
            step(g, 2);
            CHECK(enemy(g, Kind::Spiker).lane == 3); // Shortest existing spike.
        }
        for (bool super : {false, true})
            for (bool laser : {false, true}) {
                auto g = setup();
                g.spikes[3] = .4f;
                g.superSpikes[3] = super;
                Enemy tip{Kind::Spiker, 3, .4f};
                shoot(g, tip, laser);
                bool shortens = !super || laser;
                CHECK(std::abs(g.spikes[3] - (.4f + (shortens ? 3.f / 160 : 0))) < .000001f);
                CHECK(g.score == (shortens ? 3 : 0));
                CHECK(g.shots.size() == (laser && !super ? 1u : 0u));
            }
        {
            auto g = setup();
            g.spikes[3] = 1 - 3.f / 160;
            shoot(g, Enemy{Kind::Spiker, 3, g.spikes[3]});
            CHECK(g.hasSpike(3) && g.spikes[3] == 1 && g.score == 3);
            shoot(g, Enemy{Kind::Spiker, 3, 1});
            CHECK(!g.hasSpike(3) && g.score == 9); // Final shortening plus deletion award.
        }
        {
            auto g = setup();
            g.spikes[3] = 1;
            g.spikePresent[3] = true;
            g.shots = {{3, .96f}, {3, .96f}};
            step(g, 1);
            CHECK(!g.hasSpike(3) && g.score == 6 && g.shots.size() == 1);
        }
        // Fuseballs ride the left rail for 49 frames, cross in 16 two-frame steps.
        {
            auto g = setup();
            g.lane = 5;
            g.spawn(Kind::Fuseball, 3, .8f);
            step(g, 96);
            CHECK(enemy(g, Kind::Fuseball).fuseMotion == FuseMotion::Climbing);
            step(g, 2);
            CHECK(enemy(g, Kind::Fuseball).fuseMotion == FuseMotion::Crossing);
            auto before = g.enemyPosition(enemy(g, Kind::Fuseball));
            step(g, 12);
            auto &e = enemy(g, Kind::Fuseball);
            CHECK(e.crossSteps == 12 && e.lane == 3 && Game::shotVulnerable(e));
            auto after = g.enemyPosition(e);
            CHECK(std::hypot(before.x - after.x, before.y - after.y) > .001f);
            step(g, 52);
            CHECK(enemy(g, Kind::Fuseball).fuseMotion == FuseMotion::Climbing &&
                  enemy(g, Kind::Fuseball).lane == 4);
        }
        for (bool laser : {false, true})
            for (int counter : {15, 13, 12, 4, 3}) {
                auto g = setup();
                g.spawn(Kind::Fuseball, 3, .6f);
                auto &e = enemy(g, Kind::Fuseball);
                e.fuseMotion = FuseMotion::Crossing;
                e.crossSteps = counter;
                shoot(g, e, laser);
                CHECK(count(g, Kind::Fuseball) == (counter >= 4 && counter <= 12 ? 0 : 1));
            }
        {
            auto g = setup();
            g.lane = 0;
            g.spawn(Kind::Fuseball, 3, .6f);
            step(g, 98);
            CHECK(enemy(g, Kind::Fuseball).lane ==
                  2); // Left crossing changes collision lane first.
        }
        {
            auto g = setup();
            g.spawn(Kind::FuseTanker, 3, .5f);
            shoot(g, g.enemies[0]);
            CHECK(count(g, Kind::Fuseball) == 2);
            for (auto &e : g.enemies)
                if (e.kind == Kind::Fuseball) {
                    CHECK(e.fuseMotion == FuseMotion::Crossing && e.crossSteps == 15);
                    CHECK(e.travelFrom == (e.direction > 0 ? 4 : 3));
                }
        }
        for (bool crossing : {false, true}) {
            auto g = setup();
            g.spawn(Kind::Fuseball, g.lane, 0);
            g.invulnerable = 0;
            g.enemies[0].fuseMotion = crossing ? FuseMotion::Crossing : FuseMotion::Climbing;
            step(g, 1);
            CHECK(g.lives == (crossing ? 2 : 3));
        }
        // Climbing hostile droids deflect shots; above the rim they patrol, stop and zap.
        for (bool laser : {false, true}) {
            auto g = setup();
            g.spawn(Kind::Adroid, 3, .5f);
            shoot(g, g.enemies[0], laser);
            CHECK(count(g, Kind::Adroid) == 1 && g.enemies[0].depth > .5f && g.shots.empty());
            g.enemies[0].depth = .999f;
            shoot(g, g.enemies[0], laser);
            CHECK(count(g, Kind::Adroid) == 0);
        }
        {
            auto g = setup();
            g.spawn(Kind::Adroid, 3, 0);
            while (enemy(g, Kind::Adroid).droidMotion == DroidMotion::Climbing)
                g.tick(dt);
            CHECK(enemy(g, Kind::Adroid).depth < -.09375f);
            step(g, 2);
            CHECK(enemy(g, Kind::Adroid).droidMotion == DroidMotion::Crossing);
            int from = enemy(g, Kind::Adroid).lane, dest = enemy(g, Kind::Adroid).travelTo;
            step(g, 48);
            CHECK(enemy(g, Kind::Adroid).lane == from);
            step(g, 6);
            CHECK(enemy(g, Kind::Adroid).lane == dest);
            step(g, 42);
            CHECK(enemy(g, Kind::Adroid).droidMotion == DroidMotion::Zapping);
            g.lane = dest;
            g.invulnerable = 0;
            step(g, 2);
            CHECK(g.lives == 2);
        }
        {
            auto g = setup();
            g.spawn(Kind::Adroid, 3, .5f);
            g.enemies[0].droidMotion = DroidMotion::Zapping;
            step(g, 62);
            CHECK(g.enemies[0].droidMotion == DroidMotion::Zapping);
            step(g, 2);
            CHECK(g.enemies[0].droidMotion == DroidMotion::Starting);
            shoot(g, g.enemies[0]);
            CHECK(count(g, Kind::Adroid) == 0);
        }
        {
            auto g = setup();
            g.spawn(Kind::Adroid, g.lane, -.095f);
            g.enemies[0].droidMotion = DroidMotion::Zapping;
            g.jumpEnabled = true;
            g.jump();
            step(g, 50);
            g.fireHeld = true;
            step(g, 10); // Held-fire counter expires, then the new shot gets its first update.
            CHECK(count(g, Kind::Adroid) == 0 && g.lives == 3);
        }
        // Mirror hits take over all weapons; beast stages shed distinct absorbing projectiles.
        for (bool laser : {false, true})
            for (bool ally : {false, true}) {
                auto g = setup();
                g.spawn(Kind::Mirror, 3, .6f);
                shoot(g, g.enemies[0], laser, ally);
                CHECK(enemy(g, Kind::Mirror).hp == 4 && enemy(g, Kind::Mirror).depth > .6f);
                CHECK(count(g, Kind::MirrorShot) == 1 && g.shots.empty());
            }
        {
            auto g = setup();
            g.spawn(Kind::Mirror, 3, .6f);
            for (int i = 0; i < 5; ++i) {
                shoot(g, enemy(g, Kind::Mirror));
                g.enemies.erase(std::remove_if(g.enemies.begin(), g.enemies.end(),
                                               [](auto &e) {
                                                   return Game::reflected(e.kind) ||
                                                          e.kind == Kind::Powerup;
                                               }),
                                g.enemies.end());
            }
            CHECK(count(g, Kind::Mirror) == 0 && g.shots.size() == 1);
            CHECK(g.score == 1 || g.score == 250 || g.score == 500 || g.score == 750);
        }
        for (bool laser : {false, true}) {
            auto g = setup();
            g.spawn(Kind::Beast, 3, .6f);
            for (int stage = 2; stage >= 0; --stage) {
                shoot(g, enemy(g, Kind::Beast), laser);
                CHECK(count(g, Kind::Beast) == (stage ? 1 : 0));
                CHECK(count(g, Kind::BeastShot) == (stage ? 1 : 0));
                if (stage)
                    CHECK(enemy(g, Kind::Beast).hp == stage);
                g.enemies.erase(std::remove_if(g.enemies.begin(), g.enemies.end(),
                                               [](auto &e) { return Game::reflected(e.kind); }),
                                g.enemies.end());
            }
            CHECK(g.score == 150);
        }
        for (Kind kind : {Kind::MirrorShot, Kind::BeastShot})
            for (bool laser : {false, true}) {
                auto g = setup();
                g.spawn(kind, 3, .5f);
                shoot(g, g.enemies[0], laser);
                CHECK(count(g, kind) == 1 && g.score == 0);
                CHECK(g.shots.size() == (kind == Kind::MirrorShot || laser ? 1u : 0u));
                float before = g.enemies[0].depth;
                step(g, 12);
                CHECK(std::abs(before - g.enemies[0].depth -
                               g.shotSpeed() * .1f * (kind == Kind::BeastShot ? .5f : 1.f)) <
                      .00001f);
                g.enemiesRemaining = 0;
                g.tick(dt);
                CHECK(g.mode == Mode::Transition);
            }
        for (Kind kind : {Kind::MirrorShot, Kind::BeastShot}) {
            auto g = setup();
            g.spawn(kind, 3, -.12f);
            step(g, 1);
            CHECK(count(g, kind) == 1);
            step(g, 10);
            CHECK(count(g, kind) == 0);
        }
        {
            auto g = setup();
            g.spawn(Kind::Mirror, 3, .6f);
            g.spawn(Kind::MirrorShot, 4, .5f);
            g.spawn(Kind::Adroid, 5, .5f);
            g.spawn(Kind::Beast, 6, .5f);
            g.zap();
            step(g, 8);
            CHECK(count(g, Kind::Mirror) == 1 && count(g, Kind::MirrorShot) == 1 &&
                  count(g, Kind::Adroid) == 1 && count(g, Kind::Beast) == 0);
        }
        {
            auto g = setup();
            for (int i = 0; i < 8; ++i)
                g.spawn(Kind::MirrorShot, 3, .5f);
            g.fireHeld = true;
            step(g, 1);
            CHECK(g.shots.empty()); // Reflections still occupy their original shot entitlement.
            g.droidActive = true;
            step(g, 12);
            CHECK(g.shots.size() == 1 && g.shots[0].ally);
        }
        {
            auto g = setup(100);
            g.enemiesRemaining = 0;
            int mutations = 0, runners = 0;
            for (int trial = 0; trial < 32; ++trial) {
                g.enemies.clear();
                g.spawn(Kind::Tanker, 3, .6f);
                shoot(g, g.enemies[0]);
                for (auto &e : g.enemies)
                    if (Game::flipperKind(e.kind) && e.kind != Kind::Flipper) {
                        ++mutations;
                        runners += e.kind == Kind::SuperFlipper2;
                        CHECK(e.emerging && std::abs(e.scarper) >= 1 && std::abs(e.scarper) <= 4);
                        CHECK((e.scarper > 0) == (e.direction > 0));
                    }
            }
            CHECK(mutations > 40 && runners > 20);
        }
        // Shared fire counter and source bullet cap apply even on wave one.
        {
            auto g = setup();
            g.enemiesRemaining = 0;
            g.spawn(Kind::Flipper, 3, 1);
            step(g, 224);
            CHECK(count(g, Kind::EnemyShot) == 0);
            step(g, 2);
            CHECK(count(g, Kind::EnemyShot) == 1);
            step(g, 226);
            CHECK(count(g, Kind::EnemyShot) <= 1);
        }
        {
            auto g = setup();
            g.enemiesRemaining = 0;
            g.spawn(Kind::Tanker, 3, 1);
            step(g, 230);
            CHECK(count(g, Kind::EnemyShot) == 0);
        }
        {
            auto g = setup();
            g.enemiesRemaining = 0;
            g.spawn(Kind::Flipper, 3, 1);
            g.spawn(Kind::Flipper, 4, 1);
            step(g, 112);
            CHECK(count(g, Kind::EnemyShot) == 0);
            step(g, 2);
            CHECK(count(g, Kind::EnemyShot) == 1);
            step(g, 112);
            CHECK(count(g, Kind::EnemyShot) == 1); // Second firing attempt hits wave-one cap.
        }
        {
            auto g = setup();
            g.fireHeld = true;
            step(g, 16);
            CHECK(g.shots.size() == 2);
            CHECK(std::abs(g.shots[0].depth - g.shotSpeed() * 4 / 60) < .00001f);
            step(g, 100);
            CHECK(g.shots.size() <= 8);
        }
        std::cout << "Source enemy phases, building, crossing, shields, reflected shots and shared "
                     "fire rules passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

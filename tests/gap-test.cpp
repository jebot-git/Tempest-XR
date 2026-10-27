#include "game.hpp"
#include <iostream>
#include <stdexcept>
using namespace tempest;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c))                                                                                  \
            throw std::runtime_error(std::to_string(__LINE__) + ": " #c);                          \
    } while (false)
static constexpr float dt = 1.f / 120;
static Game setup(int level = 1) {
    Game g(42);
    g.start(level);
    g.tick(1.f / 120);
    g.invulnerable = 1000;
    return g;
}
static void steps(Game &g, int n) {
    for (int i = 0; i < n; ++i)
        g.tick(dt);
}
static int count(const Game &g, Kind kind) {
    return int(std::count_if(g.enemies.begin(), g.enemies.end(),
                             [=](const auto &e) { return !e.dead && e.kind == kind; }));
}
int main() {
    try {
        // A hit cancels rim entry before the beast loses its staged shield.
        for (float depth : {.1f, .0001f}) {
            auto g = setup();
            g.spawn(Kind::Beast, 3, depth);
            g.shots = {{3, depth, depth}};
            steps(g, 1);
            CHECK(count(g, Kind::Beast) == 1 && count(g, Kind::BeastShot) == 1 && g.score == 0);
            CHECK(g.enemies[0].hp == 2 && !g.enemies[0].rim &&
                  g.enemies[0].motion == FlipperMotion::Climbing);
        }
        for (float depth : {-.08f, -.0937f}) {
            auto g = setup();
            g.spawn(Kind::Adroid, 3, depth);
            g.shots = {{3, depth, depth}};
            steps(g, 1);
            CHECK(count(g, Kind::Adroid) == 1 && g.enemies[0].droidMotion == DroidMotion::Climbing);
            CHECK(!g.enemies[0].rim && g.enemies[0].depth > 0);
        }
        {
            Game g;
            g.start();
            g.invulnerable = 1000;
            std::vector<int> dispatched;
            for (int tick = 1; tick <= 850; ++tick) {
                int before = g.enemiesRemaining;
                steps(g, 1);
                if (before != g.enemiesRemaining)
                    dispatched.push_back(tick);
                g.enemies.clear();
            }
            CHECK(dispatched == std::vector<int>({122, 364, 606, 848}));
        }
        {
            auto g = setup();
            g.waveTime = 7 * 14.f / 60;
            g.invulnerable = 0;
            g.spawn(Kind::Pulsar, g.lane, 1, true);
            steps(g, 1);
            CHECK(g.lives == 3 && g.enemies[0].entering);
            g.enemies[0].depth = 1.001f;
            g.enemies[0].buildFrames = 1;
            steps(g, 2);
            CHECK(g.enemies[0].entering);
            g.waveTime = 0;
            steps(g, 2);
            CHECK(!g.enemies[0].entering && g.lives == 3);
        }
        {
            auto g = setup();
            g.spawn(Kind::Flipper, 3, 1, true);
            steps(g, 298);
            CHECK(g.enemies[0].entering);
            steps(g, 2);
            CHECK(!g.enemies[0].entering && g.enemies[0].depth < 1);
        }
        for (bool token : {false, true}) {
            auto g = setup();
            g.spawn(Kind::Mirror, 1, .5f);
            g.spawn(Kind::MirrorShot, 2, .5f);
            g.spawn(Kind::Adroid, 3, .5f);
            g.spawn(Kind::Beast, 4, .5f);
            g.spawn(Kind::Flipper, 5, .5f);
            g.spawn(Kind::Flipper, 6, .5f);
            if (token) {
                g.powerupStage = 5;
                g.spawn(Kind::Powerup, g.lane, 0);
                steps(g, 1);
            } else
                g.zap();
            CHECK(count(g, Kind::Flipper) == 2);
            steps(g, 8);
            CHECK(count(g, Kind::Flipper) == 1);
            steps(g, 8);
            CHECK(count(g, Kind::Flipper) == 0);
            CHECK(count(g, Kind::Mirror) == 1 && count(g, Kind::MirrorShot) == 1);
            steps(g, 8);
            CHECK(count(g, Kind::Adroid) == 1 && count(g, Kind::Beast) == 0);
        }
        {
            auto g = setup();
            g.droidActive = true;
            g.fireHeld = true;
            for (int i = 0; i < 8; ++i)
                g.shots.push_back({0, .2f});
            steps(g, 12);
            CHECK(g.shotSlots(false) == 8 && g.shotSlots(true) == 1);
            CHECK(g.droidLane != 0); // Sixteen-step autonomous patrol even without a target.
        }
        {
            auto g = setup();
            for (int i = 0; i < 10; ++i)
                g.spawn(Kind::Flipper, 0);
            CHECK(g.canGenerate());
            g.spawn(Kind::Tanker, 1);
            CHECK(!g.canGenerate() && g.objectSlots() == 13);
            g.enemies.pop_back();
            g.spikePresent[0] = true;
            CHECK(g.canGenerate()); // Spike allocation lowers noclog as well as afree.
        }
        {
            auto g = setup(3);
            int last = g.web.lanes() - 1;
            g.spawn(Kind::Fuseball, last, .5f);
            auto &e = g.enemies[0];
            e.fuseMotion = FuseMotion::Crossing;
            e.direction = 1;
            e.travelFrom = last;
            e.travelTo = last + 1;
            e.crossSteps = 0;
            e.crossFrames = 0;
            auto expected = g.edge(last + 1, e.depth);
            steps(g, 2);
            auto arrived = g.enemyPosition(g.enemies[0]);
            CHECK(std::abs(arrived.x - expected.x) < .00001f &&
                  std::abs(arrived.y - expected.y) < .00001f);
            CHECK(g.enemies[0].rail == last + 1);
        }
        {
            auto g = setup();
            g.startBonus(1);
            constexpr float tau = 6.283185307f;
            for (int target = 0; target < 256; ++target) {
                g.tunnel[8].track = target;
                for (int player = 0; player < 256; ++player) {
                    g.bonusAngle = (player + .25f) * tau / 256;
                    int distance = std::abs(((-target) & 255) - ((-player) & 255));
                    if (distance > 127)
                        distance = 255 - distance;
                    CHECK(g.tunnelOnTrack() == (distance <= 32));
                }
            }
        }
        {
            auto g = setup(4);
            g.enemiesRemaining = 0;
            Enemy e{Kind::Flipper, 3, 0};
            e.rim = true;
            e.motion = FlipperMotion::Stopped;
            g.enemies = {e};
            steps(g, 1);
            CHECK(g.mode == Mode::Transition && g.score == 0 && g.message == "AVOID THE SPIKES");
            float age = g.enemies[0].age;
            int lives = g.lives;
            steps(g, 120);
            CHECK(g.enemies[0].age > age + .9f && g.lives == lives);
        }
        for (int level : {1, 3, 4, 16, 17, 99}) {
            auto g = setup(level);
            g.enemiesRemaining = 0;
            steps(g, 1);
            CHECK(g.score == 0);
            CHECK(g.clearanceWarning() == (level >= 4 && level <= 16));
            if (level == 99) {
                CHECK(g.mode == Mode::Transition);
                steps(g, 290);
                CHECK(g.mode == Mode::Victory && g.beastlyUnlocked);
            }
        }
        // A consumed bullet retains its owner's entitlement through xshot's eight updates.
        {
            auto g = setup();
            g.spawn(Kind::Flipper, 3, .5f);
            g.shots = {{3, .49f, .49f}};
            steps(g, 1);
            CHECK(g.shots.empty() && g.shotSlots(false) == 1 && g.shotSlots(true) == 0);
            steps(g, 14);
            CHECK(g.shotSlots(false) == 1);
            steps(g, 2);
            CHECK(g.shotSlots(false) == 0);
        }
        // run_fuseball checks _sz before its rail/crossing dispatch.
        for (bool crossing : {false, true}) {
            auto g = setup();
            g.spawn(Kind::Fuseball, 3, .5f);
            g.enemies[0].fuseMotion = crossing ? FuseMotion::Crossing : FuseMotion::Climbing;
            g.enemies[0].crossSteps = 8;
            g.zap();
            steps(g, 8);
            CHECK(count(g, Kind::Fuseball) == 0);
        }
        {
            auto g = setup(17);
            g.start(17, true);
            CHECK(g.score == 16 * (2600 + 16 * 200) && g.lives == 4);
            g.beastly = true;
            g.start();
            g.addScore(150);
            CHECK(g.score == 300 && std::abs(g.shotSpeed() - .65625f) < .00001f);
            g.score = 19990;
            g.addScore(10);
            CHECK(g.lives == 4 && g.oneUpTime > 0);
        }
        {
            Game g;
            g.mode = Mode::Victory;
            steps(g, 4);
            CHECK(g.effects.size() == 3);
            CHECK(g.effects[0].kind == EffectKind::Firework && g.effects[0].duration == 80);
            CHECK(g.effects[0].color == 0 && g.effects[1].color == 136 &&
                  g.effects[2].color == 240);
            g.mode = Mode::Menu;
            auto start = g.effects[0];
            steps(g, 2);
            CHECK(g.effects[0].pos.y > start.pos.y && g.effects[0].velocity.y < start.velocity.y);
            steps(g, 160);
            auto position = g.effects[0].pos;
            steps(g, 2);
            CHECK(g.effects[0].pos.y == position.y); // Flight stops before sphere expansion.
            steps(g, 130);
            CHECK(g.effects.empty());
            g.mode = Mode::Victory;
            steps(g, 20000);
            CHECK(g.effects.size() <= 32);
            for (const auto &fx : g.effects)
                CHECK(std::isfinite(fx.pos.x) && std::isfinite(fx.pos.y));
        }
        {
            auto g = setup();
            g.spawn(Kind::Flipper, 3, .5f);
            g.shots = {{3, .49f, .49f}};
            steps(g, 1);
            auto pup = std::find_if(g.enemies.begin(), g.enemies.end(),
                                    [](const auto &e) { return e.kind == Kind::Powerup; });
            CHECK(pup != g.enemies.end() && pup->pickupFrames == 76);
            float depth = pup->depth;
            steps(g, 150);
            pup = std::find_if(g.enemies.begin(), g.enemies.end(),
                               [](const auto &e) { return e.kind == Kind::Powerup; });
            CHECK(pup->depth == depth && pup->pickupFrames == 1);
            steps(g, 4);
            CHECK(pup->pickupFrames == 0 && pup->depth < depth);
        }
        {
            auto g = setup();
            g.addScore(20000);
            steps(g, 122);
            CHECK(g.oneUpTime > 1.4f && g.oneUpTime < 1.6f); // Graphic outlives the color holiday.
            g.startBonus(1);
            g.addScore(20000);
            CHECK(g.oneUpTime == 0);
        }
        bool rare = false, ordinary = false;
        for (uint32_t seed = 0; seed < 1000 && !(rare && ordinary); ++seed) {
            Game g(seed);
            g.start();
            g.powerupStage = 1;
            g.spawn(Kind::Powerup, g.lane, 0);
            steps(g, 2);
            if (g.earlyExit) {
                CHECK(g.score == 3000 && g.mode == Mode::Transition && g.message == "OUTTAH HERE!");
                rare = true;
            } else {
                CHECK(g.score == 2000);
                ordinary = true;
            }
        }
        CHECK(rare && ordinary);
        std::cout << "Combat phase boundaries, zap immunity/pacing, entry/countdowns, "
                     "ownership/pools, open rails, all tunnel phases and clearance passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

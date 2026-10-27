#include "campaign.hpp"
#include "game.hpp"

namespace tempest {
float Game::enemySpeed(Kind kind) const {
    int i = std::min(int(fz_stuff.size()) - 1, (level - 1) / 2);
    int fixed = kind == Kind::Spiker                             ? sz_stuff[size_t(i)]
                : kind == Kind::Fuseball || kind == Kind::Mirror ? fuz_stuff[size_t(i)]
                : kind == Kind::Tanker || kind == Kind::FuseTanker || kind == Kind::PulsarTanker
                    ? tz_stuff[size_t(i)]
                    : fz_stuff[size_t(i)];
    return kind == Kind::Pulsar ? .5f * 60 / 160 : float(fixed) / 65536 * 60 / 160;
}
void Game::beginFuseCrossing(Enemy &e, int direction) {
    if (direction < 0 && e.lane == 0 && !web.closed)
        return; // set_fuseleft waits at an unconnected left edge.
    e.direction = direction;
    e.travelFrom = e.lane;
    e.travelTo = web.closed ? boundedLane(e.lane + direction) : e.lane + direction;
    if (direction < 0)
        e.lane = boundedLane(e.travelTo); // Right crossings change lane only on arrival.
    e.crossSteps = 15;
    e.fuseMotion = FuseMotion::Crossing;
}
void Game::tickSpecialEnemy(Enemy &e, float dt) {
    e.aiClock += double(dt) * 60;
    while (e.aiClock >= 1 - .000001) {
        e.aiClock -= 1;
        if (e.kind == Kind::Spiker) {
            float speed = enemySpeed(e.kind) / 60;
            if (e.spikerMotion == SpikerMotion::Seeking) {
                std::vector<int> free;
                int shortest = 0;
                for (int i = 0; i < web.lanes(); ++i) {
                    if (!hasSpike(i))
                        free.push_back(i);
                    if (spikes[size_t(i)] >= spikes[size_t(shortest)])
                        shortest = i;
                }
                e.lane = free.empty() ? shortest : free[size_t(int(randf(0, float(free.size()))))];
                if (!hasSpike(e.lane)) {
                    spikes[size_t(e.lane)] = 1 - 30.f / 160;
                    spikePresent[size_t(e.lane)] = true;
                    superSpikes[size_t(e.lane)] = randf(0, 256) < 2 * std::min(level - 1, 127);
                }
                e.buildFrames = 50;
                e.spikerMotion = SpikerMotion::Climbing;
            } else if (e.spikerMotion == SpikerMotion::Climbing) {
                e.depth -= speed;
                if (e.depth < 10.f / 160) {
                    e.spikerMotion = SpikerMotion::Descending;
                } else if (e.depth < spikes[size_t(e.lane)]) {
                    auto &tip = spikes[size_t(e.lane)];
                    if (tip - speed > 10.f / 160)
                        tip -= speed;
                    if (--e.buildFrames < 0)
                        e.spikerMotion = SpikerMotion::Descending;
                }
            } else {
                e.depth += speed;
                if (e.depth >= 1) {
                    e.depth = 1;
                    e.spikerMotion = SpikerMotion::Seeking;
                }
            }
        } else if (e.kind == Kind::Fuseball) {
            if (e.fuseMotion == FuseMotion::Climbing) {
                if (e.depth >= 0)
                    e.depth -= enemySpeed(e.kind) / 60;
                e.rim = e.depth <= 0;
                if (--e.riseFrames < 0) {
                    e.riseFrames = 48;
                    int delta = e.lane - lane;
                    int direction = delta > 0 ? -1 : 1;
                    if (web.closed && std::abs(delta) >= web.lanes() / 2)
                        direction = -direction;
                    beginFuseCrossing(e, direction);
                }
            } else if (--e.crossFrames < 0) {
                e.crossFrames = 1;
                if (--e.crossSteps < 0) {
                    if (e.direction > 0)
                        e.lane = boundedLane(e.travelTo);
                    e.fuseMotion = FuseMotion::Climbing;
                }
            }
        } else if (e.kind == Kind::Adroid) {
            if (e.droidMotion == DroidMotion::Climbing) {
                e.depth -= enemySpeed(Kind::Flipper) * 2 / 60;
                if (e.depth < -15.f / 160) {
                    e.rim = true;
                    e.droidMotion = DroidMotion::Starting;
                }
            } else if (e.droidMotion == DroidMotion::Starting) {
                int destination = boundedLane(e.lane + e.direction);
                if (destination == e.lane) {
                    e.direction = -e.direction;
                    destination = boundedLane(e.lane + e.direction);
                }
                e.travelFrom = e.lane;
                e.travelTo = destination;
                e.droidSteps = 16;
                e.droidMotion = DroidMotion::Crossing;
            } else if (e.droidMotion == DroidMotion::Crossing) {
                if (--e.droidFrames < 0) {
                    e.droidFrames = 2;
                    if (e.droidSteps == 8)
                        e.lane = e.travelTo; // gmove changes collision lane halfway across.
                    if (--e.droidSteps == 0)
                        e.droidMotion = DroidMotion::Zapping;
                }
            } else if (--e.zapFrames == 0) {
                e.zapFrames = 32; // zappage uses bne, unlike the signed motion counters.
                e.droidMotion = DroidMotion::Starting;
            }
        }
    }
}
void Game::tryAlienFire(Enemy &e, float dt, std::vector<Enemy> &children) {
    // alienfire counts eligible object updates, sharing one counter among aliens.
    e.fireClock += double(dt) * 60;
    while (e.fireClock >= 1 - .000001) {
        e.fireClock -= 1;
        if (--alienFireCounter >= 0)
            continue;
        alienFireCounter = 112;
        int capacity = 1 + std::min(3, (level - 1) / 8);
        auto bullet = [](const auto &a) { return !a.dead && a.kind == Kind::EnemyShot; };
        auto used = std::count_if(enemies.begin(), enemies.end(), bullet) +
                    std::count_if(children.begin(), children.end(), bullet);
        if (e.depth > 60.f / 160 && used < capacity)
            children.push_back(makeEnemy(Kind::EnemyShot, e.lane, e.depth, 1));
    }
}
} // namespace tempest

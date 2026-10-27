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
    int rail = e.rail >= 0 ? e.rail : e.lane;
    if ((direction < 0 && rail == 0 && !web.closed) ||
        (direction > 0 && rail == web.lanes() && !web.closed))
        return; // set_fuseleft waits at an unconnected left edge.
    e.direction = direction;
    e.travelFrom = rail;
    e.travelTo = web.closed ? boundedLane(rail + direction) : rail + direction;
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
                    if (objectSlots() >= 32)
                        continue;
                    spikes[size_t(e.lane)] = 1 - 30.f / 160;
                    spikePresent[size_t(e.lane)] = true;
                    spikeSerials[size_t(e.lane)] = nextSerial++;
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
                    e.rail = e.travelTo;
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
        int capacity = beastly ? 8 : 1 + std::min(3, (level - 1) / 8);
        auto bullet = [](const auto &a) { return !a.dead && a.kind == Kind::EnemyShot; };
        auto used = std::count_if(enemies.begin(), enemies.end(), bullet) +
                    std::count_if(children.begin(), children.end(), bullet);
        if (e.depth > 60.f / 160 && used < capacity && objectSlots() + int(children.size()) < 32)
            children.push_back(makeEnemy(Kind::EnemyShot, e.lane, e.depth, 1));
    }
}
int Game::claimBulletSlot() {
    int size = bulletExpanded || droidActive ? 16 : 8;
    for (int i = 0; i < size; ++i) {
        int slot = nextBulletSlot++ & (size - 1);
        if (std::none_of(shots.begin(), shots.end(),
                         [&](const auto &s) { return !s.dead && s.tableSlot == slot; }))
            return slot;
    }
    return -1;
}
size_t Game::shotSlots(bool ally) const {
    size_t remnants = size_t(std::count_if(effects.begin(), effects.end(), [=](const auto &fx) {
        return fx.kind == EffectKind::Splatter && fx.ally == ally;
    }));
    return remnants +
           size_t(std::count_if(shots.begin(), shots.end(),
                                [=](const auto &s) { return !s.dead && s.ally == ally; })) +
           size_t(std::count_if(enemies.begin(), enemies.end(), [=](const auto &e) {
               return !e.dead && reflected(e.kind) && e.ally == ally;
           }));
}
int Game::objectSlots() const {
    int used = 0;
    for (const auto &e : enemies) {
        if (e.dead || reflected(e.kind))
            continue; // Reflected projectiles keep their owner's bullet allocation.
        used += e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker
                    ? 3
                    : 1;
    }
    for (int i = 0; i < web.lanes(); ++i)
        used += hasSpike(i);
    used += int(pending.size());
    used += int(std::count_if(effects.begin(), effects.end(), [](const auto &fx) {
        return fx.kind != EffectKind::Splatter && fx.kind != EffectKind::Score;
    }));
    return used;
}
int Game::physicalSlots() const {
    // Include reserved tanker children so score effects cannot consume their room.
    return 2 + int(droidActive) + objectSlots() + int(shots.size()) +
           int(std::count_if(enemies.begin(), enemies.end(),
                             [](const auto &e) { return !e.dead && reflected(e.kind); })) +
           int(std::count_if(effects.begin(), effects.end(), [](const auto &fx) {
               return fx.kind == EffectKind::Splatter || fx.kind == EffectKind::Score;
           }));
}
bool Game::canGenerate() const {
    int spikesUsed = 0;
    for (int i = 0; i < web.lanes(); ++i)
        spikesUsed += hasSpike(i);
    // afree starts at 32; noclog starts at 21. Spike objects reduce both.
    return 32 - objectSlots() > 21 - spikesUsed && physicalSlots() < 62;
}
void Game::startZap(bool single, bool infinite) {
    infiniteZap = infinite;
    zapLastFrame = sourceFrame;
    zapActive = true;
    zapSingle = single;

    zapTime = single ? .45f : 1.1f;
}
bool Game::zapEligible(const Enemy &e) const {
    // run_flipper and run_fuseball check _sz before their shot windows.
    // rumirr/refsht and climbing adroids use colok/mcollie, bypassing _sz.
    return !e.dead && !e.entering && e.kind != Kind::Powerup && e.kind != Kind::Mirror &&
           !reflected(e.kind) && !(zapSingle && e.kind == Kind::EnemyShot) &&
           !(e.kind == Kind::Adroid && e.droidMotion == DroidMotion::Climbing);
}
void Game::tickZap(float dt) {
    if (!zapActive)
        return;
    (void)dt;
    if ((sourceFrame & 3) != 0 || zapLastFrame == sourceFrame)
        return;
    zapLastFrame = sourceFrame;
    do {
        auto target = std::find_if(enemies.rbegin(), enemies.rend(),
                                   [&](const auto &e) { return zapEligible(e); });
        if (target == enemies.rend()) {
            zapActive = false;
            break;
        }
        destroy(*target, true);
        if (zapSingle)
            zapActive = false;
    } while (infiniteZap && zapActive);
}

void Game::tickDroid(float dt) {
    droidClock += double(dt) * 60;
    while (droidClock >= 1 - .000001) {
        droidClock -= 1;
        int logicalLane = droidSteps > 8 ? droidFrom
                          : droidSteps   ? droidTo
                                         : boundedLane(int(std::round(droidLane)));
        if (--droidFireFrames < 0) {
            droidFireFrames = 5;
            if (shotSlots(true) < 8 && physicalSlots() < 64) {
                Shot shot{logicalLane};
                shot.ally = true;
                shot.tableSlot = claimBulletSlot();
                shot.serial = nextSerial++;
                shot.bornFrame = sourceFrame;
                shot.creatorSerial = droidSerial;
                shots.push_back(shot);
            }
        }
        if (droidSteps) {
            --droidSteps;
            int direction = directionTo(droidFrom, droidTo);
            droidLane = float(droidFrom) + float(direction) * (16 - droidSteps) / 16;
            if (web.closed)
                droidLane = std::fmod(droidLane + web.lanes(), float(web.lanes()));
        } else {
            const Enemy *target = nullptr;
            for (const auto &e : enemies)
                if (!e.dead && hostile(e.kind) && (!target || e.depth <= target->depth))
                    target = &e;
            droidFrom = logicalLane;
            int direction = directionTo(logicalLane, target ? target->lane : 0);
            if (web.closed && target && std::abs(target->lane - logicalLane) == web.lanes() / 2)
                direction = -direction;
            if (!direction)
                direction = 1; // lor chooses right when target and current lane coincide.
            droidTo = boundedLane(logicalLane + direction);
            droidSteps = 16;
        }
    }
}
void Game::animateClearance(float dt) {
    tickZap(dt);
    for (auto &e : enemies) {
        if (e.dead || e.kind == Kind::Powerup)
            continue;
        e.age += dt;
        if (flipperBody(e)) {
            if (e.motion == FlipperMotion::Turning) {
                e.turnAngle += dt * rospeeds[1] * 60 * (e.kind == Kind::SuperFlipper ? 2 : 1);
                if (e.turnAngle >= e.turnArc) {
                    e.turnAngle = e.turnArc;
                    e.motion = FlipperMotion::Stopped;
                    e.stopTime =
                        beastly ? 0
                                : float(pauses[size_t(crossdels[size_t(rospeeds[1])])] + 1) / 60;
                }
            } else if ((e.stopTime -= dt) <= 0)
                startFlipperTurn(e);
        } else if (e.kind == Kind::Fuseball || e.kind == Kind::Adroid || e.kind == Kind::Spiker)
            tickSpecialEnemy(e, dt);
        else if (e.kind == Kind::PulsarSpark) {
            e.sparkTime -= dt;
            while (e.sparkTime <= 0) {
                e.sparkTime += 5.f / 60;
                if (!web.closed &&
                    (e.lane + e.direction < 0 || e.lane + e.direction >= web.lanes()))
                    e.direction = -e.direction;
                e.lane = boundedLane(e.lane + e.direction);
            }
        } else if (reflected(e.kind) || e.kind == Kind::EnemyShot) {
            e.depth -= dt * (e.kind == Kind::EnemyShot
                                 ? .5625f
                                 : shotSpeed() * (e.kind == Kind::BeastShot ? .5f : 1));
            if (e.depth < -29.f / 160)
                e.dead = true;
        }
    }
    for (auto &shot : shots) {
        shot.previous = shot.depth;
        shot.depth += dt * shotSpeed();
    }
    shots.erase(std::remove_if(shots.begin(), shots.end(),
                               [](const auto &s) { return s.dead || s.depth >= 1; }),
                shots.end());
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(), [](const auto &e) { return e.dead; }),
        enemies.end());
}
} // namespace tempest

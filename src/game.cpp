#include "game.hpp"
#include "campaign.hpp"
#include "webs.hpp"
#include <iterator>
#include <queue>
#include <tuple>

namespace tempest {
float Game::randf(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(random);
}
static Web makeWeb(int level) {
    const auto &raw = rawWebs[webOrder[16 + (std::max(1, level) - 1) % 32]];
    Web web{raw.name, {}, raw.closed, raw.start, raw.orientations};
    float minX = 100, maxX = -100, minY = 100, maxY = -100;
    for (size_t i = 0; i < raw.xy.size(); i += 2) {
        minX = std::min(minX, raw.xy[i]);
        maxX = std::max(maxX, raw.xy[i]);
        minY = std::min(minY, raw.xy[i + 1]);
        maxY = std::max(maxY, raw.xy[i + 1]);
    }
    float scale = 4.2f / std::max(maxX - minX, maxY - minY);
    for (size_t i = 0; i < raw.xy.size(); i += 2)
        web.points.push_back(
            {(raw.xy[i] - (minX + maxX) * .5f) * scale,
             maxY - minY < .01f ? -1.5f : -(raw.xy[i + 1] - (minY + maxY) * .5f) * scale, 0});
    return web;
}
Game::Game(uint32_t seed) : random(seed) {
    loadWeb();
}
void Game::loadWeb() {
    level = std::clamp(level, 1, 100);
    web = makeWeb(level);
    targetLevel = std::min(99, level + 1);
    auto next = makeWeb(targetLevel);
    int first = std::clamp(next.start, 0, next.lanes() - 1);
    nextSpawn =
        (next.points[size_t(first)] + next.points[size_t(first + 1) % next.points.size()]) * .5f;
    lane = std::clamp(web.start, 0, web.lanes() - 1);
    visualLane = float(lane);
    wheelRemainder = 0;
    transitionOffset = {};
    waveTime = rechargeDelay = 0;
    fireFrames = beastly ? 6 : 3;
    bulletExpanded = false;
    nextBulletSlot = 0;
    const auto &wave = campaignWaves[size_t(level - 1)];
    waveSize = wave.count;
    enemiesRemaining = waveSize;
    lastPulse = false;
    generationClock = droidClock = 0;
    zapActive = zapSingle = earlyExit = false;
    droidSteps = 0;
    droidFireFrames = 5;
    spawnTimers.clear();
    for (auto rule : wave.spawns)
        spawnTimers.push_back(rule.periodTicks / 2);
    enemies.clear();
    pending.clear();
    shots.clear();
    effects.clear();
    spikes.assign(size_t(web.lanes()), 1.f);
    spikeSerials.assign(size_t(web.lanes()), 0);
    spikePresent.assign(size_t(web.lanes()), false);
    superSpikes.assign(size_t(web.lanes()), false);
    zaps = 2;
    droidActive = earlyDroid = false;
    pendingBonus = -1;
    boost = false;
    jumpEnabled = false;
    powerupStage = 0;
    killsToPowerup = 1;
    kills = 0;
    // Do not reset an airborne jump or its cooldown: the rig must cross webs continuously.
    unlockedLevel = std::max(unlockedLevel, level);
    runHighest = std::max(runHighest, level);
    selectedLevel = level;
}
void Game::start(int startingLevel, bool awardStart) {
    finishScoreEntry();
    level = std::clamp(startingLevel, 1, 100);
    runHighest = level;
    frontAge = 0;
    frontTicks = 0;
    attractGame.reset();
    alienFireCounter = 112;
    // frames is global in the source: title and prior games retain its phase.
    nextSerial = 1;
    rotarySensitivity = 0x2000;
    rotaryMoved = false;
    padHorizontalVelocity = 0;
    score = 0;
    lives = 3;
    kills = 0;
    nextLife = 20000;
    invulnerable = 2;
    flash = 0;
    zapTime = 0;
    jumpTime = 0;
    jumpCooldown = 0;
    deathTime = oneUpTime = 0;
    bonusTokens = 0;
    bonusesPlayed = 0;
    bonusRuns.fill(0);
    tunnelPatterns = {TunnelPattern{128, 22, 64, 28, 0x2000, 0x100, 0x800, 0x60},
                      TunnelPattern{80, 9, 112, 15, 0x6000, 0x280, 0xc00, 0xc0}};
    tunnelPatternCountdown = 300;
    droidPending = earlyDroid = false;
    pendingBonus = -1;
    returnTime = rechargeDelay = 0;
    droidActive = false;
    boost = false;
    jumpEnabled = false;
    powerupStage = 0;
    killsToPowerup = 1;
    particles.clear();
    sounds.clear();
    loadWeb();
    mode = Mode::Playing;
    if (awardStart && level > 1) {
        score = (level - 1) * (2600 + (level - 1) * 200) * (beastly ? 2 : 1);
        nextLife = (score / 20000 + 1) * 20000;
        ++lives;
        oneUpTime = 151.f / 60;
        sounds.push_back(Sound::OneUp);
        best = std::max(best, score);
    }
    sounds.push_back(Sound::Wave);
    announce("SUPERZAPPER RECHARGE");
}
void Game::announce(const std::string &text, float seconds) {
    showMessage(text, std::abs(seconds - 1.6f) < .0001f ? 100 : int(std::round(seconds * 60)));
}
void Game::showMessage(const std::string &text, int hold) {
    message = text;
    messageFrame = 0;
    messageHold = std::max(0, hold);
    messageScaleX = messageScaleY = 1;
    messageTime = messageDuration = float(messageHold + 52) / 60;
}
int Game::boundedLane(int n) const {
    int count = web.lanes();
    return web.closed ? (n % count + count) % count : std::clamp(n, 0, count - 1);
}
int Game::directionTo(int from, int to) const {
    int d = to - from;
    if (web.closed) {
        if (d > web.lanes() / 2)
            d -= web.lanes();
        if (d < -web.lanes() / 2)
            d += web.lanes();
    }
    return d == 0 ? 0 : d > 0 ? 1 : -1;
}
void Game::steer(float notches) {
    if (enteringScore) {
        wheelRemainder += std::clamp(notches, -32.f, 32.f);
        int steps = int(wheelRemainder);
        wheelRemainder -= steps;
        scoreChoice = ((scoreChoice + steps) % 32 + 32) % 32;
        return;
    }
    if (deathTime > 0)
        return;
    if (mode == Mode::Menu) {
        if (frontPage == FrontPage::Keys) {
            wheelRemainder += notches;
            int steps = int(wheelRemainder);
            wheelRemainder -= steps;
            keyCursor = std::clamp(keyCursor + steps, 0, 3);
        } else {
            frontAge = 0;
            frontTicks = 0;
            frontPage = FrontPage::Title;
        }
        return;
    }
    if (mode == Mode::Bonus) {
        if (bonusFinished)
            return;
        if (bonusType == 1) {
            tunnelVelocity = std::clamp(tunnelVelocity + notches * tunnelWheelGain, -6.f, 6.f);
            return;
        }
        bonusHorizontalVelocity += notches * ringWheelGain; // m7run: roach += rot_cum; iii.

        return;
    }
    if (mode != Mode::Playing && mode != Mode::Transition)
        return;
    wheelRemainder += std::clamp(notches, -32.f, 32.f);
    int steps = int(wheelRemainder);
    wheelRemainder -= steps;
    lane = boundedLane(lane + steps);
}
void Game::jump() {
    if (deathTime <= 0 && (mode == Mode::Playing || mode == Mode::Transition) && jumpEnabled &&
        jumpCooldown <= 0) {
        jumpTime = .85f;
        jumpCooldown = 3.f;
        sounds.push_back(Sound::Jump);
    }
}
void Game::pause() {
    if (active()) {
        pausedFrom = mode;
        mode = Mode::Paused;
    } else if (mode == Mode::Paused)
        mode = pausedFrom;
}
Vec3 Game::edge(int index, float depth) const {
    auto p = web.points[size_t((index % int(web.points.size()) + int(web.points.size())) %
                               int(web.points.size()))];
    float scale = 1 - depth * .46f;
    return {p.x * scale, p.y * scale, -3.8f - depth * 9};
}
Vec3 Game::position(float lp, float depth) const {
    int l = int(std::floor(lp));
    float u = lp - std::floor(lp);
    auto center = [&](int n) {
        n = boundedLane(n);
        return (edge(n, depth) + edge(n + 1, depth)) * .5f;
    };
    return center(l) * (1 - u) + center(l + 1) * u;
}
bool Game::flipperKind(Kind kind) {
    return kind == Kind::Flipper || kind == Kind::SuperFlipper || kind == Kind::SuperFlipper2 ||
           kind == Kind::Beast;
}
bool Game::shotVulnerable(const Enemy &e) {
    if (e.kind == Kind::MirrorShot)
        return false;
    if (e.kind == Kind::Fuseball)
        return e.fuseMotion == FuseMotion::Crossing && e.crossSteps >= 4 && e.crossSteps <= 12;
    return !flipperBody(e) || e.motion != FlipperMotion::Turning ||
           std::abs(std::remainder(e.turnArc - e.turnAngle, 256.f)) <= 52.f;
}
void Game::startFlipperTurn(Enemy &e) {
    int delta = e.lane - lane;
    if (delta == 0 && e.scarper == 0) {
        e.motion = FlipperMotion::Stopped;
        return;
    }
    int direction = delta > 0 ? -1 : 1;
    // _fset's exact half-web tie goes the other way, including circular wrap.
    if (web.closed && std::abs(delta) >= web.lanes() / 2)
        direction = -direction;
    if (e.scarper != 0) {
        direction = e.scarper > 0 ? 1 : -1;
        e.scarper -= direction;
        e.emerging = false; // A new directed turn replaces the initial tanker-entry flag.
    } else if (e.runningBelowRim)
        direction = -direction;
    beginFlipperTurn(e, direction);
}
void Game::beginFlipperTurn(Enemy &e, int direction) {
    int destination = boundedLane(e.lane + direction);
    if (destination == e.lane) {
        direction = -direction;
        destination = boundedLane(e.lane + direction);
    }
    e.turnFrom = e.lane;
    e.lane = destination; // Source collision lane is the destination during a turn.
    e.direction = direction;
    int difference = web.orientations[size_t(destination)] - web.orientations[size_t(e.turnFrom)];
    int arc = (128 + direction * difference) % 256;
    if (arc <= 0)
        arc += 256;
    e.turnArc = float(arc);
    e.turnAngle = 0;
    e.motion = FlipperMotion::Turning;
}
float Game::flipRotation(const Enemy &e) const {
    if (!flipperBody(e) || e.motion != FlipperMotion::Turning || e.turnFrom < 0)
        return 0;
    int pivot = e.turnFrom + (e.direction > 0 ? 1 : 0);
    Vec3 a = position(float(e.turnFrom), e.depth) - edge(pivot, e.depth),
         b = position(float(e.lane), e.depth) - edge(pivot, e.depth);
    float angle = std::atan2(a.x * b.y - a.y * b.x, a.x * b.x + a.y * b.y);
    constexpr float tau = 6.283185307f;
    if (e.direction > 0 && angle >= 0)
        angle -= tau;
    if (e.direction < 0 && angle <= 0)
        angle += tau;
    return angle * std::clamp(e.turnAngle / e.turnArc, 0.f, 1.f);
}
Vec3 Game::enemyPosition(const Enemy &e) const {
    if (flipperBody(e) && e.motion == FlipperMotion::Turning && e.turnFrom >= 0) {
        int pivotLane = e.turnFrom + (e.direction > 0 ? 1 : 0);
        Vec3 pivot = edge(pivotLane, e.depth), a = position(float(e.turnFrom), e.depth) - pivot,
             b = position(float(e.lane), e.depth) - pivot;
        float angle = flipRotation(e), radius = std::hypot(a.x, a.y),
              u = std::clamp(e.turnAngle / e.turnArc, 0.f, 1.f),
              scale = (radius * (1 - u) + std::hypot(b.x, b.y) * u) / std::max(.0001f, radius);
        return pivot + Vec3{(a.x * std::cos(angle) - a.y * std::sin(angle)) * scale,
                            (a.x * std::sin(angle) + a.y * std::cos(angle)) * scale, 0};
    }
    if (e.kind == Kind::Fuseball) {
        if (e.fuseMotion == FuseMotion::Crossing && e.travelFrom >= 0) {
            float t = float(15 - e.crossSteps) / 16;
            return edge(e.travelFrom, e.depth) * (1 - t) + edge(e.travelTo, e.depth) * t;
        }
        return edge(e.rail >= 0 ? e.rail : e.lane, e.depth);
    }
    if (e.kind == Kind::Adroid && e.droidMotion == DroidMotion::Crossing && e.travelFrom >= 0) {
        float t = float(16 - e.droidSteps) / 16;
        return position(float(e.travelFrom), e.depth) * (1 - t) +
               position(float(e.travelTo), e.depth) * t;
    }
    return position(float(e.lane), e.depth);
}
Vec3 Game::enemyTangent(const Enemy &e) const {
    int visualLane = flipperBody(e) && e.motion == FlipperMotion::Turning && e.turnFrom >= 0
                         ? e.turnFrom
                         : e.lane;
    Vec3 t = edge(visualLane + 1, e.depth) - edge(visualLane, e.depth);
    float angle = flipRotation(e), length = std::max(.0001f, std::hypot(t.x, t.y));
    return {(t.x * std::cos(angle) - t.y * std::sin(angle)) / length,
            (t.x * std::sin(angle) + t.y * std::cos(angle)) / length, 0};
}
Vec3 Game::shipPosition() const {
    if (deathTime > 0) {
        float t = deathProgress(), blend = t * t * (3 - 2 * t);
        return deathShip * (1 - blend) + respawnShip * blend +
               Vec3{0, 0, std::sin(t * 3.14159265f) * .8f};
    }
    if (mode == Mode::GameOver)
        return deathShip;
    if (isBonusReturn()) {
        float t = std::clamp(1 - returnTime / 1.5f, 0.f, 1.f);
        return position(visualLane, 0) + returnOffset * (1 - t * t * (3 - 2 * t));
    }
    if (isBonus()) {
        float t = std::clamp(bonusTime / 1.5f, 0.f, 1.f);
        float blend = t * t * (3 - 2 * t);
        return bonusCursor() + Vec3{0, 0, -3.8f} + bonusEntryOffset * (1 - blend);
    }
    float distance = warpDistance();
    Vec3 p = position(visualLane, distance / 9);
    float blend = std::clamp((distance - 9) / (warpTravel - 9), 0.f, 1.f);
    blend = blend * blend * (3 - 2 * blend);
    p.x = p.x * (1 - blend) + nextSpawn.x * blend;
    p.y = p.y * (1 - blend) + nextSpawn.y * blend;
    p.z += jumpLift();
    float t = transitionDepth();
    p = p + transitionOffset * (1 - t * t * (3 - 2 * t));
    return p;
}
void Game::burst(Vec3 p, int color, int count) {
    count = std::min(count, 6000 - int(particles.size()));
    for (int i = 0; i < count; i++) {
        float life = randf(.35f, 1.25f);
        particles.push_back({p,
                             {randf(-2.5f, 2.5f), randf(-2.5f, 2.5f), randf(-2, 2)},
                             life,
                             life,
                             i % 4 ? 4 + int(randf(0, 31)) : color});
    }
}
void Game::addScore(int points) {
    score += std::max(0, points) * (beastly ? 2 : 1);
    while (score >= nextLife) {
        lives++;
        nextLife += 20000;
        sounds.push_back(Sound::OneUp);
        oneUpTime = isBonus() && bonusType == 1 ? 0.f : 151.f / 60;
    }
    best = std::max(best, score);
}
bool Game::hostile(Kind kind) {
    return kind != Kind::Powerup && kind != Kind::EnemyShot && !reflected(kind);
}
Kind Game::kindForSource(int type) {
    constexpr Kind kinds[] = {Kind::Flipper,      Kind::Tanker,       Kind::Spiker,
                              Kind::Fuseball,     Kind::Pulsar,       Kind::FuseTanker,
                              Kind::PulsarTanker, Kind::SuperFlipper, Kind::Mirror,
                              Kind::Adroid,       Kind::Beast,        Kind::SuperFlipper2};
    return kinds[std::clamp(type, 0, 11)];
}
Enemy Game::makeEnemy(Kind kind, int lane, float depth, int direction) {
    Enemy e{kind, lane, depth};
    e.direction = direction;
    e.hp = kind == Kind::Mirror ? 5 : kind == Kind::Beast ? 3 : 1;
    if (kind == Kind::SuperFlipper2) {
        e.runningBelowRim = depth > 0;
        e.motion = FlipperMotion::Stopped;
    }
    return e;
}
void Game::openTanker(Enemy &e, std::vector<Enemy> &children) {
    Kind contents = e.kind == Kind::FuseTanker     ? Kind::Fuseball
                    : e.kind == Kind::PulsarTanker ? Kind::Pulsar
                                                   : Kind::Flipper;
    for (int direction : {-1, 1}) {
        auto child = makeEnemy(contents, e.lane, std::max(0.f, e.depth), direction);
        if (contents == Kind::Flipper || contents == Kind::Pulsar) {
            child.emerging = true;
            child.rim = child.depth == 0;
            beginFlipperTurn(child, direction);
            if (contents == Kind::Flipper && randf(0, 256) < 4 * std::min(level - 1, 63)) {
                child.kind = randf(0, 256) < 2 * std::min(level - 1, 127) ? Kind::SuperFlipper2
                                                                          : Kind::SuperFlipper;
                child.runningBelowRim = child.kind == Kind::SuperFlipper2 && child.depth > 0;
                child.scarper = direction * (1 + int(randf(0, 4)));
            }
        } else {
            // openfuseballs starts the right child on the parent's right rail.
            child.lane = direction > 0 ? boundedLane(e.lane + 1) : e.lane;
            child.rail =
                direction > 0 ? (web.closed ? boundedLane(e.lane + 1) : e.lane + 1) : e.lane;
            beginFuseCrossing(child, direction);
        }
        children.push_back(child);
    }
    destroy(e);
}
void Game::spawn(Kind kind, int spawnLane, float depth, bool entering) {
    if (physicalSlots() >= 64)
        return;
    if (objectSlots() +
            ((kind == Kind::Tanker || kind == Kind::FuseTanker || kind == Kind::PulsarTanker) ? 3
                                                                                              : 1) >
        32)
        return;
    if (kind == Kind::Spiker && std::any_of(enemies.begin(), enemies.end(), [](const auto &e) {
            return !e.dead && e.kind == Kind::Spiker;
        }))
        return; // max_spikers=0 permits one, then becomes -1.
    bool tanker = kind == Kind::Tanker || kind == Kind::FuseTanker || kind == Kind::PulsarTanker;
    int l = spawnLane < 0 ? (tanker ? 1 + int(randf(0, float(web.lanes() - 2)))
                                    : int(randf(0, float(web.lanes()) - .001f)))
                          : boundedLane(spawnLane);
    auto enemy = makeEnemy(kind, l, depth, randf(0, 1) > .5f ? 1 : -1);
    enemy.serial = nextSerial++;
    if (entering && kind != Kind::Spiker) {
        enemy.entering = true;
        enemy.buildFrames = 150;
        enemy.depth += 300.f / 160;
    }
    enemies.push_back(enemy);
}
void Game::destroy(Enemy &e, bool zap) {
    if (e.dead)
        return;
    e.dead = true;
    burst(enemyPosition(e), int(e.kind) % 4,
          (e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker)
              ? 90
              : 48);
    if (e.kind == Kind::EnemyShot || reflected(e.kind))
        return;
    int points = 150;
    switch (e.kind) {
    case Kind::Tanker:
    case Kind::FuseTanker:
    case Kind::PulsarTanker:
        points = 100;
        break;
    case Kind::Fuseball:
    case Kind::Mirror:
    case Kind::Adroid: {
        constexpr int bonuses[] = {250, 500, 750, 1};
        points = bonuses[int(randf(0, 4))]; // blowmeaway / xbonx.
        break;
    }
    case Kind::Pulsar:
        points = 200;
        break;
    default:
        break;
    }
    if (e.emerging)
        points = 150; // Tanker children still run the flipper handler.
    addScore(points);
    bool xbon = !e.emerging &&
                (e.kind == Kind::Fuseball || e.kind == Kind::Mirror || e.kind == Kind::Adroid);
    if (xbon && physicalSlots() < 64)
        effects.push_back({EffectKind::Score, enemyPosition(e), {}, 0, 31, points});
    bool pickup = false;
    if (!zap) {
        sounds.push_back(e.kind == Kind::Tanker || e.kind == Kind::FuseTanker ||
                                 e.kind == Kind::PulsarTanker
                             ? Sound::TankerOpen
                         : xbon ? Sound::AlienBonus
                                : Sound::Kill);
        kills++;
        if (--killsToPowerup <= 0) {
            auto powerup = makeEnemy(Kind::Powerup, e.lane, std::clamp(e.depth, .1f, .95f), 1);
            powerup.pickupFrames = 76; // 300, subtract four through signed-negative expiry.
            pending.push_back(powerup);
            sounds.push_back(Sound::PowerupSpawn);
            pickup = true;
            int index = std::min(int(pup_stuff.size()) - 1, (level - 1) / 4);
            killsToPowerup = (pup_stuff[size_t(index)] & 255) + 1;
        }
    }
    if (!pickup)
        effects.push_back(
            {e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker
                 ? EffectKind::Sphere
                 : EffectKind::Rings,
             enemyPosition(e),
             {},
             0,
             e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker
                 ? 31
                 : 50,
             int(randf(0, 16)) * 16});
}
void Game::hit(bool snatch) {
    if (invulnerable > 0 || jumpTime > 0 || deathTime > 0)
        return;
    deathShip = shipPosition();
    respawnShip = position(float(std::clamp(web.start, 0, web.lanes() - 1)), 0);
    snatched = snatch;
    lives--;
    invulnerable = 2.4f;
    flash = .55f;
    if (!snatched)
        burst(deathShip, 3, 220);
    announce(snatched ? "GOT YOU" : "OUCH");
    sounds.push_back(snatched ? Sound::Snatch : Sound::Hit);
    droidActive = false;
    boost = false;
    jumpEnabled = false;
    powerupStage = 0;
    killsToPowerup = 1;
    fireHeld = false;
    deathTime = deathDuration;
    if (lives <= 0) {
        respawnShip = deathShip + Vec3{0, 0, .8f};
        mode = Mode::GameOver;
        best = std::max(best, score);
        frontPage = FrontPage::GameOver;
        frontAge = 0;
        frontTicks = 0;
    }
}
void Game::zap() {
    if (mode != Mode::Playing || zaps <= 0 || deathTime > 0)
        return;
    bool full = zaps == 2;
    zaps--;
    flash = full ? .8f : .3f;
    zapTime = full ? 1.1f : .45f;
    announce(full ? "EAT ELECTRIC" : "LAST CHANCE");
    sounds.push_back(Sound::Zap);
    startZap(!full);
}
void Game::collectPowerup() {
    // run_pup: a pickup caught while zooming schedules an early droid on
    // the next wave; it does not turn the ordinary droid into a persistent upgrade.
    if (mode == Mode::Transition) {
        if (!droidPending && !earlyDroid) {
            droidPending = true;
            announce("DROID NEXT WEB");
            sounds.push_back(Sound::Yes);
        } else {
            powerupStage = 2;
            addScore(2000);
            announce("EXCELLENT");
            sounds.push_back(Sound::Excellent);
        }
        return;
    }
    if (droidPending) {
        droidPending = false;
        earlyDroid = true;
        droidActive = true;
        droidSerial = nextSerial++;
        droidLane = visualLane;
        announce("A.I. DROID");
        sounds.push_back(Sound::Pickup);
        return;
    }
    if (powerupStage == 4 && droidActive)
        powerupStage = 5;
    switch (powerupStage) {
    case 0:
        boost = true;
        fireFrames = beastly ? 4 : 2;
        announce("PARTICLE LASER");
        break;
    case 2:
        jumpEnabled = true;
        announce("JUMP ENABLED");
        break;
    case 4:
        droidActive = true;
        droidSerial = nextSerial++;
        droidLane = visualLane;
        announce("A.I. DROID");
        break;
    case 5:
        bonusTokens++;
        sounds.push_back(Sound::BonusToken);
        announce("BONUS TOKEN " + std::to_string(bonusTokens) + " / 3");
        zapTime = 1.1f;
        zaps = std::max(zaps, 1);
        startZap();
        break;
    default:
        surprisePowerup();
        break;
    }
    powerupStage = std::min(7, powerupStage + 1);
    sounds.push_back(Sound::Pickup);
    burst(shipPosition(), 3, 100);
}
int Game::pulsePhase() const {
    int index = std::min(int(pudels.size()) - 1, (level - 1) / 4);
    int framesPerPhase = (pudels[size_t(index)] & 255) + 1;
    return (int(std::floor(double(waveTime) * 60 + .0001)) / framesPerPhase) & 15;
}
bool Game::pulseActive() const {
    return pulsePhase() == 7;
}
void Game::beginTransition(int destination) {
    // Carry the actual outgoing camera/ship location into the tunnel, including
    // bonus coordinates, instead of snapping back to the old web's rim.
    transitionOffset = shipPosition() - (position(visualLane, 0) + Vec3{0, 0, jumpLift()});
    targetLevel = std::clamp(destination, 1, 100);
    auto next = makeWeb(targetLevel);
    int first = std::clamp(next.start, 0, next.lanes() - 1);
    nextSpawn =
        (next.points[size_t(first)] + next.points[size_t(first + 1) % next.points.size()]) * .5f;
    droidActive = earlyDroid = false; // rundroid expires collected droids at wave_tim == -2.
    mode = Mode::Transition;
    transitionTimer = 2.4f;
    enginePitch = 10;
    sounds.push_back(Sound::ClearanceEngine);
    announce(clearanceWarning() ? "AVOID THE SPIKES" : "", 250.f / 60);
}
void Game::finishCampaign() {
    mode = Mode::Victory;
    unlockedLevel = std::max(unlockedLevel, 99);
    beastlyUnlocked = true;
    fireworkPC = 0;
    fireworkWait = 1;
    fireworkStack.clear();
    effects.clear();
    enemies.clear();
    shots.clear();
    frontPage = FrontPage::Congratulations;
    frontAge = 0;
    frontTicks = 0;
    announce(beastly ? "TOTALLY AWESOME!" : "YOU ARE A SUPERSTAR", 10);
    sounds.push_back(Sound::Excellent);
    burst(shipPosition(), 3, 800);
}
void Game::completeWave() {
    if (bonusTokens >= 3) {
        constexpr int types[] = {0, 1, 2, 1};
        pendingBonus = types[level / 16 % 4]; // cweb has advanced before sweb0.
    }
    beginTransition(level + 1);
}
void Game::startBonus(int type) {
    Vec3 entry = shipPosition();
    bonusBaseLevel = level;
    mode = Mode::Bonus;
    bonusType = std::clamp(type, 0, 2);
    bonusCourse = bonusRuns[size_t(bonusType)]++ % 8;
    bonusesPlayed++;
    bonusTime = 0;
    bonusDistance = bonusEffectTime = 0;
    bonusControlClock = 0;
    bonusControlStarted = false;
    bonusHorizontalVelocity = 0;
    bonusVerticalVelocity = bonusType == 1 ? 0 : -bonusVerticalMax;
    bonusBoosts = bonusLastScore = 0;
    bonusVelocityFixed =
        (bonusType == 2 ? 0xa000 : 0x8000) + ((bonusRuns[size_t(bonusType)] - 1) * 512 & 65535);
    bonusGroundFixed = 0;
    bonusSpeed = float(bonusVelocityFixed) * 60 / 65536;
    bonusGate = 0;
    bonusHits = 0;
    bonusX = bonusAngle = 0;
    bonusY = bonusType == 1 ? 0.f : -.25f / 32; // Source Y points down; GL Y points up.
    droidActive = false;
    if (bonusType == 1)
        startTunnel();
    bonusEntryOffset = entry - (bonusCursor() + Vec3{0, 0, -3.8f});
    bonusClock = 0;
    gateTimer = gateNext = 0;
    if (bonusType != 1)
        planBonusGates();
    bonusResultTime = 0;
    bonusFinished = bonusWon = false;
    bonusUp = bonusDown = false;
    enemies.clear();
    pending.clear();
    shots.clear();
    announce(bonusType == 1 ? "TUNNEL BONUS" : "FOLLOW THE RINGS", 2.5f);
}
int Game::bonusGateCount() const {
    return bonusType == 1 ? 64 : int(ringCourses[size_t(bonusCourse)].size()) - 1;
}
float Game::bonusGateInterval() const {
    return (bonusGateDistance(bonusGate + 1) - bonusGateDistance(bonusGate)) / bonusSpeed;
}
int Game::bonusGateDelay(int gate) const {
    if (bonusType == 1)
        return 0;
    if (bonusType == 2)
        return 8; // The original psycho mode overrides the packed delay.
    const auto &course = ringCourses[size_t(bonusCourse)];
    return int(course[size_t(std::clamp(gate, 0, int(course.size()) - 2))] >> 24);
}
int Game::bonusGateEffect(int gate) const {
    if (bonusType == 1)
        return gate == 63 ? 4 : 0;
    const auto &course = ringCourses[size_t(bonusCourse)];
    return int(course[size_t(std::clamp(gate, 0, int(course.size()) - 2))] & 255);
}
float Game::bonusGateDistance(int gate) const {
    if (bonusType == 1)
        return float((gate + 1) * 16);
    return float(gateLaunches[size_t(std::clamp(gate, 0, int(gateLaunches.size()) - 1))] +
                 ((int64_t(252) * 65536 / bonusVelocityFixed + 1) * bonusVelocityFixed)) /
           65536;
}
float Game::bonusGateZ(int gate) const {
    if (bonusType == 1)
        return -3.8f;
    auto launch = gateLaunches[size_t(std::clamp(gate, 0, int(gateLaunches.size()) - 1))];
    return -3.8f - (255 - float(bonusGroundFixed - launch) / 65536) / 32;
}
Vec3 Game::bonusTarget(int gate) const {
    if (bonusType == 1) {
        float angle = tunnelTrackAngle();
        return {std::cos(angle) * 1.4f, std::sin(angle) * 1.4f, 0};
    }
    auto &course = ringCourses[size_t(bonusCourse)];
    uint32_t packed = course[size_t(std::clamp(gate, 0, int(course.size()) - 2))];
    float x = float(int8_t((packed >> 16) & 255)) / 32.f,
          y = float(int8_t((packed >> 8) & 255)) / 32.f;
    return {bonusType == 2 ? -x : x, -y, 0};
}
float Game::bonusVerticalLimit() const {
    if (strictRotaryLimits || bonusType == 1)
        return 1.25f;
    float limit = 1.25f;
    for (int i = 0; i < bonusGateCount(); ++i)
        limit = std::max(limit, std::abs(bonusTarget(i).y) + .25f);
    return limit;
}
Vec3 Game::bonusCursor() const {
    return bonusType == 1 ? Vec3{std::cos(bonusAngle) * 1.4f, std::sin(bonusAngle) * 1.4f, 0}
                          : Vec3{bonusX, bonusY, 0};
}
void Game::tickBonus(float dt) {
    bonusTime += dt;
    if (bonusFinished) {
        if (bonusType == 1)
            tunnelResultTime += dt;
        bonusResultTime -= dt;
        if (bonusResultTime <= 0)
            returnFromBonus();
        return;
    }
    if (bonusType == 1) {
        tickTunnel(dt);
        return;
    }
    bonusClock += double(dt) * 60;
    if (bonusClock < 1 - .000001)
        return;
    bonusClock -= 1;
    dt = 1.f / 60;
    tickRotaryBonus(dt);
    bonusEffectTime = std::max(0.f, bonusEffectTime - dt);
    bonusGroundFixed += bonusVelocityFixed;
    bonusDistance = float(bonusGroundFixed) / 65536;
    advanceBonusGenerator();
    if (bonusGate < gateNext &&
        (bonusGroundFixed - gateLaunches[size_t(bonusGate)]) > int64_t(252) * 65536) {
        auto target = bonusTarget(bonusGate), cursor = bonusCursor();
        float dx =
            float(std::abs(int(std::floor(cursor.x * 32)) - int(std::round(target.x * 32)))) / 32;
        float dy =
            float(std::abs(int(std::floor(-cursor.y * 32)) - int(std::round(-target.y * 32)))) / 32;
        bool passed = bonusType == 1 ? std::hypot(target.x - cursor.x, target.y - cursor.y) < .6f
                                     : dx < .25f && dy < .25f;
        if (!passed) {
            bonusFinished = true;
            bonusResultTime = 1.6f;
            announce("MISSED - TRY AGAIN", 1.6f);
            sounds.push_back(Sound::Hit);
            return;
        }
        int effect = bonusGateEffect(bonusGate);
        // run_gate / xbon: ring accuracy selects 250 / 500 / 750 points.
        constexpr int awards[] = {250, 500, 750, 1};
        int error = std::clamp(int((dx + dy) * 32) / 4, 0, 3);
        bonusLastScore = awards[bonusType == 2 ? error : 2 - std::min(2, error)];
        if (bonusType == 1)
            bonusLastScore = 250;
        addScore(bonusLastScore);
        if (effect == 1 || effect == 2) {
            bonusVerticalVelocity = effect == 1 ? bonusVerticalMax : -bonusVerticalMax;
            announce(effect == 1 ? "UP IMPULSE" : "DOWN IMPULSE", .9f);
            bonusEffectTime = 1;
        } else if (effect == 3) {
            bonusVelocityFixed += bonusVelocityFixed >> 3;
            bonusSpeed = float(bonusVelocityFixed) * 60 / 65536;
            ++bonusBoosts;
            bonusEffectTime = 1;
            announce("SPEED BOOST", 1);
        } else if (effect == 0)
            announce(std::to_string(bonusLastScore), 1.1f);
        sounds.push_back(bonusBoosts % 2 ? Sound::GateYes : Sound::GateYesAlt);
        bonusHits++;
        bonusGate++;
        burst(cursor + Vec3{0, 0, -3.8f}, 3, 32);
        if (effect == 4 || bonusGate >= bonusGateCount()) {
            bonusWon = bonusFinished = true;
            bonusResultTime = 401.f / 60;
            if (bonusType != 1)
                addScore(20000); // m7scr success; tundraw has its own continuous scoring.
            announce(level < 90 ? "WARP 5 LEVELS" : "EXCELLENT", 401.f / 60);
            sounds.push_back(Sound::Excellent);
        }
    }
}
void Game::returnFromBonus() {
    Vec3 origin = shipPosition();
    level = std::clamp(bonusBaseLevel + (bonusWon && bonusBaseLevel < 90 ? 5 : 1), 1, 100);
    loadWeb();
    returnOffset = origin - position(visualLane, 0);
    returnTime = 1.5f;
    mode = Mode::BonusReturn;
    announce("RETURNING TO WEB", 1.5f);
}
void Game::tickPickups(float dt) {
    float shipDepth = mode == Mode::Transition ? warpDistance() / 9 : 0;
    for (auto &e : enemies) {
        if (e.dead || e.kind != Kind::Powerup)
            continue;
        if (e.pickupFrames > 0) {
            e.aiClock += double(dt) * 60;
            while (e.aiClock >= 1 - .000001 && e.pickupFrames > 0) {
                e.aiClock -= 1;
                --e.pickupFrames;
            }
            continue;
        }
        float previous = e.depth;
        e.depth -= dt * 1.5f * 60 / 160;
        if (e.lane == lane && previous >= shipDepth && e.depth <= shipDepth + 32.f / 160) {
            e.dead = true;
            effects.push_back({EffectKind::Pickup, enemyPosition(e), {}, 0, 48});
            collectPowerup();
        } else if (e.depth < 0 && mode != Mode::Transition) {
            e.dead = true;
            effects.push_back({EffectKind::Rings, enemyPosition(e)});
        }
        if (mode == Mode::Transition)
            e.age += dt;
    }
}
void Game::tick(float dt) {
    if (!std::isfinite(dt) || dt <= 0)
        return;
    while (dt > .000001f) {
        float step = std::min(dt, 1.f / 120);
        tickStep(step);
        dt -= step;
    }
}
void Game::tickStep(float dt) {
    if (mode == Mode::Paused)
        return;
    sourceClock += double(dt) * 60;
    bool sourceUpdate = sourceClock >= 1 - .000001;
    if (sourceUpdate) {
        sourceClock -= 1;
        ++sourceFrame;
    }
    time += dt;
    tickEffects(dt);
    flash = std::max(0.f, flash - dt);
    zapTime = std::max(0.f, zapTime - dt);
    if (sourceUpdate) {
        tickRotaryCalibration();
        if (mode == Mode::Transition) {
            ++enginePitch;
            engineVolumePhase = (int(30 + warpDistance() / 9 * 160) * 4) & 255;
        }
        if (messageTime > 0) {
            ++messageFrame;
            int tail = std::max(0, messageFrame - messageHold - 1);
            messageScaleX = 1;
            messageScaleY = 1 + tail * .5f;
            int shear = std::abs(int((sourceFrame * 2) & 255) - 127) - 63;
            messageShearX = float(shear) / 64;
            messageShearY = -messageShearX / 2;
            messageTime = std::max(0.f, float(messageHold + 52 - messageFrame) / 60);
        }
    }
    oneUpTime = std::max(0.f, oneUpTime - dt);
    for (auto &p : particles) {
        p.life -= dt;
        p.pos = p.pos + p.velocity * dt;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(), [](auto &p) { return p.life <= 0; }),
        particles.end());
    if (deathTime > 0) {
        deathTime = std::max(0.f, deathTime - dt);
        if (deathTime == 0) {
            if (lives > 0) {
                loadWeb();
                mode = Mode::Playing;
                invulnerable = 1.5f;
                announce("SUPERZAPPER RECHARGE");
                sounds.push_back(Sound::Wave);
            } else
                deathShip = respawnShip;
        }
        return;
    }
    float diff = float(lane) - visualLane, n = float(web.lanes());
    if (web.closed) {
        if (diff > n / 2)
            diff -= n;
        if (diff < -n / 2)
            diff += n;
    }
    visualLane += diff * std::min(1.f, dt * 24);
    if (web.closed) {
        while (visualLane < 0)
            visualLane += n;
        while (visualLane >= n)
            visualLane -= n;
    }
    if (mode == Mode::Menu || mode == Mode::GameOver) {
        tickFrontend(dt, sourceUpdate);
        return;
    }
    if (mode == Mode::Victory) {
        tickFrontend(dt, sourceUpdate);
        return;
    }
    invulnerable = std::max(0.f, invulnerable - dt);
    jumpTime = std::max(0.f, jumpTime - dt);
    jumpCooldown = std::max(0.f, jumpCooldown - dt);
    if (mode == Mode::Bonus) {
        tickBonus(dt);
        return;
    }
    if (mode == Mode::BonusReturn) {
        returnTime = std::max(0.f, returnTime - dt);
        if (returnTime == 0) {
            mode = Mode::Playing;
            invulnerable = 1;
            sounds.push_back(Sound::Wave);
            announce("SUPERZAPPER RECHARGE");
        }
        return;
    }
    if (mode == Mode::Transition) {
        animateClearance(dt);
        tickPickups(dt);
        transitionTimer -= dt;
        float depth = warpDistance() / 9;
        if (depth >= spikes[size_t(lane)] && depth <= 1 && hasSpike(lane))
            hit();
        if (mode == Mode::GameOver || deathTime > 0)
            return;
        if (transitionTimer <= 0) {
            int bonus = pendingBonus, clearedLevel = level;
            if (clearedLevel >= 99) {
                finishCampaign();
                return;
            }
            level = targetLevel;
            loadWeb();
            mode = Mode::Playing;
            if (bonus >= 0) {
                bonusTokens -= 3;
                startBonus(bonus);
                bonusBaseLevel = clearedLevel;
                return;
            }
            invulnerable = 1;
            sounds.push_back(Sound::Warp);
            rechargeDelay = 80.f / 60;
        }
        return;
    }
    if (rechargeDelay > 0) {
        rechargeDelay = std::max(0.f, rechargeDelay - dt);
        if (rechargeDelay == 0) {
            sounds.push_back(Sound::Wave);
            announce("SUPERZAPPER RECHARGE");
        }
    }
    if (!sourceUpdate)
        return;
    dt = 1.f / 60;
    waveTime += dt;
    for (auto &e : enemies)
        if (!e.serial)
            e.serial = nextSerial++;
    for (int i = 0; i < web.lanes(); ++i)
        if (hasSpike(i) && !spikeSerials[size_t(i)])
            spikeSerials[size_t(i)] = nextSerial++;
    bulletExpanded = bulletExpanded || droidActive;
    for (auto &shot : shots) {
        if (!shot.serial)
            shot.serial = nextSerial++;
        if (shot.tableSlot < 0)
            shot.tableSlot = claimBulletSlot();
    }
    bool pulse = pulseActive();
    if (pulse && !lastPulse && std::any_of(enemies.begin(), enemies.end(), [](auto &e) {
            return !e.entering && (e.kind == Kind::Pulsar || e.kind == Kind::PulsarSpark);
        }))
        sounds.push_back(Sound::Pulse);
    lastPulse = pulse;
    const auto &wave = campaignWaves[size_t(level - 1)];
    generationClock += double(dt) * 60;
    while (generationClock >= 1 - .000001) {
        generationClock -= 1;
        for (size_t i = 0; i < wave.spawns.size() && enemiesRemaining > 0; ++i) {
            if (--spawnTimers[i] < 0 && canGenerate()) {
                Kind kind = kindForSource(wave.spawns[i].type);
                if (kind == Kind::Flipper && level > 17 && randf(0, 256) < (level - 17) * 2)
                    kind = Kind::SuperFlipper;
                spawn(kind, -1, 1, true);
                --enemiesRemaining;
                spawnTimers[i] = wave.spawns[i].periodTicks;
            }
        }
    }
    bool zapFrame = zapActive && (sourceFrame & 3) == 0 && zapLastFrame != sourceFrame;
    bool zapped = false;
    if (zapFrame)
        zapLastFrame = sourceFrame;
    if (fireHeld && --fireFrames < 0) {
        fireFrames = beastly ? (boost ? 4 : 6) : (boost ? 2 : 3);
        if (shotSlots(false) < 8 && physicalSlots() < 64) {
            shots.push_back({lane, -jumpLift() / 9});
            shots.back().serial = nextSerial++;
            shots.back().bornFrame = sourceFrame;
            shots.back().tableSlot = claimBulletSlot();
            sounds.push_back(boost ? Sound::PoweredShot : Sound::Shot);
        }
    }
    if (droidActive)
        tickDroid(dt);
    for (auto &s : shots) {
        s.previous = s.depth;
        if (s.bornFrame != sourceFrame)
            s.depth += dt * shotSpeed();
    }
    tickPickups(dt);
    if (mode != Mode::Playing)
        return;
    std::vector<Enemy> children;
    struct MotionFrame {
        float depth = 0, angleStart = 0, angleEnd = 0, arc = 0;
        bool turning = false, turnBegan = false, sparkContact = false;
        bool vulnerable = true;
        int shotLane = 0;
        FlipperMotion flipperMotion = FlipperMotion::Climbing;
        DroidMotion droidMotion = DroidMotion::Climbing;
        FuseMotion fuseMotion = FuseMotion::Climbing;
        Enemy before;
        bool landed = false;
    };
    std::vector<MotionFrame> motion(enemies.size());
    // Snapshot handler inputs and movement, then resolve collisions/contact in
    // source actor order, independent of C++ vector storage order.
    for (size_t index = 0; index < enemies.size(); ++index) {
        auto &e = enemies[index];
        auto &frame = motion[index];
        frame.before = e;
        frame.depth = e.depth;
        frame.vulnerable = shotVulnerable(e);
        frame.shotLane = e.lane;
        frame.flipperMotion = e.motion;
        frame.droidMotion = e.droidMotion;
        frame.fuseMotion = e.fuseMotion;
        if (e.dead)
            continue;
        e.age += dt;
        if (e.entering) {
            e.aiClock += double(dt) * 60;
            while (e.aiClock >= 1 - .000001) {
                e.aiClock -= 1;
                e.depth -= 2.f / 160;
                if (--e.buildFrames <= 0) {
                    e.depth = 1;
                    if (e.kind == Kind::Pulsar && pulsePhase() >= 3) {
                        e.depth += 2.f / 160;
                        e.buildFrames = 1;
                    } else {
                        e.buildFrames = 50;
                        e.entering = false;
                        break;
                    }
                }
            }
            if (e.entering)
                continue;
            frame.before = e;
            frame.depth = e.depth;
        }
        if (e.kind == Kind::Powerup)
            continue;
        if (e.kind == Kind::EnemyShot || reflected(e.kind)) {
            e.depth -= dt * (e.kind == Kind::EnemyShot
                                 ? 1.5f * 60 / 160
                                 : shotSpeed() * (e.kind == Kind::BeastShot ? .5f : 1.f));
            continue;
        }
        if (e.kind == Kind::PulsarSpark)
            continue; // Its contact precedes colok at its list position.
        float speed = enemySpeed(e.kind);
        if (flipperBody(e)) {
            // sweb fixes the T2K rotation-table selector to 16, then indexes
            // rospeeds -> crossdels -> pauses. Counter expiry includes zero.
            int rotation = rospeeds[1];
            float pause = beastly ? 0 : float(pauses[size_t(crossdels[size_t(rotation)])] + 1) / 60;
            if (e.rim && e.motion == FlipperMotion::Climbing)
                e.motion = FlipperMotion::Stopped;
            if (e.motion == FlipperMotion::Climbing) {
                e.depth = std::max(0.f, e.depth - dt * speed);
                if (e.depth == 0) {
                    e.rim = true;
                    startFlipperTurn(e);
                }
            } else if (e.motion == FlipperMotion::Turning) {
                if (e.runningBelowRim) {
                    e.depth = std::max(0.f, e.depth - dt * speed);
                    if (e.depth == 0) {
                        e.rim = true;
                        e.runningBelowRim = false;
                    }
                }
                frame.turning = true;
                frame.angleStart = e.turnAngle;
                frame.arc = e.turnArc;
                float angularSpeed =
                    float(rotation * 60) * (e.kind == Kind::SuperFlipper ? 2.f : 1.f);
                frame.angleEnd = e.turnAngle + dt * angularSpeed;
                e.turnAngle = std::min(e.turnArc, frame.angleEnd);
                if (e.turnAngle >= e.turnArc) {
                    e.motion = FlipperMotion::Stopped;
                    e.stopTime = e.kind == Kind::Flipper && !e.emerging ? pause : 0;
                    frame.landed = true;
                }
            } else if (e.emerging && e.scarper == 0) {
                e.emerging = false;
                e.motion = e.runningBelowRim ? FlipperMotion::Stopped : FlipperMotion::Climbing;
                e.rim = false;
            } else {
                e.stopTime -= dt;
                if (e.stopTime <= 0)
                    startFlipperTurn(e);
            }
            if (!frame.turning && e.motion == FlipperMotion::Turning) {
                frame.turning = frame.turnBegan = true;
                frame.angleStart = frame.angleEnd = e.turnAngle;
                frame.arc = e.turnArc;
            }
        } else if (e.kind == Kind::Fuseball || e.kind == Kind::Adroid) {
            tickSpecialEnemy(e, dt);
        } else if (e.kind == Kind::Spiker) {
            // Its collie/alienfire/build handler runs at its list position below.
        } else if (!e.rim) {
            e.depth -= dt * speed;
            if (e.kind == Kind::Mirror)
                e.depth = std::max(.25f, e.depth);
        }
    }
    struct Impact {
        float time, depth;
        size_t shot;
        int enemy, kind, lane;
        uint64_t serial;
        int slot = 0;
    };
    auto later = [](const Impact &a, const Impact &b) {
        if (a.serial != b.serial)
            return a.serial < b.serial;
        if (a.time != b.time)
            return a.time > b.time;
        if (a.slot != b.slot)
            return a.slot > b.slot;
        return a.shot > b.shot; // colok scans the live bullet table before player contact.
    };
    std::priority_queue<Impact, std::vector<Impact>, decltype(later)> impacts(later);
    for (size_t ei = 0; ei < enemies.size(); ++ei) {
        const auto &e = enemies[ei];
        const auto &frame = motion[ei];
        if (e.dead || e.entering || e.kind == Kind::Powerup || frame.sparkContact)
            continue;
        impacts.push({1, e.depth, shots.size(), int(ei), int(e.kind), e.lane, e.serial});
        if (e.kind == Kind::PulsarSpark)
            impacts.push({-1, frame.depth, shots.size(), int(ei), -2, e.lane, e.serial});
        if (zapFrame && zapEligible(frame.before))
            impacts.push({-.5f, frame.depth, shots.size(), int(ei), -3, e.lane, e.serial});
        bool vulnerable = frame.vulnerable;
        // flipto rotates before flipcollie; its exact landing frame returns without collie.
        if (frame.before.motion == FlipperMotion::Turning)
            vulnerable = !frame.landed && shotVulnerable(e);
        if (!vulnerable)
            continue;
        int enemyZ = int(std::floor(30.0 + double(frame.depth) * 160));
        for (size_t si = 0; si < shots.size(); ++si) {
            const auto &shot = shots[si];
            float depth = shot.serial > e.serial ? shot.depth : shot.previous;
            int shotZ = int(std::floor(30.0 + double(depth) * 160));
            if (shot.serial > e.serial && shot.depth >= 1)
                continue;
            if (shot.bornFrame == sourceFrame && shot.creatorSerial < e.serial)
                continue;
            if (!shot.dead && shot.lane == frame.shotLane && std::abs(shotZ - enemyZ) < 6)
                impacts.push({0, frame.depth, si, int(ei), int(e.kind), frame.shotLane, e.serial,
                              shot.tableSlot});
        }
    }
    for (size_t si = 0; si < shots.size(); ++si) {
        const auto &shot = shots[si];
        float tip = spikes[size_t(shot.lane)];
        if (!shot.dead && hasSpike(shot.lane))
            impacts.push(
                {0, tip, si, -1, -1, shot.lane, spikeSerials[size_t(shot.lane)], shot.tableSlot});
    }
    std::vector<bool> enemyHit(enemies.size(), false), spikeHit(size_t(web.lanes()), false);
    auto contact = [&](size_t index) {
        auto &e = enemies[index];
        const auto &frame = motion[index];
        if (!e.dead && !enemyHit[index]) {
            if (frame.landed)
                sounds.push_back(Sound::FlipperLand);
            if ((flipperKind(frame.before.kind) &&
                 frame.flipperMotion == FlipperMotion::Climbing) ||
                (e.kind == Kind::Spiker && frame.before.spikerMotion != SpikerMotion::Seeking)) {
                float moved = e.depth;
                e.depth = frame.depth;
                tryAlienFire(e, dt, children);
                e.depth = moved;
            }
        }
        if (e.dead || e.entering || e.kind == Kind::Powerup || frame.landed)
            return;
        if (e.kind == Kind::Spiker) {
            tickSpecialEnemy(e, dt);
            return;
        }
        if (reflected(e.kind)) {
            if (e.lane == lane && frame.depth >= -2.f / 160 && e.depth <= 2.f / 160) {
                hit();
                if (deathTime > 0)
                    e.dead = true;
            }
            if (e.depth < -29.f / 160) // refsht removes at Z<1; webz=110, rim Z=30.
                e.dead = true;
            return;
        }
        if (e.kind == Kind::EnemyShot) {
            if (e.depth <= 0) {
                if (e.lane == lane)
                    hit();
                e.dead = true;
            }
            return;
        }
        if (e.kind == Kind::PulsarSpark) {
            if (frame.sparkContact)
                return;
            e.sparkTime -= dt;
            while (e.sparkTime <= .000001f) {
                e.sparkTime += 5.f / 60; // prop_del=$0404: expire after five 60 Hz updates.
                int next = e.lane + e.direction;
                if (!web.closed && (next < 0 || next >= web.lanes()))
                    e.direction = -e.direction;
                e.lane = boundedLane(e.lane + e.direction);
            }
            return;
        }
        if (flipperBody(e)) {
            bool catches = !e.emerging && std::abs(frame.depth) <= 2.f / 160 &&
                           (frame.turning ? !shotVulnerable(e) && e.turnFrom == lane
                                          : frame.before.rim && frame.shotLane == lane);
            if (catches) {
                hit(true);
                if (deathTime > 0 && snatched)
                    e.dead = true;
            }
            return;
        }
        if (e.kind == Kind::Pulsar && pulseActive() && e.lane == lane)
            hit();
        if (e.kind == Kind::Adroid) {
            if (frame.droidMotion == DroidMotion::Zapping && frame.shotLane == lane)
                hit();
            return; // Patrol is above the rim; it only kills while zapping.
        }
        if (e.kind == Kind::Fuseball) {
            if (frame.fuseMotion == FuseMotion::Crossing && std::abs(frame.depth) <= 2.f / 160 &&
                frame.shotLane == lane)
                hit();
            return;
        }
        bool tanker =
            e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker;
        if (tanker && e.depth <= 0) {
            e.depth = 0;
            openTanker(e, children);
            return;
        }
        if (e.depth <= 0) {
            e.depth = 0;
            e.rim = true;
            if (e.kind == Kind::Pulsar) {
                for (int d : {-1, 1}) {
                    auto spark = makeEnemy(Kind::PulsarSpark, e.lane, 0, d);
                    spark.rim = true;
                    children.push_back(spark);
                }
                e.dead = true;
                return;
            }
            if (e.lane == lane || (e.kind == Kind::Fuseball && boundedLane(e.lane + 1) == lane))
                hit();
        }
    };
    while (!impacts.empty()) {
        auto impact = impacts.top();
        impacts.pop();
        if (impact.kind == -3) {
            auto index = size_t(impact.enemy);
            auto &e = enemies[index];
            if (!e.dead && !motion[index].sparkContact && (!zapped || infiniteZap)) {
                if (e.kind == Kind::Tanker || e.kind == Kind::FuseTanker ||
                    e.kind == Kind::PulsarTanker)
                    openTanker(e, children);
                else
                    destroy(e, true);
                zapped = true;
                if (zapSingle)
                    zapActive = false;
            }
            continue;
        }
        if (impact.kind == -2) {
            auto index = size_t(impact.enemy);
            if (motion[index].shotLane == lane && std::abs(motion[index].depth) <= 2.f / 160) {
                hit();
                motion[index].sparkContact = deathTime > 0;
            }
            continue;
        }
        if (impact.shot == shots.size()) {
            contact(size_t(impact.enemy));
            continue;
        }
        auto &shot = shots[impact.shot];
        if (shot.dead)
            continue;
        bool powered = boost && !shot.ally;
        if (impact.enemy < 0) {
            if (spikeHit[size_t(shot.lane)])
                continue;
            auto serial = spikeSerials[size_t(shot.lane)];
            if (shot.bornFrame == sourceFrame && shot.creatorSerial < serial)
                continue;
            if (shot.serial > serial && shot.depth >= 1)
                continue;
            float depth = shot.serial > serial ? shot.depth : shot.previous;
            auto &tip = spikes[size_t(shot.lane)];
            if (!hasSpike(shot.lane) || std::abs(int(std::floor(30.0 + double(depth) * 160)) -
                                                 int(std::floor(30.0 + double(tip) * 160))) >= 6)
                continue;
            spikeHit[size_t(shot.lane)] = true;
            bool super = superSpikes[size_t(shot.lane)];
            if (!powered || super)
                shot.dead = true;
            burst(position(float(shot.lane), tip), 2, 5);
            // colok: ordinary shots spatter harmlessly on super spikes;
            // xle3 consumes a laser ring but still returns a shortening hit.
            if (!super || powered) {
                sounds.push_back({Sound::Spike, 524 + (int(std::round((1 - tip) * 160)) & 511)});
                tip += 3.f / 160;
                addScore(3);
                if (tip > 1.000001f) {
                    tip = 1;
                    spikePresent[size_t(shot.lane)] = superSpikes[size_t(shot.lane)] = false;
                    addScore(3);
                } else {
                    tip = std::min(tip, 1.f);
                    spikePresent[size_t(shot.lane)] = true;
                }
            }
            continue;
        }
        auto &e = enemies[size_t(impact.enemy)];
        if (e.dead || enemyHit[size_t(impact.enemy)] || motion[size_t(impact.enemy)].sparkContact)
            continue;
        enemyHit[size_t(impact.enemy)] = true;
        if (motion[size_t(impact.enemy)].turnBegan) {
            // rail/stdstop test bullets before changing lanes. In particular,
            // a pickup from that kill belongs to the lane the flipper occupied.
            e.lane = e.turnFrom;
            e.motion = FlipperMotion::Stopped;
        }
        if (!powered)
            shot.dead = true;
        if (e.kind == Kind::EnemyShot) {
            destroy(e);
            continue;
        }
        bool tanker =
            e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker;
        if (tanker) {
            openTanker(e, children);
        } else if (e.kind == Kind::BeastShot) {
            // refsht2 absorbs normal shots and survives; laser rings pass through.
        } else if (e.kind == Kind::Mirror) {
            --e.hp;
            if (e.hp > 0)
                e.depth = motion[size_t(impact.enemy)].depth + enemySpeed(Kind::Mirror) * 16 / 60;
            if (e.hp > 0 && e.depth <= 1) {
                shot.dead = shot.takenOver = true; // mcollie takes over even a particle-laser ring.
                float bulletDepth = shot.serial > e.serial ? shot.depth : shot.previous;
                auto reflection = makeEnemy(Kind::MirrorShot, e.lane, bulletDepth, 1);
                reflection.serial = shot.serial;
                if (shot.serial < e.serial)
                    reflection.depth -= shotSpeed() / 60;
                reflection.ally = shot.ally;
                children.push_back(reflection);
                burst(enemyPosition(e), 1, 8);
                sounds.push_back(Sound::MirrorReflect);
            } else {
                shot.dead = false; // Fatal mcollie exits before taking over the shot.
                destroy(e);
            }
        } else if (e.kind == Kind::Beast &&
                   motion[size_t(impact.enemy)].flipperMotion == FlipperMotion::Climbing &&
                   e.hp > 1) {
            int hp = e.hp - 1;
            e = motion[size_t(impact.enemy)].before;
            e.hp = hp;
            shot.dead = true;
            shot.takenOver = true;
            float bulletDepth = shot.serial > e.serial ? shot.depth : shot.previous;
            auto reflection = makeEnemy(Kind::BeastShot, e.lane, bulletDepth, 1);
            reflection.serial = shot.serial;
            if (shot.serial < e.serial)
                reflection.depth -= shotSpeed() / 120;
            reflection.ally = shot.ally;
            reflection.hp = e.hp; // Preserve the shed beast stage for rendering.
            children.push_back(reflection);
        } else if (e.kind == Kind::Adroid &&
                   motion[size_t(impact.enemy)].droidMotion == DroidMotion::Climbing) {
            shot.dead = true;
            e = motion[size_t(impact.enemy)].before;
            e.depth = motion[size_t(impact.enemy)].depth + enemySpeed(Kind::Flipper) * 32 / 60;
            if (e.depth >= 1)
                destroy(e);
            else
                burst(enemyPosition(e), 1, 8);
        } else {
            if (e.kind == Kind::Beast &&
                motion[size_t(impact.enemy)].flipperMotion == FlipperMotion::Climbing)
                shot.dead = false; // Final beastrail mcollie does not consume the bullet.
            destroy(e);
        }
    }
    if (zapFrame && (!zapped || infiniteZap))
        zapActive = false;
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](auto &e) { return e.dead; }),
                  enemies.end());
    for (auto &child : children)
        if (!child.serial)
            child.serial = nextSerial++;
    for (auto &child : pending)
        child.serial = nextSerial++;
    enemies.insert(enemies.end(), children.begin(), children.end());
    enemies.insert(enemies.end(), pending.begin(), pending.end());
    pending.clear();
    for (const auto &shot : shots)
        if (shot.dead && !shot.takenOver)
            effects.push_back({EffectKind::Splatter,
                               position(float(shot.lane), shot.depth),
                               {},
                               0,
                               7,
                               0,
                               1,
                               shot.ally});
    shots.erase(
        std::remove_if(shots.begin(), shots.end(), [](auto &s) { return s.dead || s.depth >= 1; }),
        shots.end());
    // r_end/szoom: after generation ends, leave as soon as the furthest
    // counted enemy reaches the rim. Pickups/spikes do not hold the wave open;
    // alien bullets do. Rim enemies need not be killed before clearance.
    bool belowRim = std::any_of(enemies.begin(), enemies.end(), [](const auto &e) {
        return !e.dead && e.kind != Kind::Powerup && !reflected(e.kind) && e.depth > 0;
    });
    if (mode == Mode::Playing && deathTime <= 0 && enemiesRemaining <= 0 && !belowRim)
        completeWave();
}
} // namespace tempest

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
    targetLevel = std::min(100, level + 1);
    auto next = makeWeb(targetLevel);
    int first = std::clamp(next.start, 0, next.lanes() - 1);
    nextSpawn =
        (next.points[size_t(first)] + next.points[size_t(first + 1) % next.points.size()]) * .5f;
    lane = std::clamp(web.start, 0, web.lanes() - 1);
    visualLane = float(lane);
    wheelRemainder = 0;
    transitionOffset = {};
    waveTime = rechargeDelay = 0;
    fireTimer = 0;
    const auto &wave = campaignWaves[size_t(level - 1)];
    waveSize = wave.count;
    enemiesRemaining = waveSize;
    lastPulse = false;
    spawnTimers.clear();
    for (auto rule : wave.spawns)
        spawnTimers.push_back(float(rule.periodTicks) / 120.f);
    enemies.clear();
    pending.clear();
    shots.clear();
    spikes.assign(size_t(web.lanes()), 1.f);
    zaps = 2;
    droidActive = earlyDroid = false;
    droidTimer = 0;
    pendingBonus = -1;
    boost = false;
    jumpEnabled = false;
    powerupStage = 0;
    killsToPowerup = 1;
    kills = 0;
    // Do not reset an airborne jump or its cooldown: the rig must cross webs continuously.
    unlockedLevel = std::max(unlockedLevel, level);
    selectedLevel = level;
}
void Game::start(int startingLevel) {
    level = std::clamp(startingLevel, 1, 100);
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
    droidPending = earlyDroid = false;
    pendingBonus = -1;
    returnTime = rechargeDelay = 0;
    droidActive = false;
    boost = false;
    jumpEnabled = false;
    powerupStage = 0;
    killsToPowerup = 1;
    droidTimer = 0;
    particles.clear();
    sounds.clear();
    loadWeb();
    mode = Mode::Playing;
    sounds.push_back(Sound::Wave);
    announce("SUPERZAPPER RECHARGE");
}
void Game::announce(const std::string &text, float seconds) {
    message = text;
    messageTime = seconds;
    messageDuration = seconds;
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
    if (deathTime > 0)
        return;
    if (mode == Mode::Menu) {
        wheelRemainder += notches;
        int steps = int(wheelRemainder);
        wheelRemainder -= steps;
        selectedLevel = std::clamp(selectedLevel + steps, 1, unlockedLevel);
        return;
    }
    if (mode == Mode::Bonus) {
        bonusX = std::clamp(bonusX + notches * .16f, -4.5f, 4.5f);
        bonusAngle += notches * .19634954f;
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
    return !flipperKind(e.kind) || e.motion != FlipperMotion::Turning ||
           std::abs(std::remainder(e.turnArc - e.turnAngle, 256.f)) <= 52.f;
}
void Game::startFlipperTurn(Enemy &e) {
    int delta = e.lane - lane;
    if (delta == 0) {
        e.motion = FlipperMotion::Stopped;
        return;
    }
    int direction = delta > 0 ? -1 : 1;
    // _fset's exact half-web tie goes the other way, including circular wrap.
    if (web.closed && std::abs(delta) >= web.lanes() / 2)
        direction = -direction;
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
    if (!flipperKind(e.kind) || e.motion != FlipperMotion::Turning || e.turnFrom < 0)
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
    if (flipperKind(e.kind) && e.motion == FlipperMotion::Turning && e.turnFrom >= 0) {
        int pivotLane = e.turnFrom + (e.direction > 0 ? 1 : 0);
        Vec3 pivot = edge(pivotLane, e.depth), a = position(float(e.turnFrom), e.depth) - pivot,
             b = position(float(e.lane), e.depth) - pivot;
        float angle = flipRotation(e), radius = std::hypot(a.x, a.y),
              u = std::clamp(e.turnAngle / e.turnArc, 0.f, 1.f),
              scale = (radius * (1 - u) + std::hypot(b.x, b.y) * u) / std::max(.0001f, radius);
        return pivot + Vec3{(a.x * std::cos(angle) - a.y * std::sin(angle)) * scale,
                            (a.x * std::sin(angle) + a.y * std::cos(angle)) * scale, 0};
    }
    return e.kind == Kind::Fuseball ? edge(e.lane + 1, e.depth) : position(float(e.lane), e.depth);
}
Vec3 Game::enemyTangent(const Enemy &e) const {
    int visualLane = flipperKind(e.kind) && e.motion == FlipperMotion::Turning && e.turnFrom >= 0
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
    score += std::max(0, points);
    while (score >= nextLife) {
        lives++;
        nextLife += 20000;
        sounds.push_back(Sound::OneUp);
        oneUpTime = 1.8f;
    }
    best = std::max(best, score);
}
bool Game::hostile(Kind kind) {
    return kind != Kind::Powerup && kind != Kind::EnemyShot;
}
Kind Game::kindForSource(int type) {
    constexpr Kind kinds[] = {Kind::Flipper,      Kind::Tanker,       Kind::Spiker,
                              Kind::Fuseball,     Kind::Pulsar,       Kind::FuseTanker,
                              Kind::PulsarTanker, Kind::SuperFlipper, Kind::Mirror,
                              Kind::Adroid,       Kind::Beast,        Kind::SuperFlipper2};
    return kinds[std::clamp(type, 0, 11)];
}
static Enemy makeEnemy(Kind kind, int lane, float depth, int direction) {
    Enemy e{kind, lane, depth, 0, 0, direction};
    e.hp = kind == Kind::Mirror ? 5 : kind == Kind::Beast ? 3 : 1;
    e.firing = 1.5f;
    return e;
}
void Game::spawn(Kind kind, int spawnLane, float depth) {
    int l = spawnLane < 0 ? int(randf(0, float(web.lanes()) - .001f)) : boundedLane(spawnLane);
    enemies.push_back(makeEnemy(kind, l, depth, randf(0, 1) > .5f ? 1 : -1));
}
void Game::destroy(Enemy &e, bool zap) {
    if (e.dead)
        return;
    e.dead = true;
    burst(enemyPosition(e), int(e.kind) % 4,
          (e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker)
              ? 90
              : 48);
    if (e.kind == Kind::EnemyShot)
        return;
    int points = 150;
    switch (e.kind) {
    case Kind::Tanker:
    case Kind::FuseTanker:
    case Kind::PulsarTanker:
        points = 100;
        break;
    case Kind::Spiker:
        points = 50;
        break;
    case Kind::Fuseball:
        points = 250;
        break;
    case Kind::Pulsar:
        points = 200;
        break;
    case Kind::Mirror:
    case Kind::Adroid:
        points = 500;
        break;
    case Kind::Beast:
        points = 1000;
        break;
    default:
        break;
    }
    addScore(points);
    if (!zap) {
        sounds.push_back(Sound::Kill);
        kills++;
        if (--killsToPowerup <= 0) {
            pending.push_back(makeEnemy(Kind::Powerup, e.lane, std::clamp(e.depth, .1f, .95f), 1));
            int index = std::min(int(pup_stuff.size()) - 1, (level - 1) / 4);
            killsToPowerup = (pup_stuff[size_t(index)] & 255) + 1;
        }
    }
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
    if (full) {
        for (auto &e : enemies)
            if (e.kind != Kind::Powerup)
                destroy(e, true);
    } else {
        Enemy *target = nullptr;
        for (auto &e : enemies)
            if (!e.dead && e.kind != Kind::Powerup && (!target || e.depth < target->depth))
                target = &e;
        if (target)
            destroy(*target, true);
    }
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
        announce("PARTICLE LASER");
        break;
    case 2:
        jumpEnabled = true;
        announce("JUMP ENABLED");
        break;
    case 4:
        droidActive = true;
        droidLane = visualLane;
        announce("A.I. DROID");
        break;
    case 5:
        bonusTokens++;
        sounds.push_back(Sound::BonusToken);
        announce("BONUS TOKEN " + std::to_string(bonusTokens) + " / 3");
        zapTime = 1.1f;
        for (auto &e : enemies)
            if (e.kind != Kind::Powerup && !e.dead)
                destroy(e, true);
        break;
    default:
        addScore(2000);
        announce("EXCELLENT");
        sounds.push_back(Sound::Excellent);
        break;
    }
    powerupStage = std::min(7, powerupStage + 1);
    sounds.push_back(Sound::Pickup);
    burst(shipPosition(), 3, 100);
}
bool Game::pulseActive() const {
    int index = std::min(int(pudels.size()) - 1, (level - 1) / 4);
    float period = float(pudels[size_t(index)] & 255) * 16 / 60.f;
    return std::fmod(waveTime, std::max(.4f, period)) > period * .65f;
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
    sounds.push_back(Sound::ClearanceEngine);
    announce("WEB CLEAR", 2.4f);
}
void Game::completeWave() {
    addScore(1000);
    if (level >= 100) {
        mode = Mode::Victory;
        unlockedLevel = 100;
        announce("YOU ARE A SUPERSTAR", 10);
        sounds.push_back(Sound::Excellent);
        burst(shipPosition(), 3, 800);
        return;
    }
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
    bonusDistance = bonusImpulse = bonusEffectTime = 0;
    bonusBoosts = bonusLastScore = 0;
    bonusSpeed =
        (bonusType == 2 ? 37.5f : 30.f) + float(bonusRuns[size_t(bonusType)] - 1) * .46875f;
    bonusGate = 0;
    bonusHits = 0;
    bonusX = bonusAngle = 0;
    bonusY = bonusType == 1 ? 0.f : .25f / 32; // m7go initializes vp_y=$4000.
    droidActive = false;
    bonusEntryOffset = entry - (bonusCursor() + Vec3{0, 0, -3.8f});
    bonusCheckTime = 0;
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
    // cg starts at 16 units; the first gate travels from Z=255 to Z<=2.
    // These first-run distances include 60 Hz countdown quantization. Later
    // gaps use (packed delay + 1) * 12 units. Boosts accelerate every
    // approaching gate, rather than assigning each a fixed arrival timestamp.
    float distance = bonusType == 1 ? 60.f : bonusType == 2 ? 269.375f : 269.5f;
    for (int i = 0; i < gate; ++i)
        distance += bonusType == 1 ? 13.5f : float(bonusGateDelay(i) + 1) * 12;
    return distance;
}
float Game::bonusGateZ(int gate) const {
    return -3.8f - (bonusGateDistance(gate) - bonusDistance) / 32;
}
Vec3 Game::bonusTarget(int gate) const {
    if (bonusType == 1) {
        float angle = 0;
        const auto &course = tunnelCourses[size_t(bonusCourse)];
        for (int i = 0; i <= std::clamp(gate, 0, 63); i++)
            angle += float(course[size_t(i)]) * .049087385f;
        return {std::cos(angle) * 1.4f, std::sin(angle) * 1.4f, 0};
    }
    auto &course = ringCourses[size_t(bonusCourse)];
    uint32_t packed = course[size_t(std::clamp(gate, 0, int(course.size()) - 2))];
    float x = float(int8_t((packed >> 16) & 255)) / 32.f,
          y = float(int8_t((packed >> 8) & 255)) / 32.f;
    return {bonusType == 2 ? -x : x, y, 0};
}
Vec3 Game::bonusCursor() const {
    return bonusType == 1 ? Vec3{std::cos(bonusAngle) * 1.4f, std::sin(bonusAngle) * 1.4f, 0}
                          : Vec3{bonusX, bonusY, 0};
}
void Game::tickBonus(float dt) {
    bonusTime += dt;
    if (bonusFinished) {
        bonusResultTime -= dt;
        if (bonusResultTime <= 0)
            returnFromBonus();
        return;
    }
    bonusY = std::clamp(bonusY + (float(int(bonusUp) - int(bonusDown)) * 1.6f + bonusImpulse) * dt,
                        -4.5f, 4.5f);
    bonusImpulse *= std::exp(-6 * dt);
    bonusEffectTime = std::max(0.f, bonusEffectTime - dt);
    bonusDistance += bonusSpeed * dt;
    if (bonusDistance >= bonusGateDistance(bonusGate)) {
        auto target = bonusTarget(bonusGate), cursor = bonusCursor();
        float dx = std::abs(target.x - cursor.x), dy = std::abs(target.y - cursor.y);
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
            bonusImpulse = effect == 1 ? 2.4f : -2.4f;
            announce(effect == 1 ? "UP IMPULSE" : "DOWN IMPULSE", .9f);
            bonusEffectTime = 1;
        } else if (effect == 3) {
            bonusSpeed *= 1.125f; // spup adds one eighth of grndvel.
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
            bonusResultTime = 2;
            if (bonusType != 1)
                addScore(20000); // m7scr success; tundraw has its own continuous scoring.
            announce(level < 90 ? "WARP 5 LEVELS" : "EXCELLENT", 2);
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
        float previous = e.depth;
        e.depth = std::max(0.f, e.depth - dt * .28f);
        if (e.lane == lane && (std::abs(e.depth - shipDepth) < .065f ||
                               (mode == Mode::Transition && previous >= shipDepth - .1f &&
                                e.depth <= shipDepth + .1f))) {
            e.dead = true;
            collectPowerup();
        }
        if (e.age > 12)
            e.dead = true;
        if (mode == Mode::Transition)
            e.age += dt;
    }
}
void Game::tick(float dt) {
    if (mode == Mode::Paused)
        return;
    time += dt;
    flash = std::max(0.f, flash - dt);
    zapTime = std::max(0.f, zapTime - dt);
    messageTime = std::max(0.f, messageTime - dt);
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
    if (mode == Mode::Menu || mode == Mode::GameOver)
        return;
    if (mode == Mode::Victory) {
        if (particles.size() < 500)
            burst({randf(-2, 2), randf(-2, 2), -4}, 3, 40);
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
        tickPickups(dt);
        transitionTimer -= dt;
        float depth = warpDistance() / 9;
        if (depth >= spikes[size_t(lane)] && depth <= 1 && spikes[size_t(lane)] < .99f)
            hit();
        if (mode == Mode::GameOver || deathTime > 0)
            return;
        if (transitionTimer <= 0) {
            int bonus = pendingBonus, clearedLevel = level;
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
    waveTime += dt;
    bool pulse = pulseActive();
    if (pulse && !lastPulse && std::any_of(enemies.begin(), enemies.end(), [](auto &e) {
            return e.kind == Kind::Pulsar || e.kind == Kind::PulsarSpark;
        }))
        sounds.push_back(Sound::Pulse);
    lastPulse = pulse;
    const auto &wave = campaignWaves[size_t(level - 1)];
    if (enemiesRemaining > 0 && enemies.size() < 64) {
        for (size_t i = 0; i < wave.spawns.size() && enemiesRemaining > 0; i++) {
            spawnTimers[i] -= dt;
            if (spawnTimers[i] <= 0) {
                Kind kind = kindForSource(wave.spawns[i].type);
                if (kind == Kind::Flipper && level > 17 && randf(0, 256) < (level - 17) * 2)
                    kind = randf(0, 256) < level * 2 ? Kind::SuperFlipper2 : Kind::SuperFlipper;
                spawn(kind);
                enemiesRemaining--;
                spawnTimers[i] += float(wave.spawns[i].periodTicks) / 60.f;
            }
        }
    }
    fireTimer -= dt;
    if ((autoFire || fireHeld) && fireTimer <= 0) {
        shots.push_back({lane});
        fireTimer = boost ? .075f : .14f;
        sounds.push_back(boost ? Sound::PoweredShot : Sound::Shot);
    }
    if (droidActive) {
        droidTimer -= dt;
        const Enemy *target = nullptr;
        for (auto &e : enemies)
            if (!e.dead && hostile(e.kind) && (!target || e.depth < target->depth))
                target = &e;
        if (target) {
            int dl = boundedLane(int(std::round(droidLane)));
            droidLane += float(directionTo(dl, target->lane)) * dt * 10;
            droidLane =
                web.closed ? std::fmod(droidLane + n, n) : std::clamp(droidLane, 0.f, n - 1);
            if (droidTimer <= 0) {
                Shot shot{boundedLane(int(std::round(droidLane)))};
                shot.ally = true;
                shots.push_back(shot);
                droidTimer = .12f;
            }
        }
    }
    for (auto &s : shots) {
        s.previous = s.depth;
        s.depth += dt * (boost && !s.ally ? 3.3f : 2.1f);
    }
    tickPickups(dt);
    std::vector<Enemy> children;
    struct MotionFrame {
        float depth = 0, angleStart = 0, angleEnd = 0, arc = 0;
        bool turning = false, turnBegan = false, sparkContact = false;
    };
    std::vector<MotionFrame> motion(enemies.size());
    // Complete movement before resolving impacts. Entity storage order must not
    // decide which of two enemies in the same lane consumes a normal shot.
    for (size_t index = 0; index < enemies.size(); ++index) {
        auto &e = enemies[index];
        auto &frame = motion[index];
        frame.depth = e.depth;
        if (e.dead)
            continue;
        e.age += dt;
        e.flip += dt;
        e.firing -= dt;
        if (e.kind == Kind::Powerup)
            continue;
        if (e.kind == Kind::EnemyShot) {
            e.depth -= dt * .55f;
            continue;
        }
        if (e.kind == Kind::PulsarSpark) {
            // run_pspark: contact precedes bullets and propagation, regardless
            // of the global pulse phase. Movement is deferred until after hits.
            if (e.lane == lane && std::abs(e.depth) <= 2.f / 160) {
                hit();
                frame.sparkContact = deathTime > 0;
            }
            continue;
        }
        int tableIndex = std::min(int(fz_stuff.size()) - 1, (level - 1) / 2);
        bool tanker =
            e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker;
        int fixedSpeed = tanker                     ? tz_stuff[size_t(tableIndex)]
                         : e.kind == Kind::Spiker   ? sz_stuff[size_t(tableIndex)]
                         : e.kind == Kind::Fuseball ? fuz_stuff[size_t(tableIndex)]
                                                    : fz_stuff[size_t(tableIndex)];
        float speed = float(fixedSpeed) / 65536.f * 60 / 160;
        if (e.kind == Kind::SuperFlipper || e.kind == Kind::SuperFlipper2)
            speed *= 1.25f;
        if (e.kind == Kind::Beast)
            speed *= 1.6f;
        if (flipperKind(e.kind)) {
            // sweb fixes the T2K rotation-table selector to 16, then indexes
            // rospeeds -> crossdels -> pauses. Counter expiry includes zero.
            int rotation = rospeeds[1];
            float pause = float(pauses[size_t(crossdels[size_t(rotation)])] + 1) / 60;
            if (e.rim && e.motion == FlipperMotion::Climbing)
                e.motion = FlipperMotion::Stopped;
            if (e.motion == FlipperMotion::Climbing) {
                e.depth = std::max(0.f, e.depth - dt * speed);
                if (e.depth == 0) {
                    e.rim = true;
                    startFlipperTurn(e);
                }
            } else if (e.motion == FlipperMotion::Turning) {
                frame.turning = true;
                frame.angleStart = e.turnAngle;
                frame.arc = e.turnArc;
                float angularSpeed =
                    float(rotation * 60) * (e.kind == Kind::SuperFlipper ? 2.f : 1.f);
                frame.angleEnd = e.turnAngle + dt * angularSpeed;
                e.turnAngle = std::min(e.turnArc, frame.angleEnd);
                if (e.turnAngle >= e.turnArc) {
                    e.motion = FlipperMotion::Stopped;
                    e.stopTime = e.kind == Kind::Flipper ? pause : 0;
                    sounds.push_back(Sound::FlipperLand);
                }
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
        } else if (!e.rim) {
            e.depth += dt * speed * (e.retreating ? 1.5f : -1.f);
            if (e.kind == Kind::Spiker) {
                spikes[size_t(e.lane)] = std::min(spikes[size_t(e.lane)], e.depth);
                if (e.depth < .25f)
                    e.retreating = true;
                if (e.retreating && e.depth >= 1) {
                    e.lane = boundedLane(e.lane + e.direction);
                    e.retreating = false;
                    e.depth = 1;
                }
            }
            if (e.kind == Kind::Mirror)
                e.depth = std::max(.25f, e.depth);
        }
        if ((e.kind == Kind::Fuseball && e.flip > (e.rim ? .25f : .55f)) ||
            (e.kind == Kind::Adroid && e.rim && e.flip > .4f)) {
            int dir = e.kind == Kind::Fuseball ? directionTo(e.lane, lane) : e.direction;
            if (!web.closed && (e.lane + dir < 0 || e.lane + dir >= web.lanes())) {
                e.direction = -e.direction;
                dir = e.direction;
            }
            e.lane = boundedLane(e.lane + dir);
            e.flip = 0;
        }
        if (level >= 9 && e.kind != Kind::Fuseball && e.kind != Kind::PulsarSpark &&
            e.kind != Kind::Mirror && e.firing <= 0 && e.depth > .1f) {
            if (children.size() < 12)
                children.push_back(makeEnemy(Kind::EnemyShot, e.lane, e.depth, e.direction));
            e.firing = std::max(1.2f, 6.f - float(level) * .04f) + randf(0, 2);
        }
    }
    struct Impact {
        float time, depth;
        size_t shot;
        int enemy; // -1 is a spike.
        int kind, lane;
    };
    auto later = [](const Impact &a, const Impact &b) {
        return std::tie(a.time, a.depth, a.kind, a.lane, a.shot, a.enemy) >
               std::tie(b.time, b.depth, b.kind, b.lane, b.shot, b.enemy);
    };
    std::priority_queue<Impact, std::vector<Impact>, decltype(later)> impacts(later);
    auto interval = [](float start, float delta, float low, float high, float &enter, float &exit) {
        if (std::abs(delta) < .000001f)
            return start >= low && start <= high;
        float a = (low - start) / delta, b = (high - start) / delta;
        if (a > b)
            std::swap(a, b);
        enter = std::max(enter, a);
        exit = std::min(exit, b);
        return enter <= exit;
    };
    // colok's six source units on a 160-unit web, with swept relative motion.
    const float hitRadius = std::nextafter(6.f / 160, 0.f); // source comparison is strict
    for (size_t si = 0; si < shots.size(); ++si) {
        const auto &shot = shots[si];
        if (shot.dead)
            continue;
        float step = shot.depth - shot.previous;
        for (size_t ei = 0; ei < enemies.size(); ++ei) {
            const auto &e = enemies[ei];
            const auto &frame = motion[ei];
            if (e.dead || e.kind == Kind::Powerup || frame.sparkContact ||
                shot.lane != (frame.turnBegan ? e.turnFrom : e.lane))
                continue;
            bool powered = boost && !shot.ally;
            if (e.kind == Kind::Fuseball && e.flip < .13f && !powered)
                continue;
            float enter = 0, exit = 1;
            if (!interval(shot.previous - frame.depth, step - (e.depth - frame.depth), -hitRadius,
                          hitRadius, enter, exit))
                continue;
            if (frame.turning && !frame.turnBegan) {
                // flipcollie tests the wrapped target-angle distance <= 52.
                // Intersect that window with the projectile contact interval:
                // a shot which already left before vulnerability opens misses.
                float remaining = frame.arc - frame.angleStart,
                      delta = frame.angleStart - frame.angleEnd, earliest = 2;
                for (auto range : {std::pair{-10000.f, 52.f}, std::pair{204.f, 256.f}}) {
                    float a = enter, b = exit;
                    if (interval(remaining, delta, range.first, range.second, a, b))
                        earliest = std::min(earliest, a);
                }
                if (earliest > 1)
                    continue;
                enter = earliest;
            }
            float depth = frame.depth + (e.depth - frame.depth) * enter;
            impacts.push({enter, depth, si, int(ei), int(e.kind), e.lane});
        }
        float tip = spikes[size_t(shot.lane)];
        if (tip < .99f && step > 0 && shot.previous <= tip && shot.depth >= tip)
            impacts.push({(tip - shot.previous) / step, tip, si, -1, -1, shot.lane});
    }
    std::vector<bool> enemyHit(enemies.size(), false);
    while (!impacts.empty()) {
        auto impact = impacts.top();
        impacts.pop();
        auto &shot = shots[impact.shot];
        if (shot.dead)
            continue;
        bool powered = boost && !shot.ally;
        if (impact.enemy < 0) {
            auto &tip = spikes[size_t(shot.lane)];
            // Earlier bullets may have shortened this spike. Reschedule this
            // contact at its new tip instead of consuming a shot in empty space.
            if (tip != impact.depth) {
                if (tip < .99f && shot.previous <= tip && shot.depth >= tip)
                    impacts.push({(tip - shot.previous) / (shot.depth - shot.previous), tip,
                                  impact.shot, -1, -1, shot.lane});
                continue;
            }
            if (!powered)
                shot.dead = true;
            burst(position(float(shot.lane), tip), 2, 5);
            tip = std::min(1.f, tip + (powered ? .2f : .09f));
            addScore(3);
            continue;
        }
        auto &e = enemies[size_t(impact.enemy)];
        if (e.dead || enemyHit[size_t(impact.enemy)])
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
            Kind contents = e.kind == Kind::FuseTanker     ? Kind::Fuseball
                            : e.kind == Kind::PulsarTanker ? Kind::Pulsar
                                                           : Kind::Flipper;
            for (int d : {-1, 1})
                children.push_back(
                    makeEnemy(contents, boundedLane(e.lane + d), std::max(.16f, e.depth), d));
            destroy(e);
        } else if (e.kind == Kind::Mirror) {
            e.hp -= powered ? 2 : 1;
            if (!powered)
                children.push_back(makeEnemy(Kind::EnemyShot, e.lane, e.depth, 1));
            if (e.kind == Kind::Mirror)
                e.depth += .15f;
            if (e.hp > 0 && e.depth < 1) {
                burst(enemyPosition(e), 1, 8);
                sounds.push_back(Sound::Ricochet);
            } else
                destroy(e);
        } else if (e.kind == Kind::Beast && --e.hp > 0 && !powered) {
            e.direction = -e.direction;
        } else
            destroy(e);
    }
    // A stopped flipper checks bullets before capturing the player (stdstop).
    // During an invulnerable turn it can instead catch the lane it is leaving.
    for (size_t index = 0; index < enemies.size(); ++index) {
        auto &e = enemies[index];
        const auto &frame = motion[index];
        if (e.dead || e.kind == Kind::Powerup)
            continue;
        if (e.kind == Kind::EnemyShot) {
            if (e.depth <= 0) {
                if (e.lane == lane)
                    hit();
                e.dead = true;
            }
            continue;
        }
        if (e.kind == Kind::PulsarSpark) {
            if (frame.sparkContact)
                continue;
            e.sparkTime -= dt;
            while (e.sparkTime <= .000001f) {
                e.sparkTime += 5.f / 60; // prop_del=$0404: expire after five 60 Hz updates.
                int next = e.lane + e.direction;
                if (!web.closed && (next < 0 || next >= web.lanes()))
                    e.direction = -e.direction;
                e.lane = boundedLane(e.lane + e.direction);
            }
            continue;
        }
        if (flipperKind(e.kind)) {
            bool catches =
                frame.turning ? !shotVulnerable(e) && e.turnFrom == lane : e.rim && e.lane == lane;
            if (catches) {
                hit(true);
                if (deathTime > 0 && snatched)
                    e.dead = true;
            }
            continue;
        }
        if (e.kind == Kind::Pulsar && pulseActive() && e.lane == lane)
            hit();
        if (e.kind == Kind::Adroid && e.rim && std::fmod(e.age, 2.f) > .9f && e.lane == lane)
            hit();
        bool tanker =
            e.kind == Kind::Tanker || e.kind == Kind::FuseTanker || e.kind == Kind::PulsarTanker;
        if (tanker && e.depth < .22f) {
            Kind contents = e.kind == Kind::FuseTanker     ? Kind::Fuseball
                            : e.kind == Kind::PulsarTanker ? Kind::Pulsar
                                                           : Kind::Flipper;
            for (int d : {-1, 1})
                children.push_back(
                    makeEnemy(contents, boundedLane(e.lane + d), std::max(.16f, e.depth), d));
            destroy(e);
            continue;
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
                continue;
            }
            if (e.lane == lane || (e.kind == Kind::Fuseball && boundedLane(e.lane + 1) == lane))
                hit();
        }
    }
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](auto &e) { return e.dead; }),
                  enemies.end());
    enemies.insert(enemies.end(), children.begin(), children.end());
    enemies.insert(enemies.end(), pending.begin(), pending.end());
    pending.clear();
    shots.erase(std::remove_if(shots.begin(), shots.end(),
                               [](auto &s) { return s.dead || s.depth > 1.05f; }),
                shots.end());
    // r_end/szoom: after generation ends, leave as soon as the furthest
    // counted enemy reaches the rim. Pickups/spikes do not hold the wave open;
    // alien bullets do. Rim enemies need not be killed before clearance.
    bool belowRim = std::any_of(enemies.begin(), enemies.end(), [](const auto &e) {
        return !e.dead && e.kind != Kind::Powerup && e.depth > 0;
    });
    if (mode == Mode::Playing && deathTime <= 0 && enemiesRemaining <= 0 && !belowRim)
        completeWave();
}
} // namespace tempest

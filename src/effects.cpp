#include "fireworks.hpp"
#include "game.hpp"

namespace tempest {
void Game::surprisePowerup() {
    // suprise: nine of the 256 random byte values select the early-exit branch.
    if (randf(0, 256) < 9) {
        addScore(3000);
        earlyExit = true;
        startZap(false, true);
        beginTransition(level + 1);
        announce("OUTTAH HERE!", 2.4f);
    } else {
        addScore(2000);
        announce("EXCELLENT");
    }
    sounds.push_back(Sound::Excellent);
}
void Game::tickEffects(float dt) {
    effectClock += double(dt) * 60;
    while (effectClock >= 1 - .000001) {
        effectClock -= 1;
        for (auto &fx : effects) {
            if (fx.kind == EffectKind::Firework && fx.frame <= fx.duration) {
                fx.pos = fx.pos + fx.velocity * (1.f / 60);
                fx.velocity.y -= .28125f / 60; // run_fw adds $100 to source Y velocity.
            }
            ++fx.frame;
        }
        effects.erase(std::remove_if(effects.begin(), effects.end(),
                                     [](const auto &fx) {
                                         return fx.frame >
                                                fx.duration +
                                                    (fx.kind == EffectKind::Firework ? 63 : 0);
                                     }),
                      effects.end());
        if (mode == Mode::Victory || enteringScore)
            tickFireworks();
    }
}
void Game::tickFireworks() {
    if (--fireworkWait >= 0)
        return;
    auto byte = [&]() { return fireworkProgram[fireworkPC++ % fireworkProgram.size()]; };
    // The imported program always yields on w; the guard also bounds malformed data.
    for (int instructions = 0; instructions < 4096; ++instructions) {
        char command = char(byte());
        if (command == '.') {
            if (objectSlots() < 32 && physicalSlots() < 64)
                effects.push_back({EffectKind::Firework,
                                   {fireworkPosition.x * .25f, -fireworkPosition.y * .25f,
                                    -3.8f - fireworkPosition.z},
                                   {fireworkVelocity.x * 1.2f, -fireworkVelocity.y * 1.2f,
                                    -fireworkVelocity.z * 1.2f},
                                   0,
                                   fireworkDuration,
                                   fireworkColor,
                                   2});
        } else if (command == 'd')
            fireworkDuration = byte();
        else if (command == 'c')
            fireworkColor = byte();
        else if (command == 'w') {
            fireworkWait = byte();
            return;
        } else if (command == '(') {
            int count = byte();
            fireworkStack.emplace_back(fireworkPC, count);
        } else if (command == '[')
            fireworkStack.emplace_back(fireworkPC, -1);
        else if (command == ')' || command == ']') {
            if (fireworkStack.empty())
                return;
            auto &loop = fireworkStack.back();
            if (command == ']' || --loop.second >= 0)
                fireworkPC = loop.first;
            else
                fireworkStack.pop_back();
        } else if (command == '+' || command == '-') {
            char variable = char(byte());
            int sign = command == '+' ? 1 : -1;
            if (variable == 'd')
                fireworkDuration += sign * 5;
            else
                fireworkColor = (fireworkColor + sign) & 255;
        } else if ((command >= 'X' && command <= 'Z') || (command >= 'x' && command <= 'z')) {
            bool velocity = command < 'a';
            int axis = command - (velocity ? 'X' : 'x');
            auto &v = velocity ? fireworkVelocity : fireworkPosition;
            float &value = axis == 0 ? v.x : axis == 1 ? v.y : v.z;
            char op = char(byte());
            if (op == '!')
                value = -value;
            else {
                int operand = byte();
                if (op == '=')
                    value = float(int8_t(operand)) / 16;
                else if (op == '+')
                    value += float(operand) / 16;
                else if (op == '-')
                    value -= float(operand) / 16;
            }
        }
    }
}
void Game::offerHighScore() {
    pendingScore = score >= highScores.back().score;
    bool keyPossible =
        (accessKey >= 0 && accessKeys[size_t(accessKey)].unlocked) ||
        (runHighest > 17 && std::any_of(accessKeys.begin(), accessKeys.end(), [&](const auto &k) {
             return !k.unlocked || checkpoint(runHighest) > k.unlocked;
         }));
    if (!pendingScore && !keyPossible) {
        frontPage = FrontPage::Scores;
        frontAge = 0;
        frontTicks = 0;
        return;
    }
    enteringScore = true;
    enteringBrag = false;
    wheelRemainder = 0;
    scoreChoice = 0;
    scoreWheelPosition = 0;
    scoreInitials = "...";
    scoreCursor = 0;
    frontPage = FrontPage::Initials;
    fireworkPC = 0;
    fireworkWait = 1;
    fireworkStack.clear();
    effects.clear();
    if (accessKey >= 0 && accessKeys[size_t(accessKey)].unlocked) {
        scoreInitials = accessKeys[size_t(accessKey)].initials;
        finishScoreEntry(); // getinitials reuses the selected key's signature.
    }
}
void Game::setScoreLetter(char letter) {
    if (!enteringScore ||
        std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ .!-").find(letter) == std::string::npos)
        return;
    scoreInitials[size_t(scoreCursor)] = letter;
    if (++scoreCursor == (enteringBrag ? 5 : 3))
        finishScoreEntry();
}
void Game::nextScoreLetter() {
    if (!enteringScore)
        return;
    if (scoreChoice == 30) {
        if (scoreCursor)
            scoreInitials[size_t(--scoreCursor)] = '.';
    } else if (scoreChoice == 31)
        finishScoreEntry();
    else
        setScoreLetter("ABCDEFGHIJKLMNOPQRSTUVWXYZ .!-"[scoreChoice]);
}
void Game::finishScoreEntry() {
    if (!enteringScore)
        return;
    enteringScore = false;
    wheelRemainder = 0;
    if (enteringBrag) {
        brag = scoreInitials;
        enteringBrag = false;
        ++recordsVersion;
        frontPage = FrontPage::Scores;
        frontAge = 0;
        frontTicks = 0;
        return;
    }
    bool first = pendingScore && score >= highScores.front().score;
    if (pendingScore) {
        auto place = std::find_if(highScores.begin(), highScores.end(),
                                  [&](const auto &entry) { return score >= entry.score; });
        if (place != highScores.end()) {
            std::move_backward(place, highScores.end() - 1, highScores.end());
            *place = {scoreInitials, score, level, beastly};
        }
    }
    awardKey();
    ++recordsVersion;
    if (first) {
        enteringBrag = enteringScore = true;
        scoreInitials = ".....";
        scoreCursor = 0;
        scoreChoice = 0;
        scoreWheelPosition = 0;
        frontPage = FrontPage::Initials;
        return;
    }
    frontPage = FrontPage::Scores;
    frontAge = 0;
    frontTicks = 0;
}
void Game::awardKey() {
    if (accessKey >= 0 && accessKeys[size_t(accessKey)].unlocked) {
        accessKeys[size_t(accessKey)].unlocked = checkpoint(runHighest);
        return;
    }
    if (runHighest <= 17)
        return;
    int resume = checkpoint(runHighest);
    auto found = std::find_if(accessKeys.begin(), accessKeys.end(), [&](const auto &k) {
        return k.unlocked && k.initials == scoreInitials;
    });
    if (found == accessKeys.end())
        found = std::find_if(accessKeys.begin(), accessKeys.end(),
                             [](const auto &k) { return !k.unlocked; });
    if (found == accessKeys.end()) {
        found = accessKeys.begin();
        for (auto it = accessKeys.begin(); it != accessKeys.end(); ++it)
            if (it->unlocked <= found->unlocked)
                found = it;
        if (found->unlocked >= runHighest)
            return;
    }
    if (found->initials != scoreInitials || found->unlocked < resume)
        *found = {resume, beastly, scoreInitials};
}
void Game::storeAccessKey() { /* Keys are awarded only by the end-of-game initials flow. */ }
void Game::selectAccessKey() {
    if (mode != Mode::Menu)
        return;
    frontPage = FrontPage::Keys;
    frontAge = 0;
    frontTicks = 0;
    keyCursor = 0;
    for (int i = 0; i < 4; ++i)
        if (accessKeys[size_t(i)].unlocked) {
            keyCursor = i;
            break;
        }
}
void Game::planBonusGates() {
    gateLaunches.assign(size_t(bonusGateCount()), 0);
    int32_t counter = gateCounter;
    int timer = 0, next = 0;
    int64_t ground = 0;
    while (next < bonusGateCount()) {
        ground += bonusVelocityFixed;
        counter -= bonusVelocityFixed;
        if (counter >= 0)
            continue;
        counter += 12 << 16;
        if (--timer >= 0)
            continue;
        gateLaunches[size_t(next)] = ground;
        timer = bonusGateDelay(next++);
    }
}
void Game::advanceBonusGenerator() {
    gateCounter -= bonusVelocityFixed;
    if (gateCounter >= 0)
        return;
    gateCounter += 12 << 16;
    if (--gateTimer >= 0 || gateNext >= bonusGateCount())
        return;
    gateLaunches[size_t(gateNext)] = bonusGroundFixed;
    gateTimer = bonusGateDelay(gateNext++);
}
void Game::rotary(float notches) {
    if (!std::isfinite(notches))
        return;
    if (mode == Mode::Menu || enteringScore) {
        steer(notches);
        return;
    }
    if (!rotaryMoved)
        rotarySensitivity = std::min(0x5000, rotarySensitivity + 0x1000);
    rotaryMoved = true;
    steer(notches * rotaryMultiplier());
}
void Game::tickRotaryCalibration() {
    if (!rotaryMoved && rotarySensitivity > 0x2000)
        rotarySensitivity -= 0x200;
    rotaryMoved = false;
}
} // namespace tempest

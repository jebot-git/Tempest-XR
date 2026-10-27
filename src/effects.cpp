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
        if (mode == Mode::Victory)
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
                                   fireworkColor});
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
    if (score <= highScores.back().score)
        return;
    enteringScore = true;
    wheelRemainder = 0;
    scoreInitials = "AAA";
    scoreCursor = 0;
}
void Game::setScoreLetter(char letter) {
    if (!enteringScore || !((letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9')))
        return;
    scoreInitials[size_t(scoreCursor)] = letter;
    scoreCursor = std::min(2, scoreCursor + 1);
}
void Game::nextScoreLetter() {
    if (!enteringScore)
        return;
    if (scoreCursor == 2)
        finishScoreEntry();
    else
        ++scoreCursor;
}
void Game::finishScoreEntry() {
    if (!enteringScore)
        return;
    enteringScore = false;
    wheelRemainder = 0;
    auto insertion = std::find_if(highScores.begin(), highScores.end(),
                                  [&](const auto &entry) { return score > entry.score; });
    if (insertion != highScores.end()) {
        std::move_backward(insertion, highScores.end() - 1, highScores.end());
        *insertion = {scoreInitials, score, level, beastly};
        ++recordsVersion;
    }
}
void Game::storeAccessKey() {
    auto &key = accessKeys[size_t(accessKey)];
    key.unlocked = std::max(key.unlocked, unlockedLevel);
    key.beastly = key.beastly || beastlyUnlocked;
}
void Game::selectAccessKey() {
    if (mode != Mode::Menu)
        return;
    storeAccessKey();
    accessKey = (accessKey + 1) % int(accessKeys.size());
    unlockedLevel = selectedLevel = accessKeys[size_t(accessKey)].unlocked;
    beastlyUnlocked = accessKeys[size_t(accessKey)].beastly;
    beastly = false;
    ++recordsVersion;
}
} // namespace tempest

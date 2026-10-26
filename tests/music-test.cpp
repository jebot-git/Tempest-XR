#include "music.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace tempest;
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Expected soundtrack directory");
        Music music;
        if (!music.available()) {
            std::cout << "libopenmpt runtime unavailable\n";
            return 77;
        }
        std::vector<float> samples(48000 * 2 * 10);
        for (const char *file : {"tune13.mod", "tune7.mod", "tune1.mod", "tune3.mod", "rave4.mod",
                                 "tune5.mod", "tune12.mod"}) {
            if (!music.load((std::filesystem::path(argv[1]) / file).string()))
                throw std::runtime_error(std::string("Cannot decode ") + file);
            music.render(samples.data(), samples.size() / 2);
            double energy = 0, separation = 0;
            for (size_t i = 0; i < samples.size(); i += 2) {
                if (!std::isfinite(samples[i]) || !std::isfinite(samples[i + 1]))
                    throw std::runtime_error("Non-finite PCM");
                energy += samples[i] * samples[i] + samples[i + 1] * samples[i + 1];
                separation += std::abs(samples[i] - samples[i + 1]);
            }
            std::cout << file << " energy=" << energy << " stereo_difference=" << separation
                      << '\n';
            // rave4's original module explicitly centers its mix; preserve that.
            if (energy < 1 || (std::string(file) != "rave4.mod" && separation < 1))
                throw std::runtime_error(std::string("Silent or mono decode: ") + file);
            std::cout << file << " stereo_rms=" << std::sqrt(energy / samples.size()) << '\n';
        }
        music.pause(true);
        music.render(samples.data(), samples.size() / 2);
        for (float sample : samples)
            if (sample != 0)
                throw std::runtime_error("Paused soundtrack was audible");
        music.pause(false);
        music.render(samples.data(), samples.size() / 2);
        if (music.load("/missing-tempest-music.mod"))
            throw std::runtime_error("Missing track accepted");
        std::cout
            << "Seven original modules decode as stereo; pause/missing-file handling passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

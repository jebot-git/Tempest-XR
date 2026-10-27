#pragma once
#include <cstddef>
#include <memory>
#include <string>
namespace tempest {
// Optional libopenmpt runtime backend; game/audio effects work without it.
class Music {
    struct Impl;
    std::unique_ptr<Impl> p;

  public:
    Music();
    ~Music();
    Music(const Music &) = delete;
    Music &operator=(const Music &) = delete;
    bool load(const std::string &path);
    void render(float *stereo, size_t frames);
    void pause(bool value);
    bool available() const;
    bool transitioning() const;
    static const char *trackForLevel(int level, bool title = false);
};
} // namespace tempest

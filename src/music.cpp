#include "music.hpp"
#include <algorithm>
#include <cstdint>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <vector>

// Stable C ABI declarations from lib.openmpt.org/doc/group__libopenmpt__c.html.
// Opaque structs keep libopenmpt optional at both build and runtime.
struct openmpt_module;
struct openmpt_module_initial_ctl;
namespace tempest {
struct Music::Impl {
    void *library = nullptr;
    openmpt_module *module = nullptr;
    bool paused = false;
    float gain = 0;
    using Log = void (*)(const char *, void *);
    using Error = int (*)(int, void *);
    openmpt_module *(*create)(const void *, size_t, Log, void *, Error, void *, int *,
                              const char **, const openmpt_module_initial_ctl *) = nullptr;
    void (*destroy)(openmpt_module *) = nullptr;
    size_t (*read)(openmpt_module *, int32_t, size_t, float *) = nullptr;
    int (*repeat)(openmpt_module *, int32_t) = nullptr;
    Impl() {
        library = dlopen("libopenmpt.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!library)
            return;
        create = reinterpret_cast<decltype(create)>(
            dlsym(library, "openmpt_module_create_from_memory2"));
        destroy = reinterpret_cast<decltype(destroy)>(dlsym(library, "openmpt_module_destroy"));
        read = reinterpret_cast<decltype(read)>(
            dlsym(library, "openmpt_module_read_interleaved_float_stereo"));
        repeat =
            reinterpret_cast<decltype(repeat)>(dlsym(library, "openmpt_module_set_repeat_count"));
        if (!create || !destroy || !read || !repeat) {
            dlclose(library);
            library = nullptr;
        }
    }
    ~Impl() {
        if (module)
            destroy(module);
        if (library)
            dlclose(library);
    }
};
Music::Music() : p(std::make_unique<Impl>()) {}
Music::~Music() = default;
bool Music::available() const {
    return p->library != nullptr;
}
bool Music::load(const std::string &path) {
    if (!available())
        return false;
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    std::vector<char> data((std::istreambuf_iterator<char>(file)), {});
    if (data.empty() || data.size() > 16 * 1024 * 1024)
        return false;
    auto silent = [](const char *, void *) {};
    auto *next = p->create(data.data(), data.size(), silent, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr);
    if (!next)
        return false;
    p->repeat(next, -1);
    if (p->module)
        p->destroy(p->module);
    p->module = next;
    p->gain = 0;
    return true;
}
void Music::pause(bool value) {
    p->paused = value;
}
void Music::render(float *stereo, size_t frames) {
    std::fill_n(stereo, frames * 2, 0.f);
    if (!p->module || p->paused)
        return;
    size_t count = p->read(p->module, 48000, frames, stereo);
    for (size_t i = 0; i < count; i++) {
        p->gain = std::min(.85f, p->gain + .85f / (48000 * .18f));
        stereo[i * 2] *= p->gain;
        stereo[i * 2 + 1] *= p->gain;
    }
}
const char *Music::trackForLevel(int level, bool title) {
    if (title)
        return "tune13.mod";
    // The original webtunes bytes 5,2,6,7 select 16-web blocks (lsr #4).
    constexpr const char *tracks[] = {"rave4.mod", "tune7.mod", "tune5.mod", "tune12.mod"};
    return tracks[(std::max(1, level) - 1) / 16 % 4];
}
} // namespace tempest

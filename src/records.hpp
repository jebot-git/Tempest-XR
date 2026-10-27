#pragma once
#include "game.hpp"
#include <string>
namespace tempest {
bool loadRecords(const std::string &path, Game &game);
bool saveRecords(const std::string &path, Game &game);
} // namespace tempest

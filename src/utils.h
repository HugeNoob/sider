#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using TimeStamp = std::optional<std::chrono::time_point<std::chrono::system_clock>>;
using TimeStampedStringMap = std::unordered_map<std::string, std::pair<std::string, TimeStamp>>;

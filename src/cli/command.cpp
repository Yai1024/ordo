#include "ordo/cli/command.h"

#include <utility>

namespace ordo {

bool Args::has_flag(std::string_view name) const {
    return flags.find(std::string{name}) != flags.end();
}

std::optional<std::string> Args::flag(std::string_view name) const {
    const auto found = flags.find(std::string{name});
    if (found == flags.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::string Args::flag_or(std::string_view name, std::string fallback) const {
    const auto found = flags.find(std::string{name});
    if (found == flags.end()) {
        return fallback;
    }
    return found->second;
}

}  // namespace ordo

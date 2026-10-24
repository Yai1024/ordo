#include "ordo/core/result.h"

namespace ordo {

std::string_view error_code_name(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:
            return "ok";
        case ErrorCode::NotFound:
            return "not_found";
        case ErrorCode::Duplicate:
            return "duplicate";
        case ErrorCode::InvalidFormat:
            return "invalid_format";
        case ErrorCode::InvalidValue:
            return "invalid_value";
        case ErrorCode::EmptyInput:
            return "empty_input";
        case ErrorCode::IoError:
            return "io_error";
        case ErrorCode::Corrupted:
            return "corrupted";
    }
    return "unknown";
}

}  // namespace ordo

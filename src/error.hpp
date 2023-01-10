#pragma once

#include <system_error>

namespace adb {

enum adb_errors {
    /// Failed to retrieve version from ADB server.
    server_unavailable = 1,

    /// OKAY message was not received after push.
    push_unacknowledged = 2
};

std::error_code make_error_code(adb_errors condition);
std::error_code make_error_code(const std::string_view message);

} // namespace adb

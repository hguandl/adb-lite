#include <iomanip>
#include <iostream>

#include "error.hpp"
#include "protocol.hpp"

using asio::ip::tcp;

namespace adb::protocol {

/// Encoded the ADB host request.
/**
 * @param body Body of the request.
 * @return Encoded request.
 */
static inline std::string host_request(const std::string_view body) {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << std::hex << body.size() << body;
    return ss.str();
}

/// Receive and check the response.
/**
 * @param socket Opened adb connection.
 * @return `expected` if the response is OKAY, `unexpected` otherwise.
 */
static inline expected<> host_response(tcp::socket& socket) {
    std::error_code ec;
    std::array<char, 4> header;

    socket.read_some(asio::buffer(header), ec);
    if (ec) {
        return unexpected(ec);
    }

    const auto result = std::string_view(header.data(), 4);
    if (result == "OKAY") {
        return {};
    }

    if (result != "FAIL") {
        ec = make_error_code("unknown response");
        return unexpected(ec);
    }

    const auto failure = host_message(socket);
    return {};
}

expected<std::string> host_message(tcp::socket& socket) {
    std::error_code ec;
    std::array<char, 4> header;

    socket.read_some(asio::buffer(header), ec);
    if (ec) {
        return unexpected(ec);
    }
    auto remain = std::stoull(std::string(header.data(), 4), nullptr, 16);

    std::string message;
    std::array<char, 1024> buffer;
    while (remain > 0) {
        const auto length = socket.read_some(asio::buffer(buffer), ec);
        if (ec) {
            return unexpected(ec);
        }
        message.append(buffer.data(), length);
        remain -= length;
    }

    return message;
}

expected<std::string> host_data(tcp::socket& socket) {
    std::string data;
    std::array<char, 1024> buffer;
    asio::error_code ec;

    while (!ec) {
        const auto length = socket.read_some(asio::buffer(buffer), ec);
        if (ec == asio::error::eof) {
            break;
        }
        if (ec) {
            return unexpected(ec);
        }
        data.append(buffer.data(), length);
    }

    return data;
}

std::string sync_request(const std::string_view id, const uint32_t length) {
    const auto len = {
        static_cast<char>(length & 0xff),
        static_cast<char>((length >> 8) & 0xff),
        static_cast<char>((length >> 16) & 0xff),
        static_cast<char>((length >> 24) & 0xff),
    };

    return std::string(id) + std::string(len.begin(), len.end());
}

expected<> sync_response(tcp::socket& socket, std::string& id,
                         uint32_t& length) {
    std::error_code ec;
    std::array<char, 8> response;

    socket.read_some(asio::buffer(response), ec);
    if (ec) {
        return unexpected(ec);
    }

    id = std::string(response.data(), 4);
    length = response[4] | (response[5] << 8) | (response[6] << 16) |
             (response[7] << 24);

    return {};
}

expected<> send_host_request(tcp::socket& socket,
                             const std::string_view request) {
    std::error_code ec;
    socket.write_some(asio::buffer(host_request(request)), ec);
    if (ec) {
        return unexpected(ec);
    }

    auto result = host_response(socket);
    if (result) {
        return {};
    } else {
        return unexpected(result.error());
    }
}

expected<> send_sync_request(tcp::socket& socket, const std::string_view id,
                             uint32_t length, const char* body) {
    std::error_code ec;
    auto data_request = sync_request(id, length);

    socket.write_some(asio::buffer(data_request), ec);
    if (ec) {
        return unexpected(ec);
    }
    socket.write_some(asio::buffer(body, length), ec);
    if (ec) {
        return unexpected(ec);
    }

    return {};
}

} // namespace adb::protocol

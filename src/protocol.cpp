#include <fstream>
#include <iomanip>

#include "protocol.hpp"

namespace adb::protocol {

/// Adb host endpoint, which is `127.0.0.1:5037`.
static inline const asio::ip::tcp::endpoint host_endpoint = [] {
    auto localhost = asio::ip::address_v4({127, 0, 0, 1});
    return asio::ip::tcp::endpoint(localhost, 5037);
}();

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

/// Encode the ADB sync request.
/**
 * @param id 4-byte string of the request id.
 * @param length Length of the body.
 */
static inline std::string sync_request(const std::string_view id,
                                       const uint32_t length) {
    const auto len = {
        static_cast<char>(length & 0xff),
        static_cast<char>((length >> 8) & 0xff),
        static_cast<char>((length >> 16) & 0xff),
        static_cast<char>((length >> 24) & 0xff),
    };

    return std::string(id) + std::string(len.begin(), len.end());
}

async_handle::async_handle(asio::io_context& context)
    : m_context(context), m_socket(context) {
    m_buffer = std::make_unique<std::array<char, buf_size>>();
    m_file = nullptr;
}

std::string async_handle::value() const { return m_data; }

std::error_code async_handle::error() const { return m_error; }

#define CB this, callback = std::move(callback)
#define TOKEN const auto& ec
#define TOKEN1 const auto &ec, auto
#define TOKEN2 const auto &ec, auto size

void async_handle::connect(const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    m_socket.async_connect(host_endpoint, [CB](TOKEN) {
        m_error = ec;
        callback();
    });
}

void async_handle::host_request(const std::string_view request,
                                const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    const auto data = ::adb::protocol::host_request(request);
    m_socket.async_write_some(asio::buffer(data), [CB](TOKEN1) {
        if (ec) {
            m_error = ec;
            callback();
            return;
        }

        host_response(std::move(callback));
    });
}

void async_handle::host_response(const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    m_socket.async_read_some(asio::buffer(m_header), [CB](TOKEN1) {
        if (ec) {
            m_error = ec;
            callback();
            return;
        }

        const auto result = std::string_view(m_header.data(), 4);
        if (result == "OKAY") {
            callback();
            return;
        }

        if (result != "FAIL") {
            m_error = asio::error::invalid_argument;
            callback();
            return;
        }

        m_error = asio::error::fault;
        host_message(std::move(callback));
    });
}

void async_handle::host_message(const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    m_socket.async_read_some(asio::buffer(*m_buffer), [CB](TOKEN2) {
        if (ec) {
            m_error = ec;
            callback();
            return;
        }

        if (size < 4) {
            m_error = asio::error::invalid_argument;
            callback();
            return;
        }

        m_data_size = std::stoul(std::string(m_buffer->data(), 4), nullptr, 16);

        if (size > 4) {
            m_data.append(m_buffer->data() + 4, size - 4);
            if (m_data.size() >= m_data_size) {
                callback();
                return;
            }
        }

        host_read_data(std::move(callback));
    });
}

void async_handle::host_data(const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    m_socket.async_read_some(asio::buffer(*m_buffer), [CB](TOKEN2) {
        if (ec == asio::error::eof) {
            callback();
            return;
        }

        if (ec) {
            m_error = ec;
            callback();
            return;
        }

        m_data.append(m_buffer->data(), size);
        host_data(std::move(callback));
    });
}

void async_handle::sync_request(const std::string_view id,
                                const uint32_t length, const char* body,
                                const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    auto header = ::adb::protocol::sync_request(id, length);
    m_socket.async_write_some(asio::buffer(header), [=, CB](TOKEN1) {
        if (ec || body == nullptr) {
            m_error = ec;
            callback();
            return;
        }

        auto buffers = asio::buffer(body, length);
        m_socket.async_write_some(buffers, [CB](TOKEN2) {
            m_error = ec;
            m_buffer_ptr += size;
            callback();
        });
    });
}

void async_handle::sync_response(const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    m_socket.async_read_some(asio::buffer(*m_buffer), [CB](TOKEN2) {
        if (ec) {
            m_error = ec;
            callback();
            return;
        }

        if (size < 4) {
            m_error = asio::error::invalid_argument;
            callback();
            return;
        }

        const auto id = std::string(m_buffer->data(), 4);
        m_data = std::move(id);
        callback();
    });
}

void async_handle::sync_send_file(const std::filesystem::path& path,
                                  const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    m_file = std::make_unique<std::ifstream>(path, std::ios::binary);
    m_buffer_ptr = m_buffer_size = 0;
    sync_write_data(std::move(callback));
}

void async_handle::run(const int64_t timeout) {
    auto future = m_promise.get_future();
    auto status = future.wait_for(std::chrono::milliseconds(timeout));

    if (status == std::future_status::timeout) {
        m_socket.cancel(m_error);
        m_error = asio::error::timed_out;
    }
}

void async_handle::finish() { m_promise.set_value(); }

void async_handle::host_read_data(const callback_t&& callback) {
    if (m_error) {
        callback();
        return;
    }

    m_socket.async_read_some(asio::buffer(*m_buffer), [CB](TOKEN2) {
        if (ec) {
            m_error = ec;
            callback();
            return;
        }

        m_data.append(m_buffer->data(), size);
        if (m_data.size() >= m_data_size) {
            callback();
        } else {
            host_read_data(std::move(callback));
        }
    });
}

void async_handle::sync_write_data(const callback_t&& callback) {
    if (m_error || m_file->eof()) {
        m_file = nullptr;
        callback();
        return;
    }

    if (m_buffer_ptr < m_buffer_size) {
        const auto body = m_buffer->data() + m_buffer_ptr;
        const auto length = m_buffer_size - m_buffer_ptr;

        m_socket.async_write_some(asio::buffer(body, length), [CB](TOKEN2) {
            m_error = ec;
            m_buffer_ptr += size;
            sync_write_data(std::move(callback));
        });

        return;
    }

    m_file->read(m_buffer->data(), buf_size);

    m_buffer_ptr = 0;
    m_buffer_size = static_cast<uint32_t>(m_file->gcount());

    // DATA request: file data trunk, trunk size
    sync_request("DATA", m_buffer_size, m_buffer->data(),
                 [CB] { sync_write_data(std::move(callback)); });
}

} // namespace adb::protocol

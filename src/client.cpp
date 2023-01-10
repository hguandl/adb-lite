#include <fstream>
#include <iostream>
#include <thread>

#include "client.hpp"
#include "error.hpp"
#include "protocol.hpp"

using asio::ip::tcp;

using adb::protocol::send_host_request;
using adb::protocol::send_sync_request;

namespace adb {

static inline expected<std::string> version(asio::io_context& context,
                                            const tcp::endpoint& endpoint) {
    std::error_code ec;
    tcp::socket socket(context);

    socket.connect(endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    const auto request = "host:version";
    const auto result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    auto message = protocol::host_message(socket);
    socket.close();
    return message;
}

static inline expected<std::string> devices(asio::io_context& context,
                                            const tcp::endpoint& endpoint) {
    {
        const auto result = version(context, endpoint);
        if (!result) {
            return unexpected(result.error());
        }
    }

    std::error_code ec;
    tcp::socket socket(context);
    socket.connect(endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    const auto request = "host:devices";
    const auto result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    auto message = protocol::host_message(socket);
    socket.close();
    return message;
}

expected<std::string> version() {
    std::error_code ec;
    asio::io_context context;
    tcp::resolver resolver(context);

    const auto endpoints = resolver.resolve("127.0.0.1", "5037", ec);
    if (ec) {
        return unexpected(ec);
    }

    return version(context, endpoints->endpoint());
}

expected<std::string> devices() {
    std::error_code ec;
    asio::io_context context;
    tcp::resolver resolver(context);

    const auto endpoints = resolver.resolve("127.0.0.1", "5037", ec);
    if (ec) {
        return unexpected(ec);
    }

    return devices(context, endpoints->endpoint());
}

class io_handle_impl : public io_handle {
  public:
    io_handle_impl(asio::io_context& context, asio::ip::tcp::socket socket);
    ~io_handle_impl() { close(); }
    expected<> write(const std::string_view data) override;
    expected<std::string> read() override;
    expected<std::string> read(unsigned timeout) override;
    expected<> close() override;

  private:
    friend class client_impl;

    asio::io_context& m_context;
    asio::ip::tcp::socket m_socket;
};

io_handle_impl::io_handle_impl(asio::io_context& context, tcp::socket socket)
    : m_context(context), m_socket(std::move(socket)) {
    asio::socket_base::keep_alive option(true);
    m_socket.set_option(option);
}

expected<std::string> io_handle_impl::read(unsigned timeout) {
    std::error_code ec;
    std::array<char, 1024> buffer;

    size_t bytes_read = 0;
    asio::steady_timer timer(m_context, std::chrono::seconds(timeout));

    m_socket.async_read_some(
        asio::buffer(buffer),
        [&](const asio::error_code& error, size_t bytes_transferred) {
            if (error) {
                ec = error;
                return;
            }
            bytes_read = bytes_transferred;
            timer.cancel();
        });

    timer.async_wait([&](const asio::error_code& error) {
        if (error) {
            ec = error;
            return;
        }
        m_socket.cancel(ec);
        ec = asio::error::timed_out;
    });

    m_context.restart();
    while (m_context.run_one()) {
        if (ec) {
            return unexpected(ec);
        }
        if (bytes_read > 0) {
            break;
        }
    }

    return std::string(buffer.data(), bytes_read);
}

expected<std::string> io_handle_impl::read() {
    std::error_code ec;
    std::array<char, 1024> buffer;

    const auto bytes_read = m_socket.read_some(asio::buffer(buffer), ec);
    if (ec) {
        return unexpected(ec);
    }

    return std::string(buffer.data(), bytes_read);
}

expected<> io_handle_impl::write(const std::string_view data) {
    std::error_code ec;
    m_socket.write_some(asio::buffer(data), ec);
    if (ec) {
        return unexpected(ec);
    }
    return {};
}

expected<> io_handle_impl::close() {
    std::error_code ec;
    m_socket.close(ec);
    if (ec) {
        return unexpected(ec);
    }
    return {};
}

class client_impl : public client {
  public:
    client_impl(const std::string_view serial);
    expected<std::string> connect() override;
    expected<std::string> disconnect() override;
    expected<std::string> version() override;
    expected<std::string> devices() override;
    expected<std::string> shell(const std::string_view command) override;
    expected<std::string> exec(const std::string_view command) override;
    expected<> push(const std::string_view src, const std::string_view dst,
                    int perm) override;
    expected<std::shared_ptr<io_handle>>
    interactive_shell(const std::string_view command) override;
    expected<std::string> root() override;
    expected<std::string> unroot() override;
    expected<> wait_for_device() override;

  private:
    friend class client;

    const std::string m_serial;
    asio::io_context m_context;
    asio::ip::basic_endpoint<asio::ip::tcp> m_endpoint;

    bool check_adb_availabilty();

    /// Switch the connection to the device.
    /**
     * @param socket Opened adb connection.
     * @note Should be used only by the client class.
     * @note Local services (e.g. shell, push) can be requested after this.
     */
    expected<> switch_to_device(asio::ip::tcp::socket& socket);
};

std::shared_ptr<client> client::create(const std::string_view serial) {
    return std::make_shared<client_impl>(serial);
}

client_impl::client_impl(const std::string_view serial) : m_serial(serial) {
    tcp::resolver resolver(m_context);
    auto endpoints = resolver.resolve("127.0.0.1", "5037");
    m_endpoint = endpoints->endpoint();
}

expected<std::string> client_impl::connect() {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    tcp::socket socket(m_context);
    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    const auto request = "host:connect:" + m_serial;
    const auto result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    auto message = protocol::host_message(socket);
    socket.close(ec);

    return message;
}

expected<std::string> client_impl::disconnect() {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    tcp::socket socket(m_context);
    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    const auto request = "host:disconnect:" + m_serial;
    const auto result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    auto message = protocol::host_message(socket);
    socket.close(ec);
    return message;
}

expected<std::string> client_impl::version() {
    return adb::version(m_context, m_endpoint);
}

expected<std::string> client_impl::devices() {
    return adb::devices(m_context, m_endpoint);
}

expected<std::string> client_impl::shell(const std::string_view command) {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    tcp::socket socket(m_context);
    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    auto result = switch_to_device(socket);
    if (!result) {
        return unexpected(result.error());
    }

    const auto request = std::string("shell:") + command.data();
    result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    const auto data = protocol::host_data(socket);

    socket.close(ec);
    return data;
}

expected<std::string> client_impl::exec(const std::string_view command) {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    tcp::socket socket(m_context);

    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    auto result = switch_to_device(socket);
    if (!result) {
        return unexpected(result.error());
    }

    const auto request = std::string("exec:") + command.data();
    result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    const auto data = protocol::host_data(socket);

    socket.close();
    return data;
}

expected<> client_impl::push(const std::string_view src,
                             const std::string_view dst, int perm) {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    tcp::socket socket(m_context);
    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    auto result = switch_to_device(socket);
    if (!result) {
        return unexpected(result.error());
    }

    // Switch to sync mode
    const auto sync = "sync:";
    result = send_host_request(socket, sync);
    if (!result) {
        return unexpected(result.error());
    }

    // SEND request: destination, permissions
    const auto send_request = std::string(dst) + "," + std::to_string(perm);
    const auto send_size = static_cast<uint32_t>(send_request.size());
    result = send_sync_request(socket, "SEND", send_size, send_request.data());
    if (!result) {
        return unexpected(result.error());
    }

    // DATA request: file data trunk, trunk size
    std::ifstream file(src.data(), std::ios::binary);
    const auto buf_size = 64000;
    std::array<char, buf_size> buffer;
    while (!file.eof()) {
        file.read(buffer.data(), buf_size);
        const auto bytes_read = static_cast<uint32_t>(file.gcount());
        result = send_sync_request(socket, "DATA", bytes_read, buffer.data());
        if (!result) {
            return unexpected(result.error());
        }
    }

    // DONE request: timestamp
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now).count();
    const auto done_request =
        protocol::sync_request("DONE", static_cast<uint32_t>(timestamp));
    socket.write_some(asio::buffer(done_request), ec);
    if (ec) {
        return unexpected(ec);
    }

    std::string sync_result;
    uint32_t result_length;
    result = protocol::sync_response(socket, sync_result, result_length);
    if (!result) {
        return unexpected(result.error());
    }

    if (sync_result != "OKAY") {
        return unexpected(make_error_code(adb_errors::push_unacknowledged));
    }

    socket.close(ec);
    return {};
}

expected<std::string> client_impl::root() {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    tcp::socket socket(m_context);
    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    auto result = switch_to_device(socket);
    if (!result) {
        return unexpected(result.error());
    }

    const auto request = "root:";
    result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    auto message = protocol::host_data(socket);
    socket.close(ec);
    return message;
}

expected<std::string> client_impl::unroot() {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    tcp::socket socket(m_context);
    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    auto result = switch_to_device(socket);
    if (!result) {
        return unexpected(result.error());
    }

    const auto request = "unroot:";
    result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    auto message = protocol::host_data(socket);
    socket.close(ec);
    return message;
}

expected<std::shared_ptr<io_handle>>
client_impl::interactive_shell(const std::string_view command) {
    if (!check_adb_availabilty()) {
        return unexpected(make_error_code(adb_errors::server_unavailable));
    }

    std::error_code ec;
    asio::io_context context;
    tcp::socket socket(context);
    socket.connect(m_endpoint, ec);
    if (ec) {
        return unexpected(ec);
    }

    auto result = switch_to_device(socket);
    if (!result) {
        return unexpected(result.error());
    }

    const auto request = std::string("shell:") + command.data();
    result = send_host_request(socket, request);
    if (!result) {
        return unexpected(result.error());
    }

    return std::make_shared<io_handle_impl>(context, std::move(socket));
}

expected<> client_impl::wait_for_device() {
    // If adbd restarts, we should wait the device to get offline first.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const auto pattern = m_serial + "\tdevice";
    while (true) {
        const auto result = devices();
        if (result) {
            if (result.value().find(pattern) != std::string::npos) {
                return {};
            }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        } else {
            return unexpected(result.error());
        }
    }
}

bool client_impl::check_adb_availabilty() { return version().has_value(); }

expected<> client_impl::switch_to_device(tcp::socket& socket) {
    const auto request = "host:transport:" + m_serial;
    return send_host_request(socket, request);
}

} // namespace adb

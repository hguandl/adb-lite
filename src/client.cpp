#include <fstream>
#include <iostream>
#include <thread>

#include <asio.hpp>

#include "client_impl.hpp"
#include "io_handle_impl.hpp"
#include "protocol.hpp"

using asio::ip::tcp;
using tcp_endpoints = tcp::resolver::results_type;

using adb::protocol::send_host_request;
using adb::protocol::send_sync_request;

namespace adb {

static inline std::string version(asio::io_context& context,
                                  const tcp_endpoints& endpoints) {
    tcp::socket socket(context);
    asio::connect(socket, endpoints);

    const auto request = "host:version";
    send_host_request(socket, request);

    return protocol::host_message(socket);
}

static inline std::string devices(asio::io_context& context,
                                  const tcp_endpoints& endpoints) {
    tcp::socket socket(context);
    asio::connect(socket, endpoints);

    const auto request = "host:devices";
    send_host_request(socket, request);

    return protocol::host_message(socket);
}

std::string version() {
    asio::io_context context;
    tcp::resolver resolver(context);
    const auto endpoints = resolver.resolve("127.0.0.1", "5037");
    return version(context, endpoints);
}

std::string devices() {
    asio::io_context context;
    tcp::resolver resolver(context);
    const auto endpoints = resolver.resolve("127.0.0.1", "5037");
    return devices(context, endpoints);
}

void kill_server() {
    asio::io_context context;
    tcp::resolver resolver(context);
    const auto endpoints = resolver.resolve("127.0.0.1", "5037");

    tcp::socket socket(context);
    asio::connect(socket, endpoints);

    const auto request = "host:kill";
    send_host_request(socket, request);
}

std::shared_ptr<client> client::create(const std::string_view serial) {
    return std::make_shared<client_impl>(serial);
}

client_impl::client_impl(const std::string_view serial) {
    m_serial = serial;

    tcp::resolver resolver(m_context);
    m_endpoints = resolver.resolve("127.0.0.1", "5037");
}

std::string client_impl::connect() {
    tcp::socket socket(m_context);
    asio::connect(socket, m_endpoints);

    const auto request = "host:connect:" + m_serial;
    send_host_request(socket, request);

    return protocol::host_message(socket);
}

std::string client_impl::disconnect() {
    tcp::socket socket(m_context);
    asio::connect(socket, m_endpoints);

    const auto request = "host:disconnect:" + m_serial;
    send_host_request(socket, request);

    return protocol::host_message(socket);
}

std::string client_impl::version() {
    return adb::version(m_context, m_endpoints);
}

std::string client_impl::devices() {
    return adb::devices(m_context, m_endpoints);
}

std::string client_impl::shell(const std::string_view command) {
    tcp::socket socket(m_context);
    asio::connect(socket, m_endpoints);

    switch_to_device(socket);

    const auto request = std::string("shell:") + command.data();
    send_host_request(socket, request);

    return protocol::host_data(socket);
}

std::string client_impl::exec(const std::string_view command) {
    tcp::socket socket(m_context);
    asio::connect(socket, m_endpoints);

    switch_to_device(socket);

    const auto request = std::string("exec:") + command.data();
    send_host_request(socket, request);

    return protocol::host_data(socket);
}

bool client_impl::push(const std::string& src, const std::string& dst,
                       int perm) {
    tcp::socket socket(m_context);
    asio::connect(socket, m_endpoints);

    switch_to_device(socket);

    // Switch to sync mode
    const auto sync = "sync:";
    send_host_request(socket, sync);

    // SEND request: destination, permissions
    const auto send_request = dst + "," + std::to_string(perm);
    const auto request_size = static_cast<uint32_t>(send_request.size());
    send_sync_request(socket, "SEND", request_size, send_request.data());

    // DATA request: file data trunk, trunk size
    std::ifstream file(src.c_str(), std::ios::binary);
    const auto buf_size = 64000;
    std::array<char, buf_size> buffer;
    while (!file.eof()) {
        file.read(buffer.data(), buf_size);
        const auto bytes_read = static_cast<uint32_t>(file.gcount());
        send_sync_request(socket, "DATA", bytes_read, buffer.data());
    }
    file.close();

    // DONE request: timestamp
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now).count();
    const auto done_request =
        protocol::sync_request("DONE", static_cast<uint32_t>(timestamp));
    socket.write_some(asio::buffer(done_request));

    std::string result;
    uint32_t length;
    protocol::sync_response(socket, result, length);
    if (result != "OKAY") {
        return false;
    }

    return true;
}

std::string client_impl::root() {
    tcp::socket socket(m_context);
    asio::connect(socket, m_endpoints);

    switch_to_device(socket);

    const auto request = "root:";
    send_host_request(socket, request);

    return protocol::host_data(socket);
}

std::string client_impl::unroot() {
    tcp::socket socket(m_context);
    asio::connect(socket, m_endpoints);

    switch_to_device(socket);

    const auto request = "unroot:";
    send_host_request(socket, request);

    return protocol::host_data(socket);
}

std::shared_ptr<io_handle>
client_impl::interactive_shell(const std::string_view command) {
    auto context = std::make_unique<asio::io_context>();
    tcp::socket socket(*context);
    asio::connect(socket, m_endpoints);

    switch_to_device(socket);

    const auto request = std::string("shell:") + command.data();
    send_host_request(socket, request);

    return std::make_shared<io_handle_impl>(std::move(context),
                                            std::move(socket));
}

void client_impl::wait_for_device() {
    // If adbd restarts, we should wait the device to get offline first.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const auto pattern = m_serial + "\tdevice";
    while (devices().find(pattern) == std::string::npos) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void client_impl::switch_to_device(tcp::socket& socket) {
    const auto request = "host:transport:" + m_serial;
    send_host_request(socket, request);
}

} // namespace adb

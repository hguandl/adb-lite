#include <fstream>
#include <iostream>
#include <thread>

#include "client.hpp"
#include "protocol.hpp"

using asio::ip::tcp;

using adb::protocol::send_host_request;
using adb::protocol::send_sync_request;

namespace adb {

static std::string version(asio::io_context& context, tcp::endpoint& endpoint) {
    tcp::socket socket(context);
    socket.connect(endpoint);

    const auto request = "host:version";
    send_host_request(socket, request);

    auto message = protocol::host_message(socket);
    socket.close();
    return message;
}

static std::string devices(asio::io_context& context, tcp::endpoint& endpoint) {
    version(context, endpoint);

    tcp::socket socket(context);
    socket.connect(endpoint);

    const auto request = "host:devices";
    send_host_request(socket, request);

    auto message = protocol::host_message(socket);
    socket.close();
    return message;
}

std::string version() {
    asio::io_context context;
    tcp::resolver resolver(context);
    auto endpoint = resolver.resolve("127.0.0.1", "5037")->endpoint();
    return version(context, endpoint);
}

std::string devices() {
    asio::io_context context;
    tcp::resolver resolver(context);
    auto endpoint = resolver.resolve("127.0.0.1", "5037")->endpoint();
    return devices(context, endpoint);
}

class io_handle_impl : public io_handle {
  public:
    io_handle_impl(asio::ip::tcp::socket socket);
    ~io_handle_impl() { close(); }
    void write(const std::string_view data) override;
    std::string read() override;
    void close() override;

  private:
    friend class client_impl;

    asio::ip::tcp::socket m_socket;
};

io_handle_impl::io_handle_impl(tcp::socket socket)
    : m_socket(std::move(socket)) {
    asio::socket_base::keep_alive option(true);
    m_socket.set_option(option);
}

std::string io_handle_impl::read() {
    std::array<char, 1024> buffer;
    const auto bytes_read = m_socket.read_some(asio::buffer(buffer));
    return std::string(buffer.data(), bytes_read);
}

void io_handle_impl::write(const std::string_view data) {
    m_socket.write_some(asio::buffer(data));
}

void io_handle_impl::close() { m_socket.close(); }

class client_impl : public client {
  public:
    client_impl(const std::string_view serial);
    std::string connect() override;
    std::string disconnect() override;
    std::string version() override;
    std::string devices() override;
    std::string shell(const std::string_view command) override;
    std::string exec(const std::string_view command) override;
    void push(const std::string_view src, const std::string_view dst,
              int perm) override;
    std::shared_ptr<io_handle>
    interactive_shell(const std::string_view command) override;
    std::string root() override;
    std::string unroot() override;
    void wait_for_device() override;

  private:
    friend class client;

    std::string m_serial;
    asio::io_context m_context;
    asio::ip::basic_endpoint<asio::ip::tcp> m_endpoint;

    void check_adb_availabilty();

    /// Switch the connection to the device.
    /**
     * @param socket Opened adb connection.
     * @note Should be used only by the client class.
     * @note Local services (e.g. shell, push) can be requested after this.
     */
    void switch_to_device(asio::ip::tcp::socket& socket);
};

std::shared_ptr<client> client::create(const std::string_view serial) {
    return std::make_shared<client_impl>(serial);
}

client_impl::client_impl(const std::string_view serial) {
    m_serial = serial;

    tcp::resolver resolver(m_context);
    auto endpoints = resolver.resolve("127.0.0.1", "5037");
    m_endpoint = endpoints->endpoint();
}

std::string client_impl::connect() {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    const auto request = "host:connect:" + m_serial;
    send_host_request(socket, request);

    auto message = protocol::host_message(socket);
    socket.close();

    return message;
}

std::string client_impl::disconnect() {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    const auto request = "host:disconnect:" + m_serial;
    send_host_request(socket, request);

    auto message = protocol::host_message(socket);
    socket.close();
    return message;
}

std::string client_impl::version() {
    return adb::version(m_context, m_endpoint);
}

std::string client_impl::devices() {
    return adb::devices(m_context, m_endpoint);
}

std::string client_impl::shell(const std::string_view command) {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    switch_to_device(socket);

    const auto request = std::string("shell:") + command.data();
    send_host_request(socket, request);

    const auto data = protocol::host_data(socket);

    socket.close();
    return data;
}

std::string client_impl::exec(const std::string_view command) {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    switch_to_device(socket);

    const auto request = std::string("exec:") + command.data();
    send_host_request(socket, request);

    const auto data = protocol::host_data(socket);

    socket.close();
    return data;
}

void client_impl::push(const std::string_view src, const std::string_view dst,
                       int perm) {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    switch_to_device(socket);

    // Switch to sync mode
    const auto sync = "sync:";
    send_host_request(socket, sync);

    // SEND request: destination, permissions
    const auto send_request = std::string(dst) + "," + std::to_string(perm);
    const auto request_size = static_cast<uint32_t>(send_request.size());
    send_sync_request(socket, "SEND", request_size, send_request.data());

    // DATA request: file data trunk, trunk size
    std::ifstream file(src.data(), std::ios::binary);
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
        throw std::runtime_error("failed to push file");
    }

    socket.close();
}

std::string client_impl::root() {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    switch_to_device(socket);

    const auto request = "root:";
    send_host_request(socket, request);

    auto message = protocol::host_data(socket);
    socket.close();
    return message;
}

std::string client_impl::unroot() {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    switch_to_device(socket);

    const auto request = "unroot:";
    send_host_request(socket, request);

    auto message = protocol::host_data(socket);
    socket.close();
    return message;
}

std::shared_ptr<io_handle>
client_impl::interactive_shell(const std::string_view command) {
    check_adb_availabilty();

    tcp::socket socket(m_context);
    socket.connect(m_endpoint);

    switch_to_device(socket);

    const auto request = std::string("shell:") + command.data();
    send_host_request(socket, request);

    return std::make_shared<io_handle_impl>(std::move(socket));
}

void client_impl::wait_for_device() {
    // If adbd restarts, we should wait the device to get offline first.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const auto pattern = m_serial + "\tdevice";
    while (devices().find(pattern) == std::string::npos) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void client_impl::check_adb_availabilty() { version(); }

void client_impl::switch_to_device(tcp::socket& socket) {
    const auto request = "host:transport:" + m_serial;
    send_host_request(socket, request);
}

} // namespace adb

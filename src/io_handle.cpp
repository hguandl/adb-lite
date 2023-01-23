#include <future>

#include "io_handle_impl.hpp"

adb::io_handle_impl::io_handle_impl(protocol::async_handle&& handle)
    : m_socket(std::move(handle.m_socket)) {
    asio::socket_base::keep_alive option(true);
    m_socket.set_option(option);
}

std::string adb::io_handle_impl::read(unsigned timeout) {
    std::array<char, 1024> buffer;

    if (timeout == 0) {
        const auto bytes_read = m_socket.read_some(asio::buffer(buffer));
        return std::string(buffer.data(), bytes_read);
    }

    std::error_code ec;
    size_t bytes_read = 0;
    std::promise<void> promise;

    const auto buffers = asio::buffer(buffer);
    m_socket.async_read_some(buffers, [&](const auto& error, auto size) {
        if (error == asio::error::operation_aborted) {
            return;
        }

        ec = error;
        bytes_read = size;
        promise.set_value();
    });

    auto future = promise.get_future();
    auto status = future.wait_for(std::chrono::milliseconds(timeout));
    if (status == std::future_status::timeout) {
        m_socket.cancel();
    }

    return std::string(buffer.data(), bytes_read);
}

void adb::io_handle_impl::write(const std::string_view data) {
    m_socket.write_some(asio::buffer(data));
}

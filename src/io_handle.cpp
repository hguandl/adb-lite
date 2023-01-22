#include "io_handle_impl.hpp"

using asio::ip::tcp;

adb::io_handle_impl::io_handle_impl(std::unique_ptr<asio::io_context> context,
                                    tcp::socket socket)
    : m_context(std::move(context)), m_socket(std::move(socket)) {
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
    asio::steady_timer timer(*m_context, std::chrono::seconds(timeout));

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
    });

    m_context->restart();
    while (m_context->run_one()) {
        if (ec == asio::error::eof) {
            break;
        } else if (ec == asio::error::operation_aborted) {
            break;
        } else if (ec) {
            throw std::system_error(ec);
        }
    }

    return std::string(buffer.data(), bytes_read);
}

void adb::io_handle_impl::write(const std::string_view data) {
    m_socket.write_some(asio::buffer(data));
}

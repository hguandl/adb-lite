#pragma once

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>

#include "client.hpp"
#include "protocol.hpp"

namespace adb {

/// Pimpl class for client.
class client_impl : public client {
  public:
    client_impl(const std::string_view serial);
    ~client_impl() { stop(); }

    std::string connect(std::error_code& ec, const int64_t timeout) override;
    std::string disconnect(std::error_code& ec, const int64_t timeout) override;

    std::string shell(const std::string_view command, std::error_code& ec,
                      const int64_t timeout, const bool recv_by_sock) override;
    std::string exec(const std::string_view command, std::error_code& ec,
                     const int64_t timeout, const bool recv_by_socket) override;

    bool push(const std::filesystem::path& src, const std::string& dst,
              int perm, std::error_code& ec, const int64_t timeout) override;

    std::shared_ptr<io_handle>
    interactive_shell(const std::string_view command, std::error_code& ec,
                      const int64_t timeout) override;

    std::string root(std::error_code& ec, const int64_t timeout) override;
    std::string unroot(std::error_code& ec, const int64_t timeout) override;

    void start() override;
    void stop() override;

    void wait_for_device(std::error_code& ec, const int64_t timeout) override;

  private:
    friend class client;

    const std::string m_serial;

    std::thread m_thread;
    asio::io_context m_context;
    asio::ip::tcp::acceptor m_acceptor;

    /// Convert the port of command to the client's one.
    std::string nc_command(const std::string_view command);
};

/// TCP connection class for receiving data by nc.
class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
  public:
    typedef std::shared_ptr<tcp_connection> pointer;
    typedef std::function<void(asio::error_code, std::string&&)> callback_t;

    static pointer create(asio::io_context& context) {
        return std::make_shared<tcp_connection>(context);
    }

    tcp_connection(asio::io_context& context) : m_socket(context) {
        m_buffer = std::make_unique<std::array<char, buf_size>>();
    }

    asio::ip::tcp::socket& socket() { return m_socket; }

    void start(const callback_t&& callback) {
        m_callback = std::move(callback);

        asio::async_read(m_socket, asio::buffer(*m_buffer),
                         std::bind(&tcp_connection::handle_read,
                                   shared_from_this(), std::placeholders::_1,
                                   std::placeholders::_2));
    }

    void cancel() {
        asio::error_code ec;
        m_socket.cancel(ec);
    }

  private:
    void handle_read(const asio::error_code& error, size_t bytes_transferred) {
        m_data.append(m_buffer->data(), bytes_transferred);

        if (error) {
            m_callback(error, std::move(m_data));
            return;
        }

        asio::async_read(m_socket, asio::buffer(*m_buffer),
                         std::bind(&tcp_connection::handle_read,
                                   shared_from_this(), std::placeholders::_1,
                                   std::placeholders::_2));
    }

    callback_t m_callback;

    asio::ip::tcp::socket m_socket;
    std::string m_data;

    static constexpr size_t buf_size = 64000;
    std::unique_ptr<std::array<char, buf_size>> m_buffer;
};

/// Client handle that use async methods to communicate with the adbd.
class client_handle : public protocol::async_handle {
  public:
    client_handle(asio::io_context& context);

    /// Request a host service on the adbd.
    void oneshot_request(const std::string_view request, const bool bounded,
                         const async_handle::callback_t&& callback);

    /// Switch the connection to the device.
    /**
     * @param serial Serial of the device.
     * @param callback Callback to be called when the connection is switched.
     * @note Local services (e.g. shell, push) can be requested after this.
     */
    void connect_device(const std::string_view serial,
                        const async_handle::callback_t&& callback);

    /// Request a host service on the adbd.
    /**
     * @return Response data of the host service.
     * @param request Request to be sent to the adbd.
     * @param bounded Whether the size is at the beginning of the response.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     */
    std::string timed_host_request(const std::string_view request,
                                   const bool bounded, std::error_code& ec,
                                   const int64_t timeout);

    /// Request a local service on the device.
    /**
     * @return Response data of the local service.
     * @param serial Serial of the device.
     * @param request Request to be sent to the device.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     */
    std::string timed_device_request(const std::string_view serial,
                                     const std::string_view request,
                                     std::error_code& ec,
                                     const int64_t timeout);

    /// Request a local service on the device and receive the response by nc.
    /**
     * @return Response data of the local service.
     * @param serial Serial of the device.
     * @param request Request to be sent to the device.
     * @param context io_context to be used for the connection.
     * @param acceptor Acceptor to be used for the connection.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     */
    std::string timed_device_request(const std::string_view serial,
                                     const std::string_view request,
                                     asio::io_context& context,
                                     asio::ip::tcp::acceptor& acceptor,
                                     std::error_code& ec,
                                     const int64_t timeout);
};

/// Stand-alone client handle that owns its an io_context.
class standalone_handle {
  public:
    standalone_handle() : m_handle(m_context){};

    /// Request a host service on the adbd.
    /**
     * @return Response data of the host service.
     * @param request Request to be sent to the adbd.
     * @param bounded Whether the size is at the beginning of the response.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     */
    std::string timed_host_request(const std::string_view request,
                                   const bool bounded, std::error_code& ec,
                                   const int64_t timeout);

  private:
    asio::io_context m_context;
    client_handle m_handle;
};

} // namespace adb

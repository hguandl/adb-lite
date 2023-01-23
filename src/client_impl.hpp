#pragma once

#include <asio/ip/tcp.hpp>

#include "client.hpp"
#include "protocol.hpp"

namespace adb {

/// Pimpl class for client.
class client_impl : public client {
  public:
    client_impl(const std::string_view serial) : m_serial(serial){};
    ~client_impl() { stop(); }

    std::string connect(std::error_code& ec, const int64_t timeout) override;
    std::string disconnect(std::error_code& ec, const int64_t timeout) override;

    std::string shell(const std::string_view command, std::error_code& ec,
                      const int64_t timeout) override;
    std::string exec(const std::string_view command, std::error_code& ec,
                     const int64_t timeout) override;

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

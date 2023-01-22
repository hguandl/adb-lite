#pragma once

#include <asio/ip/tcp.hpp>

#include "client.hpp"

namespace adb {
class client_impl : public client {
  public:
    client_impl(const std::string_view serial);
    std::string connect() override;
    std::string disconnect() override;
    std::string version() override;
    std::string devices() override;
    std::string shell(const std::string_view command) override;
    std::string exec(const std::string_view command) override;
    bool push(const std::string& src, const std::string& dst,
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
    asio::ip::tcp::resolver::results_type m_endpoints;

    /// Switch the connection to the device.
    /**
     * @param socket Opened adb connection.
     * @note Should be used only by the client class.
     * @note Local services (e.g. shell, push) can be requested after this.
     */
    void switch_to_device(asio::ip::tcp::socket& socket);
};
} // namespace adb

#pragma once

#include <asio/ip/tcp.hpp>

#include "io_handle.hpp"
#include "protocol.hpp"

namespace adb {

/// Pimpl class for io_handle.
class io_handle_impl : public io_handle {
  public:
    io_handle_impl(protocol::async_handle&& handle);

    void write(const std::string_view data) override;
    std::string read(unsigned timeout = 0) override;

  private:
    /// TCP connection to adbd.
    asio::ip::tcp::socket m_socket;
};

} // namespace adb

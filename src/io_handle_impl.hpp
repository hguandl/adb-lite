#pragma once

#include <asio/ip/tcp.hpp>

#include "io_handle.hpp"

namespace adb {

class io_handle_impl : public io_handle {
  public:
    io_handle_impl(std::unique_ptr<asio::io_context> context,
                   asio::ip::tcp::socket socket);
    void write(const std::string_view data) override;
    std::string read(unsigned timeout = 0) override;

  private:
    friend class client_impl;

    const std::unique_ptr<asio::io_context> m_context;
    asio::ip::tcp::socket m_socket;
};

} // namespace adb

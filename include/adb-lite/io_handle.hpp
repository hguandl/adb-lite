#pragma once

#include <string>
#include <string_view>

namespace adb {

/// Context for an interactive adb connection.
/**
 * @note Should be used only by the client class, after a shell request.
 * @note The socket will be closed when the handle is destroyed.
 */
class io_handle {
  public:
    virtual ~io_handle() = default;

    /// Write data to the adb connection.
    /**
     * @param data Data to write.
     * @throw std::runtime_error Thrown on socket failure.
     * @note Typically used to write stdin of a shell command.
     * @note The data should end with a newline.
     */
    virtual void write(const std::string_view data) = 0;

    /// Read data from the adb connection.
    /**
     * @param timeout Timeout in seconds. 0 means no timeout.
     * @return Data read. Empty if timeout or the connection is closed.
     * @throw std::runtime_error Thrown on socket failure.
     * @note Typically used to read stdout of a shell command.
     */
    virtual std::string read(unsigned timeout = 0) = 0;

  protected:
    io_handle() = default;
};

} // namespace adb

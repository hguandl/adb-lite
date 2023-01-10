#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "expected.hpp"

namespace adb {

/// Retrieve the version of local adb server.
/**
 * @return 4-byte string of the version number.
 */
expected<std::string> version();

/// Retrieve the available Android devices.
/**
 * @return A string of the list of devices.
 * @note Equivalent to `adb devices`.
 */
expected<std::string> devices();

class io_handle_impl;

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
     * @note Typically used to write stdin of a shell command.
     * @note The data should end with a newline.
     */
    virtual expected<> write(const std::string_view data) = 0;

    /// Read data from the adb connection.
    /**
     * @return Data read.
     * @note Typically used to read stdout of a shell command.
     * @note Function will be blocked until stdout produces more data.
     */
    virtual expected<std::string> read() = 0;

    /// Read data from the adb connection within timeout.
    /**
     * @return Data read.
     * @param timeout Timeout in seconds.
     * @note Typically used to read stdout of a shell command.
     */
    virtual expected<std::string> read(unsigned timeout) = 0;

    /// Close the adb connection.
    virtual expected<> close() = 0;

  protected:
    io_handle() = default;
};

class client_impl;

/// A client for the Android Debug Bridge.
class client {
  public:
    /// Create a client for a specific device.
    /**
     * @param serial serial number of the device.
     * @note If the serial is empty, the unique device will be used. If there
     * are multiple devices, an exception will be thrown.
     */
    static std::shared_ptr<client> create(const std::string_view serial);
    virtual ~client() = default;

    /// Connect to the device.
    /**
     * @return A string of the connection status.
     * @note Equivalent to `adb connect <serial>`.
     */
    virtual expected<std::string> connect() = 0;

    /// Disconnect from the device.
    /**
     * @return A string of the disconnection status.
     * @note Equivalent to `adb disconnect <serial>`.
     */
    virtual expected<std::string> disconnect() = 0;

    /// Retrieve the version of local adb server.
    /**
     * @return 4-byte string of the version number.
     * @note This function reuses the class member io_context, which is
     * thread-safe for the client.
     */
    virtual expected<std::string> version() = 0;

    /// Retrieve the available Android devices.
    /**
     * @return A string of the list of devices.
     * @note Equivalent to `adb devices`.
     * @note This function reuses the class member io_context, which is
     * thread-safe for the client.
     */
    virtual expected<std::string> devices() = 0;

    /// Send an one-shot shell command to the device.
    /**
     * @param command Command to execute.
     * @return A string of the command output.
     * @note Equivalent to `adb -s <serial> shell <command>` without stdin.
     */
    virtual expected<std::string> shell(const std::string_view command) = 0;

    /// Send an one-shot shell command to the device, using raw PTY.
    /**
     * @param command Command to execute.
     * @return A string of the command output, which is not mangled.
     * @note Equivalent to `adb -s <serial> exec-out <command>` without stdin.
     */
    virtual expected<std::string> exec(const std::string_view command) = 0;

    /// Send a file to the device.
    /**
     * @param src Path to the source file.
     * @param dst Path to the destination file.
     * @param perm Permission of the destination file.
     * @note Equivalent to `adb -s <serial> push <src> <dst>`.
     */
    virtual expected<> push(const std::string_view src,
                            const std::string_view dst, int perm) = 0;

    /// Set the user of adbd to root on the device.
    /**
     * @note Equivalent to `adb -s <serial> root`.
     * @note The device might be offline after this command. Remember to wait
     * for the restart.
     */
    virtual expected<std::string> root() = 0;

    /// Set the user of adbd to non-root on the device.
    /**
     * @note Equivalent to `adb -s <serial> unroot`.
     * @note The device might be offline after this command. Remember to wait
     * for the restart.
     */
    virtual expected<std::string> unroot() = 0;

    /// Start an interactive shell session on the device.
    /**
     * @param command Command to execute.
     * @return An io_handle for the interactive session.
     * @note Equivalent to `adb -s <serial> shell <command>` with stdin.
     */
    virtual expected<std::shared_ptr<io_handle>>
    interactive_shell(const std::string_view command) = 0;

    /// Wait for the device to be available.
    virtual expected<> wait_for_device() = 0;

  protected:
    client() = default;
};

} // namespace adb

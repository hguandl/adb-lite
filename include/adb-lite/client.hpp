#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include "io_handle.hpp"

namespace adb {

/// Retrieve the version of local adb server.
/**
 * @return 4-byte string of the version number.
 * @param ec std::error_code to indicate what error occurred, if any.
 * @param timeout Timeout in milliseconds.
 * @note Equivalent to `adb version`.
 */
std::string version(std::error_code& ec, const int64_t timeout);

/// Retrieve the available Android devices.
/**
 * @return A string of the list of devices.
 * @param ec std::error_code to indicate what error occurred, if any.
 * @param timeout Timeout in milliseconds.
 * @note Equivalent to `adb devices`.
 */
std::string devices(std::error_code& ec, const int64_t timeout);

/// Kill the adb server if it is running.
/**
 * @param ec std::error_code to indicate what error occurred, if any.
 * @param timeout Timeout in milliseconds.
 * @note Equivalent to `adb kill-server`.
 */
void kill_server(std::error_code& ec, const int64_t timeout);

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
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @note Equivalent to `adb connect <serial>`.
     */
    virtual std::string connect(std::error_code& ec, const int64_t timeout) = 0;

    /// Disconnect from the device.
    /**
     * @return A string of the disconnection status.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @note Equivalent to `adb disconnect <serial>`.
     */
    virtual std::string disconnect(std::error_code& ec,
                                   const int64_t timeout) = 0;

    /// Send an one-shot shell command to the device.
    /**
     * @param command Command to execute.
     * @return A string of the command output.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @param recv_by_socket Whether to receive the output by socket.
     * @note Equivalent to `adb -s <serial> shell <command>` without stdin.
     */
    virtual std::string shell(const std::string_view command,
                              std::error_code& ec, const int64_t timeout,
                              const bool recv_by_socket = false) = 0;

    /// Send an one-shot shell command to the device, using raw PTY.
    /**
     * @param command Command to execute.
     * @return A string of the command output, which is not mangled.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @param recv_by_socket Whether to receive the output by socket.
     * @note Equivalent to `adb -s <serial> exec-out <command>` without stdin.
     */
    virtual std::string exec(const std::string_view command,
                             std::error_code& ec, const int64_t timeout,
                             const bool recv_by_socket = false) = 0;

    /// Send a file to the device.
    /**
     * @return true if the file is successfully sent.
     * @param src Path to the source file.
     * @param dst Path to the destination file.
     * @param perm Permission of the destination file.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @note Equivalent to `adb -s <serial> push <src> <dst>`.
     */
    virtual bool push(const std::filesystem::path& src, const std::string& dst,
                      int perm, std::error_code& ec, const int64_t timeout) = 0;

    /// Set the user of adbd to root on the device.
    /**
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @note Equivalent to `adb -s <serial> root`.
     * @note The device might be offline after this command. Remember to wait
     * for the restart.
     */
    virtual std::string root(std::error_code& ec, const int64_t timeout) = 0;

    /// Set the user of adbd to non-root on the device.
    /**
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @note Equivalent to `adb -s <serial> unroot`.
     * @note The device might be offline after this command. Remember to wait
     * for the restart.
     */
    virtual std::string unroot(std::error_code& ec, const int64_t timeout) = 0;

    /// Start an interactive shell session on the device.
    /**
     * @param command Command to execute.
     * @return An io_handle for the interactive session.
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     * @note Equivalent to `adb -s <serial> shell <command>` with stdin.
     */
    virtual std::shared_ptr<io_handle>
    interactive_shell(const std::string_view command, std::error_code& ec,
                      const int64_t timeout) = 0;

    /// Start the event loop for the client.
    /**
     * @note A thread will be created to run the event loop.
     */
    virtual void start() = 0;

    /// Stop the event loop for the client.
    /**
     * @note This function will block until the thread is joined.
     */
    virtual void stop() = 0;

    /// Wait for the device to be available.
    /**
     * @param ec std::error_code to indicate what error occurred, if any.
     * @param timeout Timeout in milliseconds.
     */
    virtual void wait_for_device(std::error_code& ec,
                                 const int64_t timeout) = 0;

  protected:
    client() = default;
};

} // namespace adb

#pragma once

#include <string_view>

#include <asio/ip/tcp.hpp>

namespace adb {

/// Retrieve the version of local adb server.
/**
 * @return 4-byte string of the version number.
 * @throw std::system_error if the server is not available.
 */
std::string version();

/// Retrieve the available Android devices.
/**
 * @return A string of the list of devices.
 * @throw std::system_error if the server is not available.
 * @note Equivalent to `adb devices`.
 */
std::string devices();

/// Context for an interactive adb connection.
class io_handle {
  public:
    /// Create an handle with an existing active connection.
    /**
     * @param socket Opened adb connection.
     * @note Should be used only by the client class, after a shell request.
     * @note The socket will be closed when the handle is destroyed.
     */
    io_handle(asio::ip::tcp::socket socket);
    ~io_handle();

    /// Write data to the adb connection.
    /**
     * @param data Data to write.
     * @note Typically used to write stdin of a shell command.
     * @note The data should end with a newline.
     */
    void write(const std::string_view& data);

    /// Read data from the adb connection.
    /**
     * @return Data read.
     * @note Typically used to read stdout of a shell command.
     * @note Function will be blocked until stdout produces more data.
     */
    std::string read();

    /// Close the adb connection.
    void close();

  private:
    asio::ip::tcp::socket m_socket;
};

/// A client for the Android Debug Bridge.
class client {
  public:
    /// Create a client for a specific device.
    /**
     * @param serial serial number of the device.
     * @note If the serial is empty, the unique device will be used. If there
     * are multiple devices, an exception will be thrown.
     */
    client(const std::string_view& serial);

    /// Connect to the device.
    /**
     * @return A string of the connection status.
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb connect <serial>`.
     */
    std::string connect();

    /// Disconnect from the device.
    /**
     * @return A string of the disconnection status.
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb disconnect <serial>`.
     */
    std::string disconnect();

    /// Retrieve the version of local adb server.
    /**
     * @return 4-byte string of the version number.
     * @throw std::system_error if the server is not available.
     * @note This function reuses the class member io_context, which is
     * thread-safe for the client.
     */
    std::string version();

    /// Retrieve the available Android devices.
    /**
     * @return A string of the list of devices.
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb devices`.
     * @note This function reuses the class member io_context, which is
     * thread-safe for the client.
     */
    std::string devices();

    /// Send an one-shot shell command to the device.
    /**
     * @param command Command to execute.
     * @return A string of the command output.
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb -s <serial> shell <command>` without stdin.
     */
    std::string shell(const std::string_view& command);

    /// Send an one-shot shell command to the device, using raw PTY.
    /**
     * @param command Command to execute.
     * @return A string of the command output, which is not mangled.
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb -s <serial> exec-out <command>` without stdin.
     */
    std::string exec(const std::string_view& command);

    /// Send a file to the device.
    /**
     * @param src Path to the source file.
     * @param dst Path to the destination file.
     * @param perm Permission of the destination file.
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb -s <serial> push <src> <dst>`.
     */
    void push(const std::string_view& src, const std::string_view& dst,
              int perm);

    /// Set the user of adbd to root on the device.
    /**
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb -s <serial> root`.
     * @note The device might be offline after this command. Remember to wait
     * for the restart.
     */
    std::string root();

    /// Set the user of adbd to non-root on the device.
    /**
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb -s <serial> unroot`.
     * @note The device might be offline after this command. Remember to wait
     * for the restart.
     */
    std::string unroot();

    /// Start an interactive shell session on the device.
    /**
     * @param command Command to execute.
     * @return An io_handle for the interactive session.
     * @throw std::system_error if the server is not available.
     * @note Equivalent to `adb -s <serial> shell <command>` with stdin.
     */
    io_handle interactive_shell(const std::string_view& command);

    /// Wait for the device to be available.
    /**
     * @throw std::system_error if the server is not available.
     */
    void wait_for_device();

  private:
    std::string m_serial;

    asio::io_context m_context;
    asio::ip::basic_endpoint<asio::ip::tcp> m_endpoint;

    void check_adb_availabilty();

    /// Switch the connection to the device.
    /**
     * @param socket Opened adb connection.
     * @note Should be used only by the client class.
     * @note Local services (e.g. shell, push) can be requested after this.
     */
    void switch_to_device(asio::ip::tcp::socket& socket);
};

} // namespace adb

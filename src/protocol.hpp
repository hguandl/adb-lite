#pragma once

#include <filesystem>
#include <fstream>
#include <future>
#include <string_view>

#include <asio/ip/tcp.hpp>

namespace adb {
class io_handle_impl;
}

namespace adb::protocol {

/// Handle that manages async methods for socket transports.
class async_handle {
  public:
    /// Construct an async_handle.
    /**
     * @param context Reference to the socket I/O event loop.
     */
    async_handle(asio::io_context& context);

    /// Get the data received from the host.
    /**
     * @return Data received from the host.
     * @note Value undefined if error() evaluates to true.
     */
    std::string value() const;

    /// Get the error code of the last operation.
    /**
     * @return error_code of the last operation.
     * @note If no error occurred, the error code will be 0.
     */
    std::error_code error() const;

    /// Callback function type for async operations.
    typedef std::function<void()> callback_t;

    /// Connect to the adbd.
    /**
     * @param callback Function called when the connection is established.
     */
    void connect(const callback_t&& callback);

    /// Send an ADB host request and check its response.
    /**
     * @param request Encoded request.
     * @param callback Function called when the request is sent.
     */
    void host_request(const std::string_view request,
                      const callback_t&& callback);

    /// Receive encoded message from the host.
    /**
     * @return Message from the host.
     * @param callback Function called when the message is received.
     */
    void host_message(const callback_t&& callback);

    /// Receive all data from the host.
    /**
     * @return Data from the host.
     * @param callback Function called when the data is received.
     * @note All data will be read until EOF.
     */
    void host_data(const callback_t&& callback);

    /// Send an ADB sync request.
    /**
     * @param id 4-byte string of the request id.
     * @param length Length of the body.
     * @param body Body of the request.
     * @param callback Function called when the request is sent.
     */
    void sync_request(const std::string_view id, const uint32_t length,
                      const char* body, const callback_t&& callback);

    /// Receive the sync response.
    /**
     * @param callback Function called when the response is received.
     */
    void sync_response(const callback_t&& callback);

    /// Send the content of file with sync requests.
    /**
     * @param path Path to the file.
     * @param callback Function called when the file is sent.
     */
    void sync_send_file(const std::filesystem::path& path,
                        const callback_t&& callback);

    /// Run the wait for the tasks in the handle.
    /**
     * @param timeout Timeout in milliseconds.
     * @note This function should be called after all tasks are added, and the
     * last task should call finish().
     */
    void run(const int64_t timeout);

    /// Mark the tasks in the handle resolved.
    /**
     * @note This function should be called in the callback of the last task.
     */
    void finish();

  protected:
    /// Error code of the last operation.
    asio::error_code m_error;

    /// Data or message received from the host.
    std::string m_data;

  private:
    /// Socket I/O event loop.
    asio::io_context& m_context;

    /// Socket for the adb connection.
    asio::ip::tcp::socket m_socket;

    /// Buffer for the header.
    /**
     * @note Exclusively used in host_response().
     */
    std::array<char, 4> m_header;

    /// Buffer size for regular transportations.
    static constexpr size_t buf_size = 64000;

    /// Buffer for the data.
    /**
     * @note Used as the read buffer to get data or response.
     * @note Speicially used as the write buffer in sync_write_data().
     */
    std::unique_ptr<std::array<char, buf_size>> m_buffer;

    /// Offset of the buffer indicating how many bytes are processed.
    /**
     * @note Exclusively used in sync writing because the buffer may not be
     * fully sent within one write.
     */
    size_t m_buffer_ptr;

    /// Size of the buffer indicating how many bytes shoue be processed.
    /**
     * @note Exclusively used in sync writing because the buffer may not be
     * fully sent within one write.
     */
    size_t m_buffer_size;

    /// Promise to wait for the tasks in the handle.
    std::promise<void> m_promise;

    /// Size of the data received from the host.
    /**
     * @note Exclusively used in host_message(), whose size has been encoded in
     * the response header.
     */
    size_t m_data_size;

    /// File to be sent with SEND sync request.
    /**
     * @note Exclusively used in sync_send_file and sync_write_data.
     */
    std::unique_ptr<std::ifstream> m_file;

    /// Receive and check the response.
    /**
     * @param callback Function called when the response is received.
     * @note This function is called internally by host_request().
     */
    void host_response(const callback_t&& callback);

    /// Receive host message with given size.
    /**
     * @param callback Function called when the message is received.
     * @note This function is called internally by host_message().
     */
    void host_read_data(const callback_t&& callback);

    /// Send the content of file with DATA sync request.
    /**
     * @param callback Function called when the data is sent.
     * @note This function is called internally by sync_send_file().
     */
    void sync_write_data(const callback_t&& callback);

    /// Allow io_handle_impl to contruct from this class.
    friend class ::adb::io_handle_impl;
};

} // namespace adb::protocol

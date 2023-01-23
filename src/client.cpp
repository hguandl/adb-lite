#include "client_impl.hpp"
#include "io_handle_impl.hpp"

namespace adb {

std::string version(std::error_code& ec, const int64_t timeout) {
    standalone_handle handle;
    const auto request = "host:version";
    return handle.timed_host_request(request, true, ec, timeout);
}

std::string devices(std::error_code& ec, const int64_t timeout) {
    standalone_handle handle;
    const auto request = "host:devices";
    return handle.timed_host_request(request, true, ec, timeout);
}

void kill_server(std::error_code& ec, const int64_t timeout) {
    standalone_handle handle;
    const auto request = "host:kill";
    handle.timed_host_request(request, false, ec, timeout);
}

std::shared_ptr<client> client::create(const std::string_view serial) {
    return std::make_shared<client_impl>(serial);
}

std::string client_impl::connect(std::error_code& ec, const int64_t timeout) {
    client_handle handle(m_context);
    const auto request = "host:connect:" + m_serial;
    return handle.timed_host_request(request, true, ec, timeout);
}

std::string client_impl::disconnect(std::error_code& ec,
                                    const int64_t timeout) {
    client_handle handle(m_context);
    const auto request = "host:disconnect:" + m_serial;
    return handle.timed_host_request(request, true, ec, timeout);
}

std::string client_impl::shell(const std::string_view command,
                               std::error_code& ec, const int64_t timeout) {
    client_handle handle(m_context);
    const auto request = std::string("shell:") + command.data();
    return handle.timed_device_request(m_serial, request, ec, timeout);
}

std::string client_impl::exec(const std::string_view command,
                              std::error_code& ec, const int64_t timeout) {
    client_handle handle(m_context);
    const auto request = std::string("exec:") + command.data();
    return handle.timed_device_request(m_serial, request, ec, timeout);
}

bool client_impl::push(const std::filesystem::path& src, const std::string& dst,
                       int perm, std::error_code& ec, const int64_t timeout) {
    client_handle handle(m_context);

    static constexpr auto now_ts = [] {
        using namespace std::chrono;
        const auto now = system_clock::now().time_since_epoch();
        const auto ts = duration_cast<seconds>(now).count();
        return static_cast<uint32_t>(ts);
    };

    const auto send_req = dst + "," + std::to_string(perm);
    const auto req_size = static_cast<uint32_t>(send_req.size());

    handle.connect_device(m_serial, [&] {
        // Switch to sync mode
        const auto request = "sync:";
        handle.host_request(request, [&] {
            // SEND request: destination, permissions
            handle.sync_request("SEND", req_size, send_req.data(), [&] {
                handle.sync_send_file(src, [&] {
                    // DONE request: timestamp
                    handle.sync_request("DONE", now_ts(), nullptr, [&] {
                        handle.sync_response([&] { handle.finish(); });
                    });
                });
            });
        });
    });

    handle.run(timeout);

    ec = handle.error();
    return handle.value() == "OKAY";
}

std::string client_impl::root(std::error_code& ec, const int64_t timeout) {
    client_handle handle(m_context);
    return handle.timed_device_request(m_serial, "root:", ec, timeout);
}

std::string client_impl::unroot(std::error_code& ec, const int64_t timeout) {
    client_handle handle(m_context);
    return handle.timed_device_request(m_serial, "unroot:", ec, timeout);
}

std::shared_ptr<io_handle>
client_impl::interactive_shell(const std::string_view command,
                               std::error_code& ec, const int64_t timeout) {
    client_handle handle(m_context);

    handle.connect_device(m_serial, [=, &handle] {
        const auto request = std::string("shell:") + command.data();
        handle.host_request(request, [&handle] { handle.finish(); });
    });

    handle.run(timeout);

    ec = handle.error();
    return std::make_shared<io_handle_impl>(std::move(handle));
}

void client_impl::start() {
    m_thread = std::thread([this] {
        auto worker = asio::make_work_guard(m_context);
        m_context.restart();
        m_context.run();
    });
}

void client_impl::stop() {
    m_context.stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void client_impl::wait_for_device(std::error_code& ec, const int64_t timeout) {
    // If adbd restarts, we should wait the device to get offline first.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const auto pattern = m_serial + "\tdevice";
    auto devices_str = devices(ec, timeout);
    while (devices_str.find(pattern) == std::string::npos) {
        if (ec) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        devices_str = devices(ec, timeout);
    }
}

client_handle::client_handle(asio::io_context& context)
    : async_handle(context) {}

void client_handle::oneshot_request(const std::string_view request,
                                    const bool bounded,
                                    const callback_t&& callback) {
    host_request(request, [=, callback = std::move(callback)] {
        if (bounded) {
            host_message(std::move(callback));
        } else {
            host_data(std::move(callback));
        }
    });
}

void client_handle::connect_device(const std::string_view serial,
                                   const callback_t&& callback) {
    connect([=] {
        const auto request = "host:transport:" + std::string(serial);
        host_request(request, std::move(callback));
    });
}

std::string client_handle::timed_host_request(const std::string_view request,
                                              const bool bounded,
                                              std::error_code& ec,
                                              const int64_t timeout) {
    connect([=] { oneshot_request(request, bounded, [=] { finish(); }); });

    run(timeout);

    ec = error();
    return value();
}

std::string client_handle::timed_device_request(const std::string_view serial,
                                                const std::string_view request,
                                                std::error_code& ec,
                                                const int64_t timeout) {
    connect_device(serial,
                   [=] { oneshot_request(request, false, [=] { finish(); }); });

    run(timeout);

    ec = error();
    return value();
}

std::string
standalone_handle::timed_host_request(const std::string_view request,
                                      const bool bounded, std::error_code& ec,
                                      const int64_t timeout) {
    asio::steady_timer timer(m_context, std::chrono::milliseconds(timeout));

    m_handle.connect([&] {
        m_handle.oneshot_request(request, bounded, [&] { timer.cancel(); });
    });

    timer.async_wait([=](auto) { m_context.stop(); });

    m_context.run();

    ec = m_handle.error();
    return m_handle.value();
}

} // namespace adb

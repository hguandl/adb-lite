#include <fstream>
#include <iostream>
#include <thread>

#include <adb-lite/client.hpp>

int main() {
    std::error_code ec;
    int64_t timeout = 5000;

    std::cout << "adb host version: " << adb::version(ec, timeout) << std::endl;
    std::cout << adb::devices(ec, timeout) << std::endl;

    auto client = adb::client::create("127.0.0.1:5555");
    client->start();

    std::cout << client->connect(ec, timeout) << std::endl;

    std::cout << client->root(ec, timeout) << std::endl;
    client->wait_for_device(ec, timeout);

    auto handle = client->interactive_shell("tee", ec, timeout);
    handle->write("Hello, world!");
    std::cout << handle->read(1000) << std::endl;
    std::cout << handle->read(1000) << std::endl;

    std::string screencap = client->exec("screencap -p", ec, timeout);
    std::ofstream file("screenshot.png", std::ios::binary);
    file << screencap;

    client->push("screenshot.png", "/data/local/tmp/screenshot.png", 0644, ec,
                 timeout);

    std::cout << client->shell("ls -l /data/local/tmp", ec, timeout)
              << std::endl;

    // Test minitouch
    // client->push("minitouch", "/data/local/tmp/minitouch", 0700);

    // auto minitouch = client->interactive_shell(
    //     "/data/local/tmp/minitouch -d /dev/input/event1 -i");
    // std::cout << minitouch->read(3) << std::endl;

    // minitouch->write("d 0 14000 25000 1024\n");
    // minitouch->write("c\n");
    // minitouch->write("u 0\n");
    // minitouch->write("c\n");

    std::cout << client->disconnect(ec, timeout) << std::endl;
    return 0;
}

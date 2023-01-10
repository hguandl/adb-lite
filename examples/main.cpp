#include <fstream>
#include <iostream>

#include <adb-lite/client.hpp>

int main() {
    std::cout << "adb host version: " << adb::version() << std::endl;
    std::cout << adb::devices() << std::endl;

    auto client = adb::client::create("127.0.0.1:5555");

    std::cout << client->connect() << std::endl;

    std::cout << client->root() << std::endl;
    client->wait_for_device();

    auto handle = client->interactive_shell("tee");
    handle->write("Hello, world!");
    std::cout << handle->read() << std::endl;
    std::cout << handle->read(1) << std::endl;

    std::string screencap = client->exec("screencap -p");
    std::ofstream file("screenshot.png", std::ios::binary);
    file << screencap;

    client->push("screenshot.png", "/data/local/tmp/screenshot.png", 0700);
    std::cout << client->shell("ls -l /data/local/tmp") << std::endl;

    // Test minitouch
    // client->push("minitouch", "/data/local/tmp/minitouch", 0700);

    // auto minitouch = client->interactive_shell(
    //     "/data/local/tmp/minitouch -d /dev/input/event1 -i");
    // std::cout << minitouch->read(3) << std::endl;

    // minitouch->write("d 0 14000 25000 1024\n");
    // minitouch->write("c\n");
    // minitouch->write("u 0\n");
    // minitouch->write("c\n");

    std::cout << client->disconnect() << std::endl;
    return 0;
}

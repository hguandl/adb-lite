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

    auto handle = client->interactive_shell("ls -l /data/local/tmp");

    std::string screencap = client->exec("screencap -p");
    std::ofstream file("screenshot.png", std::ios::binary);
    file << screencap;
    file.close();

    // Test minitouch
    // client.push("minitouch", "/data/local/tmp/minitouch", 0700);

    // auto minitouch = client.interactive_shell(
    //     "/data/local/tmp/minitouch -d /dev/input/event1 -i");
    // std::cout << minitouch.read() << std::endl;

    // minitouch.write("d 0 14000 25000 1024\n");
    // minitouch.write("c\n");
    // minitouch.write("u 0\n");
    // minitouch.write("c\n");

    std::cout << client->disconnect() << std::endl;
    return 0;
}

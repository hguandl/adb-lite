#include <fstream>
#include <iostream>

#include <adb-lite/client.hpp>

int main() {
    std::cout << "adb host version: " << adb::version().value_or("error")
              << std::endl;
    std::cout << adb::devices().value_or("devices error") << std::endl;

    auto client = adb::client::create("127.0.0.1:5555");

    std::cout << client->connect().value_or("connect error") << std::endl;

    std::cout << client->root().value_or("root error") << std::endl;
    client->wait_for_device();

    auto screencap = client->exec("screencap -p");
    std::ofstream file("screenshot.png", std::ios::binary);
    file << screencap.value_or("");
    file.close();

    // Test minitouch
    // client->push("minitouch", "/data/local/tmp/minitouch", 0700);

    // auto minitouch = client->interactive_shell(
    //     "/data/local/tmp/minitouch -d /dev/input/event1 -i");
    // std::cout << minitouch.value()->read().value_or("error") << std::endl;

    // minitouch.value()->write("d 0 14000 25000 1024\n");
    // minitouch.value()->write("c\n");
    // minitouch.value()->write("u 0\n");
    // minitouch.value()->write("c\n");

    std::cout << client->disconnect().value_or("disconnect error") << std::endl;
    return 0;
}

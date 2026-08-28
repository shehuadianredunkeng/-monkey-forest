#include <exception>
#include <iostream>

int runRoomTests();

int main() {
    try {
        return runRoomTests();
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << '\n';
        return 1;
    }
}

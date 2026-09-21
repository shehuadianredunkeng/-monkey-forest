#include <exception>
#include <iostream>

using namespace std;

int runRoomTests();

int main() {
    try {
        return runRoomTests();
    } catch (const exception& error) {
        cerr << "Test failed: " << error.what() << '\n';
        return 1;
    }
}

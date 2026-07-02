#include "test_framework.hpp"

int main() {
    int failed = 0;
    for (const auto& test : tests::registry()) {
        try {
            test.func();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }
    std::cout << "Executed " << tests::registry().size() << " tests\n";
    return failed == 0 ? 0 : 1;
}

#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

#define TEST_CASE(name) \
    void name(); \
    static Registrar registrar_##name(#name, name); \
    void name()

TEST_CASE(smoke_test_build_wiring_works) {
    assert(1 + 1 == 2);
}

} // namespace

int main() {
    int failures = 0;

    for (auto& t : registry()) {
        std::cout << "[ RUN ] " << t.name << std::endl;
        t.fn();
        std::cout << "[ OK  ] " << t.name << std::endl;
    }

    if (failures == 0) {
        std::cout << registry().size() << " test(s) passed." << std::endl;
    }

    return failures;
}

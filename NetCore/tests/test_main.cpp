#include "test_harness.hpp"

int main() {
    return netcore::testing::TestRegistry::getInstance().runAll();
}

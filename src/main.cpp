#include <iostream>
#include "engine/uci/uci.hpp"
#include "engine/config.hpp"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    engine::Uci uci;
    uci.loop();
    return 0;
}

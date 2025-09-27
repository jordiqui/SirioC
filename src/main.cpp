#include <iostream>
#include "engine/uci/uci.hpp"
#include "engine/config.hpp"

namespace engine {
class Engine {
public:
    Engine() = default;
    void run() {
        Uci uci(*this);
        uci.loop();
    }
    // UCI will call these:
    void on_newgame() {}
};
} // namespace engine

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout << "id name " << engine::kEngineName << " " << engine::kEngineVersion << "\n";
    std::cout << "id author Jorge Ruiz créditos Codex OpenAi\n";
    engine::Engine e;
    e.run();
    return 0;
}

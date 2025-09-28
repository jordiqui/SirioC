#pragma once
#include <string>

namespace engine {

class Uci {
public:
    Uci() = default;
    void loop();

private:
    void handle_line(const std::string& line);
    void cmd_uci();
    void cmd_isready();
    void cmd_ucinewgame();
    void cmd_setoption(const std::string& s);
    void cmd_position(const std::string& s);
    void cmd_go(const std::string& s);
    void cmd_stop();
    void cmd_perft(const std::string& s);
    void cmd_bench(const std::string& s);
};

} // namespace engine

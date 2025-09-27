#pragma once
#include <string>

namespace engine {

class Engine; // forward

class Uci {
public:
    explicit Uci(Engine& engine) : engine_(engine) {}
    void loop();

private:
    Engine& engine_;
    void handle_line(const std::string& line);
    void cmd_uci();
    void cmd_isready();
    void cmd_ucinewgame();
    void cmd_setoption(const std::string& s);
    void cmd_position(const std::string& s);
    void cmd_go(const std::string& s);
    void cmd_stop();
};

} // namespace engine

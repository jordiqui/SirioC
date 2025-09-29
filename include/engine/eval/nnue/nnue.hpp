#pragma once

#include <string>

namespace engine {
class Board;
}

namespace engine::nnue_runtime {

bool is_enabled();
bool is_loaded();
void request_reload();
void try_reload_if_requested();
bool load_from_file(const std::string& path, std::string* outInfo = nullptr);
int  evaluate(const Board& pos);
void on_new_game();
void on_thread_start(int tid);
void on_thread_stop(int tid);

} // namespace engine::nnue_runtime

namespace NNUE = engine::nnue_runtime;


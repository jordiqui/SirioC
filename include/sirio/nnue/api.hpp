#pragma once

#include <optional>
#include <string>

namespace sirio::nnue {

struct NetworkInfo {
    std::string path;
    std::size_t bytes = 0;
    std::string dims;
};

// Initialize the NNUE backend from the file located at `path`.
// Returns true on success and installs the loaded backend as the active evaluation.
// On failure, returns false and writes the error message into `error_message` if provided.
bool init(const std::string &path, std::string *error_message = nullptr);

// Reset the evaluation backend to the classical handcrafted evaluation.
void unload();

// Returns true if a NNUE network is currently active.
bool is_loaded();

// Returns metadata about the active network if any.
std::optional<NetworkInfo> info();

}  // namespace sirio::nnue

